/* gameplaySP
 *
 * Copyright (C) 2006 Exophase <exophase@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

// Important todo:
// - stm reglist writeback when base is in the list needs adjustment
// - block memory needs psr swapping and user mode reg swapping

#include "common.h"

u32 memory_region_access_read_u8[16];
u32 memory_region_access_read_s8[16];
u32 memory_region_access_read_u16[16];
u32 memory_region_access_read_s16[16];
u32 memory_region_access_read_u32[16];
u32 memory_region_access_write_u8[16];
u32 memory_region_access_write_u16[16];
u32 memory_region_access_write_u32[16];
u32 memory_reads_u8;
u32 memory_reads_s8;
u32 memory_reads_u16;
u32 memory_reads_s16;
u32 memory_reads_u32;
u32 memory_writes_u8;
u32 memory_writes_u16;
u32 memory_writes_u32;

const u8 bit_count[256] =
{
  0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4, 1, 2, 2, 3, 2, 3, 3,
  4, 2, 3, 3, 4, 3, 4, 4, 5, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4,
  4, 5, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 1, 2, 2, 3, 2,
  3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5,
  4, 5, 5, 6, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4,
  5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3,
  3, 4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 2,
  3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6,
  4, 5, 5, 6, 5, 6, 6, 7, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5,
  6, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 3, 4, 4, 5, 4, 5,
  5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6,
  7, 7, 8
};


#ifdef REGISTER_USAGE_ANALYZE

u64 instructions_total = 0;

u64 arm_reg_freq[16];
u64 arm_reg_access_total = 0;
u64 arm_instructions_total = 0;

u64 thumb_reg_freq[16];
u64 thumb_reg_access_total = 0;
u64 thumb_instructions_total = 0;

// mla/long mla's addition operand are not counted yet.

#define using_register(instruction_set, register, type)                       \
  instruction_set##_reg_freq[register]++;                                     \
  instruction_set##_reg_access_total++                                        \

#define using_register_list(instruction_set, rlist, count)                    \
{                                                                             \
  u32 i;                                                                      \
  for(i = 0; i < count; i++)                                                  \
  {                                                                           \
    if((reg_list >> i) & 0x01)                                                \
    {                                                                         \
      using_register(instruction_set, i, memory_target);                      \
    }                                                                         \
  }                                                                           \
}                                                                             \

#define using_instruction(instruction_set)                                    \
  instruction_set##_instructions_total++;                                     \
  instructions_total++                                                        \

int sort_tagged_element(const void *_a, const void *_b)
{
  const u64 *a = _a;
  const u64 *b = _b;

  return (int)(b[1] - a[1]);
}

void print_register_usage(void)
{
  u32 i;
  u64 arm_reg_freq_tagged[32];
  u64 thumb_reg_freq_tagged[32];
  double percent;
  double percent_total = 0.0;

  for(i = 0; i < 16; i++)
  {
    arm_reg_freq_tagged[i * 2] = i;
    arm_reg_freq_tagged[(i * 2) + 1] = arm_reg_freq[i];
    thumb_reg_freq_tagged[i * 2] = i;
    thumb_reg_freq_tagged[(i * 2) + 1] = thumb_reg_freq[i];
  }

  qsort(arm_reg_freq_tagged, 16, sizeof(u64) * 2, sort_tagged_element);
  qsort(thumb_reg_freq_tagged, 16, sizeof(u64) * 2, sort_tagged_element);

  printf("ARM register usage (%lf%% ARM instructions):\n",
   (arm_instructions_total * 100.0) / instructions_total);
  for(i = 0; i < 16; i++)
  {
    percent = (arm_reg_freq_tagged[(i * 2) + 1] * 100.0) /
     arm_reg_access_total;
    percent_total += percent;
    printf("r%02d: %lf%% (-- %lf%%)\n",
     (u32)arm_reg_freq_tagged[(i * 2)], percent, percent_total);
  }

  percent_total = 0.0;

  printf("\nThumb register usage (%lf%% Thumb instructions):\n",
   (thumb_instructions_total * 100.0) / instructions_total);
  for(i = 0; i < 16; i++)
  {
    percent = (thumb_reg_freq_tagged[(i * 2) + 1] * 100.0) /
     thumb_reg_access_total;
    percent_total += percent;
    printf("r%02d: %lf%% (-- %lf%%)\n",
     (u32)thumb_reg_freq_tagged[(i * 2)], percent, percent_total);
  }

  memset(arm_reg_freq, 0, sizeof(u64) * 16);
  memset(thumb_reg_freq, 0, sizeof(u64) * 16);
  arm_reg_access_total = 0;
  thumb_reg_access_total = 0;
}

#else

#define using_register(instruction_set, register, type)                       \

#define using_register_list(instruction_set, rlist, count)                    \

#define using_instruction(instruction_set)                                    \

#endif


#define arm_decode_data_proc_reg(opcode)                                      \
  u32 rn = (opcode >> 16) & 0x0F;                                             \
  u32 rd = (opcode >> 12) & 0x0F;                                             \
  u32 rm = opcode & 0x0F;                                                     \
  (void)rd;                                                                   \
  (void)rn;                                                                   \
  using_register(arm, rd, op_dest);                                           \
  using_register(arm, rn, op_src);                                            \
  using_register(arm, rm, op_src)                                             \

#define arm_decode_data_proc_imm(opcode)                                      \
  u32 imm;                                                                    \
  u32 rn = (opcode >> 16) & 0x0F;                                             \
  u32 rd = (opcode >> 12) & 0x0F;                                             \
  (void)rd;                                                                   \
  (void)rn;                                                                   \
  ror(imm, opcode & 0xFF, ((opcode >> 8) & 0x0F) * 2);                        \
  using_register(arm, rd, op_dest);                                           \
  using_register(arm, rn, op_src)                                             \

#define arm_decode_psr_reg(opcode)                                            \
  u32 psr_field = (opcode >> 16) & 0x0F;                                      \
  u32 rd = (opcode >> 12) & 0x0F;                                             \
  u32 rm = opcode & 0x0F;                                                     \
  (void)rd;                                                                   \
  (void)rm;                                                                   \
  (void)psr_field;                                                            \
  using_register(arm, rd, op_dest);                                           \
  using_register(arm, rm, op_src)                                             \

#define arm_decode_psr_imm(opcode)                                            \
  u32 imm;                                                                    \
  u32 psr_field = (opcode >> 16) & 0x0F;                                      \
  u32 rd = (opcode >> 12) & 0x0F;                                             \
  (void)rd;                                                                   \
  ror(imm, opcode & 0xFF, ((opcode >> 8) & 0x0F) * 2);                        \
  using_register(arm, rd, op_dest)                                            \

#define arm_decode_branchx(opcode)                                            \
  u32 rn = opcode & 0x0F;                                                     \
  using_register(arm, rn, branch_target)                                      \

#define arm_decode_multiply()                                                 \
  u32 rd = (opcode >> 16) & 0x0F;                                             \
  u32 rn = (opcode >> 12) & 0x0F;                                             \
  u32 rs = (opcode >> 8) & 0x0F;                                              \
  u32 rm = opcode & 0x0F;                                                     \
  (void)rn;                                                                   \
  using_register(arm, rd, op_dest);                                           \
  using_register(arm, rn, op_src);                                            \
  using_register(arm, rm, op_src)                                             \

#define arm_decode_multiply_long()                                            \
  u32 rdhi = (opcode >> 16) & 0x0F;                                           \
  u32 rdlo = (opcode >> 12) & 0x0F;                                           \
  u32 rn = (opcode >> 8) & 0x0F;                                              \
  u32 rm = opcode & 0x0F;                                                     \
  using_register(arm, rdhi, op_dest);                                         \
  using_register(arm, rdlo, op_dest);                                         \
  using_register(arm, rn, op_src);                                            \
  using_register(arm, rm, op_src)                                             \

#define arm_decode_swap()                                                     \
  u32 rn = (opcode >> 16) & 0x0F;                                             \
  u32 rd = (opcode >> 12) & 0x0F;                                             \
  u32 rm = opcode & 0x0F;                                                     \
  using_register(arm, rd, memory_target);                                     \
  using_register(arm, rn, memory_base);                                       \
  using_register(arm, rm, memory_target)                                      \

#define arm_decode_half_trans_r()                                             \
  u32 rn = (opcode >> 16) & 0x0F;                                             \
  u32 rd = (opcode >> 12) & 0x0F;                                             \
  u32 rm = opcode & 0x0F;                                                     \
  using_register(arm, rd, memory_target);                                     \
  using_register(arm, rn, memory_base);                                       \
  using_register(arm, rm, memory_offset)                                      \

#define arm_decode_half_trans_of()                                            \
  u32 rn = (opcode >> 16) & 0x0F;                                             \
  u32 rd = (opcode >> 12) & 0x0F;                                             \
  u32 offset = ((opcode >> 4) & 0xF0) | (opcode & 0x0F);                      \
  using_register(arm, rd, memory_target);                                     \
  using_register(arm, rn, memory_base)                                        \

#define arm_decode_data_trans_imm()                                           \
  u32 rn = (opcode >> 16) & 0x0F;                                             \
  u32 rd = (opcode >> 12) & 0x0F;                                             \
  u32 offset = opcode & 0x0FFF;                                               \
  using_register(arm, rd, memory_target);                                     \
  using_register(arm, rn, memory_base)                                        \

#define arm_decode_data_trans_reg()                                           \
  u32 rn = (opcode >> 16) & 0x0F;                                             \
  u32 rd = (opcode >> 12) & 0x0F;                                             \
  u32 rm = opcode & 0x0F;                                                     \
  using_register(arm, rd, memory_target);                                     \
  using_register(arm, rn, memory_base);                                       \
  using_register(arm, rm, memory_offset)                                      \

#define arm_decode_block_trans()                                              \
  u32 rn = (opcode >> 16) & 0x0F;                                             \
  u32 reg_list = opcode & 0xFFFF;                                             \
  using_register(arm, rn, memory_base);                                       \
  using_register_list(arm, reg_list, 16)                                      \

#define arm_decode_branch()                                                   \
  s32 offset = ((s32)((u32)(opcode << 8))) >> 6                               \


#define thumb_decode_shift()                                                  \
  u32 imm = (opcode >> 6) & 0x1F;                                             \
  u32 rs = (opcode >> 3) & 0x07;                                              \
  u32 rd = opcode & 0x07;                                                     \
  using_register(thumb, rd, op_dest);                                         \
  using_register(thumb, rs, op_shift)                                         \

#define thumb_decode_add_sub()                                                \
  u32 rn = (opcode >> 6) & 0x07;                                              \
  u32 rs = (opcode >> 3) & 0x07;                                              \
  u32 rd = opcode & 0x07;                                                     \
  using_register(thumb, rd, op_dest);                                         \
  using_register(thumb, rn, op_src);                                          \
  using_register(thumb, rn, op_src)                                           \

#define thumb_decode_add_sub_imm()                                            \
  u32 imm = (opcode >> 6) & 0x07;                                             \
  u32 rs = (opcode >> 3) & 0x07;                                              \
  u32 rd = opcode & 0x07;                                                     \
  using_register(thumb, rd, op_src_dest);                                     \
  using_register(thumb, rs, op_src)                                           \

#define thumb_decode_imm()                                                    \
  u32 imm = opcode & 0xFF;                                                    \
  using_register(thumb, ((opcode >> 8) & 0x07), op_dest)                      \

#define thumb_decode_alu_op()                                                 \
  u32 rs = (opcode >> 3) & 0x07;                                              \
  u32 rd = opcode & 0x07;                                                     \
  using_register(thumb, rd, op_src_dest);                                     \
  using_register(thumb, rs, op_src)                                           \

#define thumb_decode_hireg_op()                                               \
  u32 rs = (opcode >> 3) & 0x0F;                                              \
  u32 rd = ((opcode >> 4) & 0x08) | (opcode & 0x07);                          \
  (void)rd;                                                                   \
  using_register(thumb, rd, op_src_dest);                                     \
  using_register(thumb, rs, op_src)                                           \


#define thumb_decode_mem_reg()                                                \
  u32 ro = (opcode >> 6) & 0x07;                                              \
  u32 rb = (opcode >> 3) & 0x07;                                              \
  u32 rd = opcode & 0x07;                                                     \
  using_register(thumb, rd, memory_target);                                   \
  using_register(thumb, rb, memory_base);                                     \
  using_register(thumb, ro, memory_offset)                                    \


#define thumb_decode_mem_imm()                                                \
  u32 imm = (opcode >> 6) & 0x1F;                                             \
  u32 rb = (opcode >> 3) & 0x07;                                              \
  u32 rd = opcode & 0x07;                                                     \
  using_register(thumb, rd, memory_target);                                   \
  using_register(thumb, rb, memory_base)                                      \


#define thumb_decode_add_sp()                                                 \
  u32 imm = opcode & 0x7F;                                                    \
  using_register(thumb, REG_SP, op_dest)                                      \

#define thumb_decode_rlist()                                                  \
  u32 reg_list = opcode & 0xFF;                                               \
  using_register_list(thumb, rlist, 8)                                        \

#define thumb_decode_branch_cond()                                            \
  s32 offset = (s8)(opcode & 0xFF)                                            \

#define thumb_decode_swi()                                                    \
  u32 comment = opcode & 0xFF                                                 \

#define thumb_decode_branch()                                                 \
  u32 offset = opcode & 0x07FF                                                \


#define get_shift_register(dest)                                              \
  u32 shift = reg[(opcode >> 8) & 0x0F];                                      \
  using_register(arm, ((opcode >> 8) & 0x0F), op_shift);                      \
  dest = reg[rm];                                                             \
  if(rm == 15)                                                                \
    dest += 4                                                                 \


#define calculate_z_flag(dest)                                                \
  z_flag = (dest == 0)                                                        \

#define calculate_n_flag(dest)                                                \
  n_flag = ((signed)dest < 0)                                                 \

#define calculate_c_flag_sub(dest, src_a, src_b)                              \
  c_flag = ((unsigned)src_b <= (unsigned)src_a)                               \

#define calculate_v_flag_sub(dest, src_a, src_b)                              \
  v_flag = ((signed)src_b > (signed)src_a) != ((signed)dest < 0)              \

#define calculate_c_flag_add(dest, src_a, src_b)                              \
  c_flag = ((unsigned)dest < (unsigned)src_a)                                 \

#define calculate_v_flag_add(dest, src_a, src_b)                              \
  v_flag = ((signed)dest < (signed)src_a) != ((signed)src_b < 0)              \


#define calculate_reg_sh()                                                    \
  u32 reg_sh = 0;                                                             \
  switch((opcode >> 4) & 0x07)                                                \
  {                                                                           \
    /* LSL imm */                                                             \
    case 0x0:                                                                 \
    {                                                                         \
      reg_sh = reg[rm] << ((opcode >> 7) & 0x1F);                             \
      break;                                                                  \
    }                                                                         \
                                                                              \
    /* LSL reg */                                                             \
    case 0x1:                                                                 \
    {                                                                         \
      get_shift_register(reg_sh);                                             \
      if(shift <= 31)                                                         \
        reg_sh = reg_sh << shift;                                             \
      else                                                                    \
        reg_sh = 0;                                                           \
      break;                                                                  \
    }                                                                         \
                                                                              \
    /* LSR imm */                                                             \
    case 0x2:                                                                 \
    {                                                                         \
      u32 imm = (opcode >> 7) & 0x1F;                                         \
      if(imm == 0)                                                            \
        reg_sh = 0;                                                           \
      else                                                                    \
        reg_sh = reg[rm] >> imm;                                              \
      break;                                                                  \
    }                                                                         \
                                                                              \
    /* LSR reg */                                                             \
    case 0x3:                                                                 \
    {                                                                         \
      get_shift_register(reg_sh);                                             \
      if(shift <= 31)                                                         \
        reg_sh = reg_sh >> shift;                                             \
      else                                                                    \
        reg_sh = 0;                                                           \
      break;                                                                  \
    }                                                                         \
                                                                              \
    /* ASR imm */                                                             \
    case 0x4:                                                                 \
    {                                                                         \
      u32 imm = (opcode >> 7) & 0x1F;                                         \
      reg_sh = reg[rm];                                                       \
                                                                              \
      if(imm == 0)                                                            \
        reg_sh = (s32)reg_sh >> 31;                                           \
      else                                                                    \
        reg_sh = (s32)reg_sh >> imm;                                          \
      break;                                                                  \
    }                                                                         \
                                                                              \
    /* ASR reg */                                                             \
    case 0x5:                                                                 \
    {                                                                         \
      get_shift_register(reg_sh);                                             \
      if(shift <= 31)                                                         \
        reg_sh = (s32)reg_sh >> shift;                                        \
      else                                                                    \
        reg_sh = (s32)reg_sh >> 31;                                           \
      break;                                                                  \
    }                                                                         \
                                                                              \
    /* ROR imm */                                                             \
    case 0x6:                                                                 \
    {                                                                         \
      u32 imm = (opcode >> 7) & 0x1F;                                         \
                                                                              \
      if(imm == 0)                                                            \
        reg_sh = (reg[rm] >> 1) | (c_flag << 31);                             \
      else                                                                    \
        ror(reg_sh, reg[rm], imm);                                            \
      break;                                                                  \
    }                                                                         \
                                                                              \
    /* ROR reg */                                                             \
    case 0x7:                                                                 \
    {                                                                         \
      get_shift_register(reg_sh);                                             \
      ror(reg_sh, reg_sh, shift);                                             \
      break;                                                                  \
    }                                                                         \
  }                                                                           \

#define calculate_reg_sh_flags()                                              \
  u32 reg_sh = 0;                                                             \
  switch((opcode >> 4) & 0x07)                                                \
  {                                                                           \
    /* LSL imm */                                                             \
    case 0x0:                                                                 \
    {                                                                         \
      u32 imm = (opcode >> 7) & 0x1F;                                         \
      reg_sh = reg[rm];                                                       \
                                                                              \
      if(imm != 0)                                                            \
      {                                                                       \
        c_flag = (reg_sh >> (32 - imm)) & 0x01;                               \
        reg_sh <<= imm;                                                       \
      }                                                                       \
                                                                              \
      break;                                                                  \
    }                                                                         \
                                                                              \
    /* LSL reg */                                                             \
    case 0x1:                                                                 \
    {                                                                         \
      get_shift_register(reg_sh);                                             \
      if(shift != 0)                                                          \
      {                                                                       \
        if(shift > 31)                                                        \
        {                                                                     \
          if(shift == 32)                                                     \
            c_flag = reg_sh & 0x01;                                           \
          else                                                                \
            c_flag = 0;                                                       \
          reg_sh = 0;                                                         \
        }                                                                     \
        else                                                                  \
        {                                                                     \
          c_flag = (reg_sh >> (32 - shift)) & 0x01;                           \
          reg_sh <<= shift;                                                   \
        }                                                                     \
      }                                                                       \
      break;                                                                  \
    }                                                                         \
                                                                              \
    /* LSR imm */                                                             \
    case 0x2:                                                                 \
    {                                                                         \
      u32 imm = (opcode >> 7) & 0x1F;                                         \
      reg_sh = reg[rm];                                                       \
      if(imm == 0)                                                            \
      {                                                                       \
        c_flag = reg_sh >> 31;                                                \
        reg_sh = 0;                                                           \
      }                                                                       \
      else                                                                    \
      {                                                                       \
        c_flag = (reg_sh >> (imm - 1)) & 0x01;                                \
        reg_sh >>= imm;                                                       \
      }                                                                       \
      break;                                                                  \
    }                                                                         \
                                                                              \
    /* LSR reg */                                                             \
    case 0x3:                                                                 \
    {                                                                         \
      get_shift_register(reg_sh);                                             \
      if(shift != 0)                                                          \
      {                                                                       \
        if(shift > 31)                                                        \
        {                                                                     \
          if(shift == 32)                                                     \
            c_flag = (reg_sh >> 31) & 0x01;                                   \
          else                                                                \
            c_flag = 0;                                                       \
          reg_sh = 0;                                                         \
        }                                                                     \
        else                                                                  \
        {                                                                     \
          c_flag = (reg_sh >> (shift - 1)) & 0x01;                            \
          reg_sh >>= shift;                                                   \
        }                                                                     \
      }                                                                       \
      break;                                                                  \
    }                                                                         \
                                                                              \
    /* ASR imm */                                                             \
    case 0x4:                                                                 \
    {                                                                         \
      u32 imm = (opcode >> 7) & 0x1F;                                         \
      reg_sh = reg[rm];                                                       \
      if(imm == 0)                                                            \
      {                                                                       \
        reg_sh = (s32)reg_sh >> 31;                                           \
        c_flag = reg_sh & 0x01;                                               \
      }                                                                       \
      else                                                                    \
      {                                                                       \
        c_flag = (reg_sh >> (imm - 1)) & 0x01;                                \
        reg_sh = (s32)reg_sh >> imm;                                          \
      }                                                                       \
      break;                                                                  \
    }                                                                         \
                                                                              \
    /* ASR reg */                                                             \
    case 0x5:                                                                 \
    {                                                                         \
      get_shift_register(reg_sh);                                             \
      if(shift != 0)                                                          \
      {                                                                       \
        if(shift > 31)                                                        \
        {                                                                     \
          reg_sh = (s32)reg_sh >> 31;                                         \
          c_flag = reg_sh & 0x01;                                             \
        }                                                                     \
        else                                                                  \
        {                                                                     \
          c_flag = (reg_sh >> (shift - 1)) & 0x01;                            \
          reg_sh = (s32)reg_sh >> shift;                                      \
        }                                                                     \
      }                                                                       \
      break;                                                                  \
    }                                                                         \
                                                                              \
    /* ROR imm */                                                             \
    case 0x6:                                                                 \
    {                                                                         \
      u32 imm = (opcode >> 7) & 0x1F;                                         \
      reg_sh = reg[rm];                                                       \
      if(imm == 0)                                                            \
      {                                                                       \
        u32 old_c_flag = c_flag;                                              \
        c_flag = reg_sh & 0x01;                                               \
        reg_sh = (reg_sh >> 1) | (old_c_flag << 31);                          \
      }                                                                       \
      else                                                                    \
      {                                                                       \
        c_flag = (reg_sh >> (imm - 1)) & 0x01;                                \
        ror(reg_sh, reg_sh, imm);                                             \
      }                                                                       \
      break;                                                                  \
    }                                                                         \
                                                                              \
    /* ROR reg */                                                             \
    case 0x7:                                                                 \
    {                                                                         \
      get_shift_register(reg_sh);                                             \
      if(shift != 0)                                                          \
      {                                                                       \
        c_flag = (reg_sh >> (shift - 1)) & 0x01;                              \
        ror(reg_sh, reg_sh, shift);                                           \
      }                                                                       \
      break;                                                                  \
    }                                                                         \
  }                                                                           \

#define calculate_reg_offset()                                                \
  u32 reg_offset = 0;                                                         \
  switch((opcode >> 5) & 0x03)                                                \
  {                                                                           \
    /* LSL imm */                                                             \
    case 0x0:                                                                 \
    {                                                                         \
      reg_offset = reg[rm] << ((opcode >> 7) & 0x1F);                         \
      break;                                                                  \
    }                                                                         \
                                                                              \
    /* LSR imm */                                                             \
    case 0x1:                                                                 \
    {                                                                         \
      u32 imm = (opcode >> 7) & 0x1F;                                         \
      if(imm == 0)                                                            \
        reg_offset = 0;                                                       \
      else                                                                    \
        reg_offset = reg[rm] >> imm;                                          \
      break;                                                                  \
    }                                                                         \
                                                                              \
    /* ASR imm */                                                             \
    case 0x2:                                                                 \
    {                                                                         \
      u32 imm = (opcode >> 7) & 0x1F;                                         \
      if(imm == 0)                                                            \
        reg_offset = (s32)reg[rm] >> 31;                                      \
      else                                                                    \
        reg_offset = (s32)reg[rm] >> imm;                                     \
      break;                                                                  \
    }                                                                         \
                                                                              \
    /* ROR imm */                                                             \
    case 0x3:                                                                 \
    {                                                                         \
      u32 imm = (opcode >> 7) & 0x1F;                                         \
      if(imm == 0)                                                            \
        reg_offset = (reg[rm] >> 1) | (c_flag << 31);                         \
      else                                                                    \
        ror(reg_offset, reg[rm], imm);                                        \
      break;                                                                  \
    }                                                                         \
  }                                                                           \

#define calculate_flags_add(dest, src_a, src_b)                               \
  calculate_z_flag(dest);                                                     \
  calculate_n_flag(dest);                                                     \
  calculate_c_flag_add(dest, src_a, src_b);                                   \
  calculate_v_flag_add(dest, src_a, src_b)                                    \

#define calculate_flags_sub(dest, src_a, src_b)                               \
  calculate_z_flag(dest);                                                     \
  calculate_n_flag(dest);                                                     \
  calculate_c_flag_sub(dest, src_a, src_b);                                   \
  calculate_v_flag_sub(dest, src_a, src_b)                                    \

#define calculate_flags_logic(dest)                                           \
  calculate_z_flag(dest);                                                     \
  calculate_n_flag(dest)                                                      \

#define extract_flags()                                                       \
  n_flag = reg[REG_CPSR] >> 31;                                               \
  z_flag = (reg[REG_CPSR] >> 30) & 0x01;                                      \
  c_flag = (reg[REG_CPSR] >> 29) & 0x01;                                      \
  v_flag = (reg[REG_CPSR] >> 28) & 0x01;                                      \

#define collapse_flags()                                                      \
  reg[REG_CPSR] = (n_flag << 31) | (z_flag << 30) | (c_flag << 29) |          \
   (v_flag << 28) | (reg[REG_CPSR] & 0xFF)                                    \

#define check_pc_region()                                                     \
  new_pc_region = (pc >> 15);                                                 \
  if(new_pc_region != pc_region)                                              \
  {                                                                           \
    pc_region = new_pc_region;                                                \
    pc_address_block = memory_map_read[new_pc_region];                        \
                                                                              \
    if(!pc_address_block)                                                     \
      pc_address_block = load_gamepak_page(pc_region & 0x3FF);                \
  }                                                                           \

u32 branch_targets = 0;
u32 high_frequency_branch_targets = 0;

#define BRANCH_ACTIVITY_THRESHOLD 50

#define arm_update_pc()                                                       \
  pc = reg[REG_PC]                                                            \

#define arm_pc_offset(val)                                                    \
  pc += val;                                                                  \
  reg[REG_PC] = pc                                                            \

#define arm_pc_offset_update(val)                                             \
  pc += val;                                                                  \
  reg[REG_PC] = pc                                                            \

#define arm_pc_offset_update_direct(val)                                      \
  pc = val;                                                                   \
  reg[REG_PC] = pc                                                            \


// It should be okay to still generate result flags, spsr will overwrite them.
// This is pretty infrequent (returning from interrupt handlers, et al) so
// probably not worth optimizing for.

#define check_for_interrupts()                                                \
  if((read_ioreg(REG_IE) & read_ioreg(REG_IF)) &&                             \
   read_ioreg(REG_IME) && ((reg[REG_CPSR] & 0x80) == 0))                      \
  {                                                                           \
    reg_mode[MODE_IRQ][6] = reg[REG_PC] + 4;                                  \
    spsr[MODE_IRQ] = reg[REG_CPSR];                                           \
    reg[REG_CPSR] = 0xD2;                                                     \
    reg[REG_PC] = 0x00000018;                                                 \
    arm_update_pc();                                                          \
    set_cpu_mode(MODE_IRQ);                                                   \
    goto arm_loop;                                                            \
  }                                                                           \

#define arm_spsr_restore()                                                    \
  if(rd == 15)                                                                \
  {                                                                           \
    if(reg[CPU_MODE] != MODE_USER)                                            \
    {                                                                         \
      reg[REG_CPSR] = spsr[reg[CPU_MODE]];                                    \
      extract_flags();                                                        \
      set_cpu_mode(cpu_modes[reg[REG_CPSR] & 0x1F]);                          \
      check_for_interrupts();                                                 \
    }                                                                         \
    arm_update_pc();                                                          \
                                                                              \
    if(reg[REG_CPSR] & 0x20)                                                  \
      goto thumb_loop;                                                        \
  }                                                                           \

#define arm_data_proc_flags_reg()                                             \
  arm_decode_data_proc_reg(opcode);                                           \
  calculate_reg_sh_flags()                                                    \

#define arm_data_proc_reg()                                                   \
  arm_decode_data_proc_reg(opcode);                                           \
  calculate_reg_sh()                                                          \

#define arm_data_proc_flags_imm()                                             \
  arm_decode_data_proc_imm(opcode)                                            \

#define arm_data_proc_imm()                                                   \
  arm_decode_data_proc_imm(opcode)                                            \

#define arm_data_proc(expr, type)                                             \
{                                                                             \
  u32 dest;                                                                   \
  arm_pc_offset(8);                                                           \
  arm_data_proc_##type();                                                     \
  dest = expr;                                                                \
  arm_pc_offset(-4);                                                          \
  reg[rd] = dest;                                                             \
                                                                              \
  if(rd == 15)                                                                \
  {                                                                           \
    arm_update_pc();                                                          \
  }                                                                           \
}                                                                             \

#define flags_vars(src_a, src_b)                                              \
  u32 dest;                                                                   \
  const u32 _sa = src_a;                                                      \
  const u32 _sb = src_b                                                       \

#define arm_data_proc_logic_flags(expr, type)                                 \
{                                                                             \
  arm_pc_offset(8);                                                           \
  arm_data_proc_flags_##type();                                               \
  u32 dest = expr;                                                            \
  calculate_flags_logic(dest);                                                \
  arm_pc_offset(-4);                                                          \
  reg[rd] = dest;                                                             \
  arm_spsr_restore();                                                         \
}                                                                             \

#define arm_data_proc_add_flags(src_a, src_b, type)                           \
{                                                                             \
  arm_pc_offset(8);                                                           \
  arm_data_proc_##type();                                                     \
  flags_vars(src_a, src_b);                                                   \
  dest = _sa + _sb;                                                           \
  calculate_flags_add(dest, _sa, _sb);                                        \
  arm_pc_offset(-4);                                                          \
  reg[rd] = dest;                                                             \
  arm_spsr_restore();                                                         \
}

#define arm_data_proc_sub_flags(src_a, src_b, type)                           \
{                                                                             \
  arm_pc_offset(8);                                                           \
  arm_data_proc_##type();                                                     \
  flags_vars(src_a, src_b);                                                   \
  dest = _sa - _sb;                                                           \
  calculate_flags_sub(dest, _sa, _sb);                                        \
  arm_pc_offset(-4);                                                          \
  reg[rd] = dest;                                                             \
  arm_spsr_restore();                                                         \
}                                                                             \

#define arm_data_proc_test_logic(expr, type)                                  \
{                                                                             \
  arm_pc_offset(8);                                                           \
  arm_data_proc_flags_##type();                                               \
  u32 dest = expr;                                                            \
  calculate_flags_logic(dest);                                                \
  arm_pc_offset(-4);                                                          \
}                                                                             \

#define arm_data_proc_test_add(src_a, src_b, type)                            \
{                                                                             \
  arm_pc_offset(8);                                                           \
  arm_data_proc_##type();                                                     \
  flags_vars(src_a, src_b);                                                   \
  dest = _sa + _sb;                                                           \
  calculate_flags_add(dest, _sa, _sb);                                        \
  arm_pc_offset(-4);                                                          \
}                                                                             \

#define arm_data_proc_test_sub(src_a, src_b, type)                            \
{                                                                             \
  arm_pc_offset(8);                                                           \
  arm_data_proc_##type();                                                     \
  flags_vars(src_a, src_b);                                                   \
  dest = _sa - _sb;                                                           \
  calculate_flags_sub(dest, _sa, _sb);                                        \
  arm_pc_offset(-4);                                                          \
}                                                                             \

#define arm_multiply_flags_yes(_dest)                                         \
  calculate_z_flag(_dest);                                                    \
  calculate_n_flag(_dest);                                                    \

#define arm_multiply_flags_no(_dest)                                          \

#define arm_multiply_long_flags_yes(_dest_lo, _dest_hi)                       \
  z_flag = (_dest_lo == 0) & (_dest_hi == 0);                                 \
  calculate_n_flag(_dest_hi)                                                  \

#define arm_multiply_long_flags_no(_dest_lo, _dest_hi)                        \

#define arm_multiply(add_op, flags)                                           \
{                                                                             \
  u32 dest;                                                                   \
  arm_decode_multiply();                                                      \
  dest = (reg[rm] * reg[rs]) add_op;                                          \
  arm_multiply_flags_##flags(dest);                                           \
  reg[rd] = dest;                                                             \
  arm_pc_offset(4);                                                           \
}                                                                             \

#define arm_multiply_long_addop(type)                                         \
  + ((type##64)((((type##64)reg[rdhi]) << 32) | reg[rdlo]));                  \

#define arm_multiply_long(add_op, flags, type)                                \
{                                                                             \
  type##64 dest;                                                              \
  u32 dest_lo;                                                                \
  u32 dest_hi;                                                                \
  arm_decode_multiply_long();                                                 \
  dest = ((type##64)((type##32)reg[rm]) *                                     \
   (type##64)((type##32)reg[rn])) add_op;                                     \
  dest_lo = (u32)dest;                                                        \
  dest_hi = (u32)(dest >> 32);                                                \
  arm_multiply_long_flags_##flags(dest_lo, dest_hi);                          \
  reg[rdlo] = dest_lo;                                                        \
  reg[rdhi] = dest_hi;                                                        \
  arm_pc_offset(4);                                                           \
}                                                                             \

const u32 psr_masks[16] =
{
  0x00000000, 0x000000FF, 0x0000FF00, 0x0000FFFF, 0x00FF0000,
  0x00FF00FF, 0x00FFFF00, 0x00FFFFFF, 0xFF000000, 0xFF0000FF,
  0xFF00FF00, 0xFF00FFFF, 0xFFFF0000, 0xFFFF00FF, 0xFFFFFF00,
  0xFFFFFFFF
};

#define arm_psr_read(dummy, psr_reg)                                          \
  collapse_flags();                                                           \
  reg[rd] = psr_reg                                                           \

#define arm_psr_store_cpsr(source)                                            \
  reg[REG_CPSR] = (source & store_mask) | (reg[REG_CPSR] & (~store_mask));    \
  extract_flags();                                                            \
  if(store_mask & 0xFF)                                                       \
  {                                                                           \
    set_cpu_mode(cpu_modes[reg[REG_CPSR] & 0x1F]);                            \
    check_for_interrupts();                                                   \
  }                                                                           \

#define arm_psr_store_spsr(source)                                            \
  u32 _psr = spsr[reg[CPU_MODE]];                                             \
  spsr[reg[CPU_MODE]] = (source & store_mask) | (_psr & (~store_mask))        \

#define arm_psr_store(source, psr_reg)                                        \
  const u32 store_mask = psr_masks[psr_field];                                \
  arm_psr_store_##psr_reg(source)                                             \

#define arm_psr_src_reg reg[rm]

#define arm_psr_src_imm imm

#define arm_psr(op_type, transfer_type, psr_reg)                              \
{                                                                             \
  arm_decode_psr_##op_type(opcode);                                           \
  arm_pc_offset(4);                                                           \
  arm_psr_##transfer_type(arm_psr_src_##op_type, psr_reg);                    \
}                                                                             \

#define arm_data_trans_reg()                                                  \
  arm_decode_data_trans_reg();                                                \
  calculate_reg_offset()                                                      \

#define arm_data_trans_imm()                                                  \
  arm_decode_data_trans_imm()                                                 \

#define arm_data_trans_half_reg()                                             \
  arm_decode_half_trans_r()                                                   \

#define arm_data_trans_half_imm()                                             \
  arm_decode_half_trans_of()                                                  \

#define aligned_address_mask8  0xF0000000
#define aligned_address_mask16 0xF0000001
#define aligned_address_mask32 0xF0000003

#define fast_read_memory(size, type, addr, dest)                              \
{                                                                             \
  u8 *map;                                                                    \
  u32 _address = addr;                                                        \
                                                                              \
  if(_address < 0x10000000)                                                   \
  {                                                                           \
    memory_region_access_read_##type[_address >> 24]++;                       \
    memory_reads_##type++;                                                    \
  }                                                                           \
  if(((_address >> 24) == 0) && (pc >= 0x4000))                               \
  {                                                                           \
    dest = *((type *)((u8 *)&bios_read_protect + (_address & 0x03)));         \
  }                                                                           \
  else                                                                        \
                                                                              \
  if(((_address & aligned_address_mask##size) == 0) &&                        \
   (map = memory_map_read[_address >> 15]))                                   \
  {                                                                           \
    dest = (type)readaddress##size(map, (_address & 0x7FFF));                 \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    dest = (type)read_memory##size(_address);                                 \
  }                                                                           \
}                                                                             \

#define fast_read_memory_s16(address, dest)                                   \
{                                                                             \
  u8 *map;                                                                    \
  u32 _address = address;                                                     \
  if(_address < 0x10000000)                                                   \
  {                                                                           \
    memory_region_access_read_s16[_address >> 24]++;                          \
    memory_reads_s16++;                                                       \
  }                                                                           \
  if(((_address & aligned_address_mask16) == 0) &&                            \
   (map = memory_map_read[_address >> 15]))                                   \
  {                                                                           \
    dest = (s16)readaddress16(map, (_address & 0x7FFF));                      \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    dest = (s16)read_memory16_signed(_address);                               \
  }                                                                           \
}                                                                             \


#define fast_write_memory(size, type, address, value)                         \
{                                                                             \
  u32 _address = (address) & ~(aligned_address_mask##size & 0x03);            \
  if(_address < 0x10000000)                                                   \
  {                                                                           \
    memory_region_access_write_##type[_address >> 24]++;                      \
    memory_writes_##type++;                                                   \
  }                                                                           \
                                                                              \
  cpu_alert = write_memory##size(_address, value);                            \
  if(cpu_alert)                                                               \
    goto alert;                                                               \
}                                                                             \

#define load_aligned32(address, dest)                                         \
{                                                                             \
  u32 _address = address;                                                     \
  u8 *map = memory_map_read[_address >> 15];                                  \
  if(_address < 0x10000000)                                                   \
  {                                                                           \
    memory_region_access_read_u32[_address >> 24]++;                          \
    memory_reads_u32++;                                                       \
  }                                                                           \
  if(_address < 0x10000000 && map)                                            \
  {                                                                           \
    dest = readaddress32(map, _address & 0x7FFF);                             \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    dest = read_memory32(_address);                                           \
  }                                                                           \
}                                                                             \

#define store_aligned32(address, value)                                       \
{                                                                             \
  u32 _address = address;                                                     \
  if(_address < 0x10000000)                                                   \
  {                                                                           \
    memory_region_access_write_u32[_address >> 24]++;                         \
    memory_writes_u32++;                                                      \
  }                                                                           \
  cpu_alert = write_memory32(_address, value);                                \
  if(cpu_alert)                                                               \
    goto alert;                                                               \
}                                                                             \

#define load_memory_u8(address, dest)                                         \
  fast_read_memory(8, u8, address, dest)                                      \

#define load_memory_u16(address, dest)                                        \
  fast_read_memory(16, u16, address, dest)                                    \

#define load_memory_u32(address, dest)                                        \
  fast_read_memory(32, u32, address, dest)                                    \

#define load_memory_s8(address, dest)                                         \
  fast_read_memory(8, s8, address, dest)                                      \

#define load_memory_s16(address, dest)                                        \
  fast_read_memory_s16(address, dest)                                         \

#define store_memory_u8(address, value)                                       \
  fast_write_memory(8, u8, address, value)                                    \

#define store_memory_u16(address, value)                                      \
  fast_write_memory(16, u16, address, value)                                  \

#define store_memory_u32(address, value)                                      \
  fast_write_memory(32, u32, address, value)                                  \

#define no_op                                                                 \

#define arm_access_memory_writeback_yes(off_op)                               \
  reg[rn] = address off_op                                                    \

#define arm_access_memory_writeback_no(off_op)                                \

#define arm_access_memory_pc_preadjust_load()                                 \

#define arm_access_memory_pc_preadjust_store()                                \
  u32 reg_op = reg[rd];                                                       \
  if(rd == 15)                                                                \
    reg_op += 4                                                               \

#define arm_access_memory_pc_postadjust_load()                                \
  arm_update_pc()                                                             \

#define arm_access_memory_pc_postadjust_store()                               \

#define load_reg_op reg[rd]                                                   \

#define store_reg_op reg_op                                                   \

#define arm_access_memory(access_type, off_op, off_type, mem_type,            \
 wb, wb_off_op)                                                               \
{                                                                             \
  arm_pc_offset(8);                                                           \
  arm_data_trans_##off_type();                                                \
  u32 address = reg[rn] off_op;                                               \
  arm_access_memory_pc_preadjust_##access_type();                             \
                                                                              \
  arm_pc_offset(-4);                                                          \
  arm_access_memory_writeback_##wb(wb_off_op);                                \
  access_type##_memory_##mem_type(address, access_type##_reg_op);             \
  arm_access_memory_pc_postadjust_##access_type();                            \
}                                                                             \

#define word_bit_count(word)                                                  \
  (bit_count[word >> 8] + bit_count[word & 0xFF])                             \

#define sprint_no(access_type, offset_type, writeback_type)                   \

#define sprint_yes(access_type, offset_type, writeback_type)                  \
  printf("sbit on %s %s %s\n", #access_type, #offset_type, #writeback_type)   \

#define arm_block_writeback_load()                                            \
  if(!((reg_list >> rn) & 0x01))                                              \
  {                                                                           \
    reg[rn] = address;                                                        \
  }                                                                           \

#define arm_block_writeback_store()                                           \
  reg[rn] = address                                                           \

#define arm_block_writeback_yes(access_type)                                  \
  arm_block_writeback_##access_type()                                         \

#define arm_block_writeback_no(access_type)                                   \

#define load_block_memory(address, dest)                                      \
  dest = readaddress32(address_region, (address + offset) & 0x7FFF)           \

#define store_block_memory(address, dest)                                     \
  address32(address_region, (address + offset) & 0x7FFF) = eswap32(dest)      \

#define arm_block_memory_offset_down_a()                                      \
  (base - (word_bit_count(reg_list) * 4) + 4)                                 \

#define arm_block_memory_offset_down_b()                                      \
  (base - (word_bit_count(reg_list) * 4))                                     \

#define arm_block_memory_offset_no()                                          \
  (base)                                                                      \

#define arm_block_memory_offset_up()                                          \
  (base + 4)                                                                  \

#define arm_block_memory_writeback_down()                                     \
  reg[rn] = base - (word_bit_count(reg_list) * 4)                             \

#define arm_block_memory_writeback_up()                                       \
  reg[rn] = base + (word_bit_count(reg_list) * 4)                             \

#define arm_block_memory_writeback_no()                                       \

#define arm_block_memory_load_pc()                                            \
  load_aligned32(address, pc);                                                \
  reg[REG_PC] = pc                                                            \

#define arm_block_memory_store_pc()                                           \
  store_aligned32(address, pc + 4)                                            \

#define arm_block_memory(access_type, offset_type, writeback_type, s_bit)     \
{                                                                             \
  arm_decode_block_trans();                                                   \
  u32 base = reg[rn];                                                         \
  u32 address = arm_block_memory_offset_##offset_type() & 0xFFFFFFFC;         \
  u32 i;                                                                      \
                                                                              \
  arm_block_memory_writeback_##writeback_type();                              \
                                                                              \
  for(i = 0; i < 15; i++)                                                     \
  {                                                                           \
    if((reg_list >> i) & 0x01)                                                \
    {                                                                         \
      access_type##_aligned32(address, reg[i]);                               \
      address += 4;                                                           \
    }                                                                         \
  }                                                                           \
                                                                              \
  arm_pc_offset(4);                                                           \
  if(reg_list & 0x8000)                                                       \
  {                                                                           \
    arm_block_memory_##access_type##_pc();                                    \
  }                                                                           \
}                                                                             \

#define arm_swap(type)                                                        \
{                                                                             \
  arm_decode_swap();                                                          \
  u32 temp;                                                                   \
  load_memory_##type(reg[rn], temp);                                          \
  store_memory_##type(reg[rn], reg[rm]);                                      \
  reg[rd] = temp;                                                             \
  arm_pc_offset(4);                                                           \
}                                                                             \

#define arm_next_instruction()                                                \
{                                                                             \
  arm_pc_offset(4);                                                           \
  goto skip_instruction;                                                      \
}                                                                             \

#define thumb_update_pc()                                                     \
  pc = reg[REG_PC]                                                            \

#define thumb_pc_offset(val)                                                  \
  pc += val;                                                                  \
  reg[REG_PC] = pc                                                            \

#define thumb_pc_offset_update(val)                                           \
  pc += val;                                                                  \
  reg[REG_PC] = pc                                                            \

#define thumb_pc_offset_update_direct(val)                                    \
  pc = val;                                                                   \
  reg[REG_PC] = pc                                                            \

// Types: add_sub, add_sub_imm, alu_op, imm
// Affects N/Z/C/V flags

#define thumb_add(type, dest_reg, src_a, src_b)                               \
{                                                                             \
  thumb_decode_##type();                                                      \
  const u32 _sa = src_a;                                                      \
  const u32 _sb = src_b;                                                      \
  u32 dest = _sa + _sb;                                                       \
  calculate_flags_add(dest, _sa, _sb);                                        \
  reg[dest_reg] = dest;                                                       \
  thumb_pc_offset(2);                                                         \
}                                                                             \

#define thumb_add_noflags(type, dest_reg, src_a, src_b)                       \
{                                                                             \
  thumb_decode_##type();                                                      \
  u32 dest = (src_a) + (src_b);                                               \
  reg[dest_reg] = dest;                                                       \
  thumb_pc_offset(2);                                                         \
}                                                                             \

#define thumb_sub(type, dest_reg, src_a, src_b)                               \
{                                                                             \
  thumb_decode_##type();                                                      \
  const u32 _sa = src_a;                                                      \
  const u32 _sb = src_b;                                                      \
  u32 dest = _sa - _sb;                                                       \
  calculate_flags_sub(dest, _sa, _sb);                                        \
  reg[dest_reg] = dest;                                                       \
  thumb_pc_offset(2);                                                         \
}                                                                             \

// Affects N/Z flags

#define thumb_logic(type, dest_reg, expr)                                     \
{                                                                             \
  thumb_decode_##type();                                                      \
  u32 dest = expr;                                                            \
  calculate_flags_logic(dest);                                                \
  reg[dest_reg] = dest;                                                       \
  thumb_pc_offset(2);                                                         \
}                                                                             \

// Decode types: shift, alu_op
// Operation types: lsl, lsr, asr, ror
// Affects N/Z/C flags

#define thumb_shift_lsl_reg()                                                 \
  u32 shift = reg[rs];                                                        \
  u32 dest = reg[rd];                                                         \
  if(shift != 0)                                                              \
  {                                                                           \
    if(shift > 31)                                                            \
    {                                                                         \
      if(shift == 32)                                                         \
        c_flag = dest & 0x01;                                                 \
      else                                                                    \
        c_flag = 0;                                                           \
      dest = 0;                                                               \
    }                                                                         \
    else                                                                      \
    {                                                                         \
      c_flag = (dest >> (32 - shift)) & 0x01;                                 \
      dest <<= shift;                                                         \
    }                                                                         \
  }                                                                           \

#define thumb_shift_lsr_reg()                                                 \
  u32 shift = reg[rs];                                                        \
  u32 dest = reg[rd];                                                         \
  if(shift != 0)                                                              \
  {                                                                           \
    if(shift > 31)                                                            \
    {                                                                         \
      if(shift == 32)                                                         \
        c_flag = dest >> 31;                                                  \
      else                                                                    \
        c_flag = 0;                                                           \
      dest = 0;                                                               \
    }                                                                         \
    else                                                                      \
    {                                                                         \
      c_flag = (dest >> (shift - 1)) & 0x01;                                  \
      dest >>= shift;                                                         \
    }                                                                         \
  }                                                                           \

#define thumb_shift_asr_reg()                                                 \
  u32 shift = reg[rs];                                                        \
  u32 dest = reg[rd];                                                         \
  if(shift != 0)                                                              \
  {                                                                           \
    if(shift > 31)                                                            \
    {                                                                         \
      dest = (s32)dest >> 31;                                                 \
      c_flag = dest & 0x01;                                                   \
    }                                                                         \
    else                                                                      \
    {                                                                         \
      c_flag = (dest >> (shift - 1)) & 0x01;                                  \
      dest = (s32)dest >> shift;                                              \
    }                                                                         \
  }                                                                           \

#define thumb_shift_ror_reg()                                                 \
  u32 shift = reg[rs];                                                        \
  u32 dest = reg[rd];                                                         \
  if(shift != 0)                                                              \
  {                                                                           \
    c_flag = (dest >> (shift - 1)) & 0x01;                                    \
    ror(dest, dest, shift);                                                   \
  }                                                                           \

#define thumb_shift_lsl_imm()                                                 \
  u32 dest = reg[rs];                                                         \
  if(imm != 0)                                                                \
  {                                                                           \
    c_flag = (dest >> (32 - imm)) & 0x01;                                     \
    dest <<= imm;                                                             \
  }                                                                           \

#define thumb_shift_lsr_imm()                                                 \
  u32 dest;                                                                   \
  if(imm == 0)                                                                \
  {                                                                           \
    dest = 0;                                                                 \
    c_flag = reg[rs] >> 31;                                                   \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    dest = reg[rs];                                                           \
    c_flag = (dest >> (imm - 1)) & 0x01;                                      \
    dest >>= imm;                                                             \
  }                                                                           \

#define thumb_shift_asr_imm()                                                 \
  u32 dest;                                                                   \
  if(imm == 0)                                                                \
  {                                                                           \
    dest = (s32)reg[rs] >> 31;                                                \
    c_flag = dest & 0x01;                                                     \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    dest = reg[rs];                                                           \
    c_flag = (dest >> (imm - 1)) & 0x01;                                      \
    dest = (s32)dest >> imm;                                                  \
  }                                                                           \

#define thumb_shift_ror_imm()                                                 \
  u32 dest = reg[rs];                                                         \
  if(imm == 0)                                                                \
  {                                                                           \
    u32 old_c_flag = c_flag;                                                  \
    c_flag = dest & 0x01;                                                     \
    dest = (dest >> 1) | (old_c_flag << 31);                                  \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    c_flag = (dest >> (imm - 1)) & 0x01;                                      \
    ror(dest, dest, imm);                                                     \
  }                                                                           \

#define thumb_shift(decode_type, op_type, value_type)                         \
{                                                                             \
  thumb_decode_##decode_type();                                               \
  thumb_shift_##op_type##_##value_type();                                     \
  calculate_flags_logic(dest);                                                \
  reg[rd] = dest;                                                             \
  thumb_pc_offset(2);                                                         \
}                                                                             \

#define thumb_test_add(type, src_a, src_b)                                    \
{                                                                             \
  thumb_decode_##type();                                                      \
  const u32 _sa = src_a;                                                      \
  const u32 _sb = src_b;                                                      \
  u32 dest = _sa + _sb;                                                       \
  calculate_flags_add(dest, src_a, src_b);                                    \
  thumb_pc_offset(2);                                                         \
}                                                                             \

#define thumb_test_sub(type, src_a, src_b)                                    \
{                                                                             \
  thumb_decode_##type();                                                      \
  const u32 _sa = src_a;                                                      \
  const u32 _sb = src_b;                                                      \
  u32 dest = _sa - _sb;                                                       \
  calculate_flags_sub(dest, src_a, src_b);                                    \
  thumb_pc_offset(2);                                                         \
}                                                                             \

#define thumb_test_logic(type, expr)                                          \
{                                                                             \
  thumb_decode_##type();                                                      \
  u32 dest = expr;                                                            \
  calculate_flags_logic(dest);                                                \
  thumb_pc_offset(2);                                                         \
}

#define thumb_hireg_op(expr)                                                  \
{                                                                             \
  thumb_pc_offset(4);                                                         \
  thumb_decode_hireg_op();                                                    \
  u32 dest = expr;                                                            \
  thumb_pc_offset(-2);                                                        \
  if(rd == 15)                                                                \
  {                                                                           \
    reg[REG_PC] = dest & ~0x01;                                               \
    thumb_update_pc();                                                        \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    reg[rd] = dest;                                                           \
  }                                                                           \
}                                                                             \

// Operation types: imm, mem_reg, mem_imm

#define thumb_access_memory(access_type, op_type, address, reg_op,            \
 mem_type)                                                                    \
{                                                                             \
  thumb_decode_##op_type();                                                   \
  access_type##_memory_##mem_type(address, reg_op);                           \
  thumb_pc_offset(2);                                                         \
}                                                                             \

#define thumb_block_address_preadjust_no_op()                                 \

#define thumb_block_address_preadjust_up()                                    \
  address += bit_count[reg_list] * 4                                          \

#define thumb_block_address_preadjust_down()                                  \
  address -= bit_count[reg_list] * 4                                          \

#define thumb_block_address_preadjust_push_lr()                               \
  address -= (bit_count[reg_list] + 1) * 4                                    \

#define thumb_block_address_postadjust_no_op()                                \

#define thumb_block_address_postadjust_up()                                   \
  address += offset                                                           \

#define thumb_block_address_postadjust_down()                                 \
  address -= offset                                                           \

#define thumb_block_address_postadjust_pop_pc()                               \
  load_memory_u32(address + offset, pc);                                      \
  pc &= ~0x01;                                                                \
  reg[REG_PC] = pc;                                                           \
  address += offset + 4                                                       \

#define thumb_block_address_postadjust_push_lr()                              \
  store_memory_u32(address + offset, reg[REG_LR]);                            \

#define thumb_block_memory_wb_load(base_reg)                                  \
  if(!((reg_list >> base_reg) & 0x01))                                        \
  {                                                                           \
    reg[base_reg] = address;                                                  \
  }                                                                           \

#define thumb_block_memory_wb_store(base_reg)                                 \
  reg[base_reg] = address                                                     \

#define thumb_block_memory(access_type, pre_op, post_op, base_reg)            \
{                                                                             \
  u32 i;                                                                      \
  u32 offset = 0;                                                             \
  thumb_decode_rlist();                                                       \
  using_register(thumb, base_reg, memory_base);                               \
  u32 address = reg[base_reg] & ~0x03;                                        \
  thumb_block_address_preadjust_##pre_op();                                   \
                                                                              \
  for(i = 0; i < 8; i++)                                                      \
  {                                                                           \
    if((reg_list >> i) & 1)                                                   \
    {                                                                         \
      access_type##_aligned32(address + offset, reg[i]);                      \
      offset += 4;                                                            \
    }                                                                         \
  }                                                                           \
                                                                              \
  thumb_pc_offset(2);                                                         \
                                                                              \
  thumb_block_address_postadjust_##post_op();                                 \
  thumb_block_memory_wb_##access_type(base_reg);                              \
}                                                                             \

#define thumb_conditional_branch(condition)                                   \
{                                                                             \
  thumb_decode_branch_cond();                                                 \
  if(condition)                                                               \
  {                                                                           \
    thumb_pc_offset((offset * 2) + 4);                                        \
  }                                                                           \
  else                                                                        \
  {                                                                           \
    thumb_pc_offset(2);                                                       \
  }                                                                           \
}                                                                             \

// When a mode change occurs from non-FIQ to non-FIQ retire the current
// reg[13] and reg[14] into reg_mode[cpu_mode][5] and reg_mode[cpu_mode][6]
// respectively and load into reg[13] and reg[14] reg_mode[new_mode][5] and
// reg_mode[new_mode][6]. When swapping to/from FIQ retire/load reg[8]
// through reg[14] to/from reg_mode[MODE_FIQ][0] through reg_mode[MODE_FIQ][6].

u32 cpu_modes[32] =
{
  MODE_INVALID, MODE_INVALID, MODE_INVALID, MODE_INVALID, MODE_INVALID,
  MODE_INVALID, MODE_INVALID, MODE_INVALID, MODE_INVALID, MODE_INVALID,
  MODE_INVALID, MODE_INVALID, MODE_INVALID, MODE_INVALID, MODE_INVALID,
  MODE_INVALID, MODE_USER, MODE_FIQ, MODE_IRQ, MODE_SUPERVISOR, MODE_INVALID,
  MODE_INVALID, MODE_INVALID, MODE_ABORT, MODE_INVALID, MODE_INVALID,
  MODE_INVALID, MODE_INVALID, MODE_UNDEFINED, MODE_INVALID, MODE_INVALID,
  MODE_USER
};

u32 cpu_modes_cpsr[7] = { 0x10, 0x11, 0x12, 0x13, 0x17, 0x1B, 0x1F };

// When switching modes set spsr[new_mode] to cpsr. Modifying PC as the
// target of a data proc instruction will set cpsr to spsr[cpu_mode].

#ifndef HAVE_DYNAREC
u32 reg[64];
u32 spsr[6];
u32 reg_mode[7][7];
#endif

// ARM/Thumb mode is stored in the flags directly, this is simpler than
// shadowing it since it has a constant 1bit represenation.

char *reg_names[16] =
{
  " r0", " r1", " r2", " r3", " r4", " r5", " r6", " r7",
  " r8", " r9", "r10", " fp", " ip", " sp", " lr", " pc"
};

char *cpu_mode_names[] =
{
  "user", "irq", "fiq", "svsr", "abrt", "undf", "invd"
};

u32 instruction_count = 0;

u32 output_field = 0;
const u32 num_output_fields = 2;

u32 last_instruction = 0;

u32 in_interrupt = 0;

void set_cpu_mode(cpu_mode_type new_mode)
{
  u32 i;
  cpu_mode_type cpu_mode = reg[CPU_MODE];

  if(cpu_mode == new_mode)
     return;

  if(new_mode == MODE_FIQ)
  {
     for(i = 8; i < 15; i++)
        reg_mode[cpu_mode][i - 8] = reg[i];
  }
  else
  {
     reg_mode[cpu_mode][5] = reg[REG_SP];
     reg_mode[cpu_mode][6] = reg[REG_LR];
  }

  if(cpu_mode == MODE_FIQ)
  {
     for(i = 8; i < 15; i++)
        reg[i] = reg_mode[new_mode][i - 8];
  }
  else
  {
     reg[REG_SP] = reg_mode[new_mode][5];
     reg[REG_LR] = reg_mode[new_mode][6];
  }

  reg[CPU_MODE] = new_mode;
}

void raise_interrupt(irq_type irq_raised)
{
  // The specific IRQ must be enabled in IE, master IRQ enable must be on,
  // and it must be on in the flags.
  write_ioreg(REG_IF, read_ioreg(REG_IF) | irq_raised);

  if((read_ioreg(REG_IE) & irq_raised) && read_ioreg(REG_IME) &&
   ((reg[REG_CPSR] & 0x80) == 0))
  {
    bios_read_protect = 0xe55ec002;

    // Interrupt handler in BIOS
    reg_mode[MODE_IRQ][6] = reg[REG_PC] + 4;
    spsr[MODE_IRQ] = reg[REG_CPSR];
    reg[REG_CPSR] = 0xD2;
    reg[REG_PC] = 0x00000018;

    set_cpu_mode(MODE_IRQ);
    reg[CPU_HALT_STATE] = CPU_ACTIVE;
    reg[CHANGED_PC_STATUS] = 1;
  }
}

#ifndef HAVE_DYNAREC
u8 *memory_map_read [8 * 1024];
u16 oam_ram[512];
u16 palette_ram[512];
u16 palette_ram_converted[512];
#endif

void execute_arm(u32 cycles)
{
  u32 pc = reg[REG_PC];
  u32 opcode;
  u32 condition;
  u32 n_flag, z_flag, c_flag, v_flag;
  u32 pc_region = (pc >> 15);
  u8 *pc_address_block = memory_map_read[pc_region];
  u32 new_pc_region;
  s32 cycles_remaining;
  u32 cycles_per_instruction = global_cycles_per_instruction;
  cpu_alert_type cpu_alert;

  u32 old_pc;

  (void)old_pc;

  if(!pc_address_block)
    pc_address_block = load_gamepak_page(pc_region & 0x3FF);

  cycles_remaining = cycles;
  while(1)
  {
    /* Do not execute until CPU is active */
    while(reg[CPU_HALT_STATE] != CPU_ACTIVE) {
       cycles_remaining = update_gba();

       if (reg[COMPLETED_FRAME])
          return;
    }

    pc = reg[REG_PC];
    extract_flags();

    if(reg[REG_CPSR] & 0x20)
      goto thumb_loop;

    do
    {
arm_loop:

       collapse_flags();
       cycles_per_instruction = global_cycles_per_instruction;

       /* Process cheats if we are about to execute the cheat hook */
       if (pc == cheat_master_hook)
          process_cheats();

       old_pc = pc;

       /* Execute ARM instruction */
       using_instruction(arm);
       check_pc_region();
       pc &= ~0x03;
       opcode = readaddress32(pc_address_block, (pc & 0x7FFF));
       condition = opcode >> 28;

       switch(condition)
       {
          case 0x0:
             /* EQ */
             if(!z_flag)
                arm_next_instruction();
             break;
          case 0x1:
             /* NE      */
             if(z_flag)
                arm_next_instruction();
             break;
          case 0x2:
             /* CS       */
             if(!c_flag)
                arm_next_instruction();
             break;
          case 0x3:
             /* CC       */
             if(c_flag)
                arm_next_instruction();
             break;
          case 0x4:
             /* MI       */
             if(!n_flag)
                arm_next_instruction();
             break;

          case 0x5:
             /* PL       */
             if(n_flag)
                arm_next_instruction();
             break;

          case 0x6:
             /* VS       */
             if(!v_flag)
                arm_next_instruction();
             break;

          case 0x7:
             /* VC       */
             if(v_flag)
                arm_next_instruction();
             break;

          case 0x8:
             /* HI       */
             if((c_flag == 0) | z_flag)
                arm_next_instruction();
             break;

          case 0x9:
             /* LS       */
             if(c_flag & (z_flag ^ 1))
                arm_next_instruction();
             break;

          case 0xA:
             /* GE       */
             if(n_flag != v_flag)
                arm_next_instruction();
             break;

          case 0xB:
             /* LT       */
             if(n_flag == v_flag)
                arm_next_instruction();
             break;

          case 0xC:
             /* GT       */
             if(z_flag | (n_flag != v_flag))
                arm_next_instruction();
             break;

          case 0xD:
             /* LE       */
             if((z_flag == 0) & (n_flag == v_flag))
                arm_next_instruction();
             break;

          case 0xE:
             /* AL       */
             break;

          case 0xF:
             /* Reserved - treat as "never" */
             arm_next_instruction();
             break;
       }

       switch((opcode >> 20) & 0xFF)
       {
          case 0x00:
             if((opcode & 0x90) == 0x90)
             {
                if(opcode & 0x20)
                {
                   /* STRH rd, [rn], -rm */
                   arm_access_memory(store, no_op, half_reg, u16, yes, - reg[rm]);
                }
                else
                {
                   /* MUL rd, rm, rs */
                   arm_multiply(no_op, no);
                }
             }
             else
             {
                /* AND rd, rn, reg_op */
                arm_data_proc(reg[rn] & reg_sh, reg);
             }
             break;

          case 0x01:
             if((opcode & 0x90) == 0x90)
             {
                switch((opcode >> 5) & 0x03)
                {
                   case 0:
                      /* MULS rd, rm, rs */
                      arm_multiply(no_op, yes);
                      break;

                   case 1:
                      /* LDRH rd, [rn], -rm */
                      arm_access_memory(load, no_op, half_reg, u16, yes, - reg[rm]);
                      break;

                   case 2:
                      /* LDRSB rd, [rn], -rm */
                      arm_access_memory(load, no_op, half_reg, s8, yes, - reg[rm]);
                      break;

                   case 3:
                      /* LDRSH rd, [rn], -rm */
                      arm_access_memory(load, no_op, half_reg, s16, yes, - reg[rm]);
                      break;
                }
             }
             else
             {
                /* ANDS rd, rn, reg_op */
                arm_data_proc_logic_flags(reg[rn] & reg_sh, reg);
             }
             break;

          case 0x02:
             if((opcode & 0x90) == 0x90)
             {
                if(opcode & 0x20)
                {
                   /* STRH rd, [rn], -rm */
                   arm_access_memory(store, no_op, half_reg, u16, yes, - reg[rm]);
                }
                else
                {
                   /* MLA rd, rm, rs, rn */
                   arm_multiply(+ reg[rn], no);
                }
             }
             else
             {
                /* EOR rd, rn, reg_op */
                arm_data_proc(reg[rn] ^ reg_sh, reg);
             }
             break;

          case 0x03:
             if((opcode & 0x90) == 0x90)
             {
                switch((opcode >> 5) & 0x03)
                {
                   case 0:
                      /* MLAS rd, rm, rs, rn */
                      arm_multiply(+ reg[rn], yes);
                      break;

                   case 1:
                      /* LDRH rd, [rn], -rm */
                      arm_access_memory(load, no_op, half_reg, u16, yes, - reg[rm]);
                      break;

                   case 2:
                      /* LDRSB rd, [rn], -rm */
                      arm_access_memory(load, no_op, half_reg, s8, yes, - reg[rm]);
                      break;

                   case 3:
                      /* LDRSH rd, [rn], -rm */
                      arm_access_memory(load, no_op, half_reg, s16, yes, - reg[rm]);
                      break;
                }
             }
             else
             {
                /* EORS rd, rn, reg_op */
                arm_data_proc_logic_flags(reg[rn] ^ reg_sh, reg);
             }
             break;

          case 0x04:
             if((opcode & 0x90) == 0x90)
             {
                /* STRH rd, [rn], -imm */
                arm_access_memory(store, no_op, half_imm, u16, yes, - offset);
             }
             else
             {
                /* SUB rd, rn, reg_op */
                arm_data_proc(reg[rn] - reg_sh, reg);
             }
             break;

          case 0x05:
             if((opcode & 0x90) == 0x90)
             {
                switch((opcode >> 5) & 0x03)
                {
                   case 1:
                      /* LDRH rd, [rn], -imm */
                      arm_access_memory(load, no_op, half_imm, u16, yes, - offset);
                      break;

                   case 2:
                      /* LDRSB rd, [rn], -imm */
                      arm_access_memory(load, no_op, half_imm, s8, yes, - offset);
                      break;

                   case 3:
                      /* LDRSH rd, [rn], -imm */
                      arm_access_memory(load, no_op, half_imm, s16, yes, - offset);
                      break;
                }
             }
             else
             {
                /* SUBS rd, rn, reg_op */
                arm_data_proc_sub_flags(reg[rn], reg_sh, reg);
             }
             break;

          case 0x06:
             if((opcode & 0x90) == 0x90)
             {
                /* STRH rd, [rn], -imm */
                arm_access_memory(store, no_op, half_imm, u16, yes, - offset);
             }
             else
             {
                /* RSB rd, rn, reg_op */
                arm_data_proc(reg_sh - reg[rn], reg);
             }
             break;

          case 0x07:
             if((opcode & 0x90) == 0x90)
             {
                switch((opcode >> 5) & 0x03)
                {
                   case 1:
                      /* LDRH rd, [rn], -imm */
                      arm_access_memory(load, no_op, half_imm, u16, yes, - offset);
                      break;

                   case 2:
                      /* LDRSB rd, [rn], -imm */
                      arm_access_memory(load, no_op, half_imm, s8, yes, - offset);
                      break;

                   case 3:
                      /* LDRSH rd, [rn], -imm */
                      arm_access_memory(load, no_op, half_imm, s16, yes, - offset);
                      break;
                }
             }
             else
             {
                /* RSBS rd, rn, reg_op */
                arm_data_proc_sub_flags(reg_sh, reg[rn], reg);
             }
             break;

          case 0x08:
             if((opcode & 0x90) == 0x90)
             {
                if(opcode & 0x20)
                {
                   /* STRH rd, [rn], +rm */
                   arm_access_memory(store, no_op, half_reg, u16, yes, + reg[rm]);
                }
                else
                {
                   /* UMULL rd, rm, rs */
                   arm_multiply_long(no_op, no, u);
                }
             }
             else
             {
                /* ADD rd, rn, reg_op */
                arm_data_proc(reg[rn] + reg_sh, reg);
             }
             break;

          case 0x09:
             if((opcode & 0x90) == 0x90)
             {
                switch((opcode >> 5) & 0x03)
                {
                   case 0:
                      /* UMULLS rdlo, rdhi, rm, rs */
                      arm_multiply_long(no_op, yes, u);
                      break;

                   case 1:
                      /* LDRH rd, [rn], +rm */
                      arm_access_memory(load, no_op, half_reg, u16, yes, + reg[rm]);
                      break;

                   case 2:
                      /* LDRSB rd, [rn], +rm */
                      arm_access_memory(load, no_op, half_reg, s8, yes, + reg[rm]);
                      break;

                   case 3:
                      /* LDRSH rd, [rn], +rm */
                      arm_access_memory(load, no_op, half_reg, s16, yes, + reg[rm]);
                      break;
                }
             }
             else
             {
                /* ADDS rd, rn, reg_op */
                arm_data_proc_add_flags(reg[rn], reg_sh, reg);
             }
             break;

          case 0x0A:
             if((opcode & 0x90) == 0x90)
             {
                if(opcode & 0x20)
                {
                   /* STRH rd, [rn], +rm */
                   arm_access_memory(store, no_op, half_reg, u16, yes, + reg[rm]);
                }
                else
                {
                   /* UMLAL rd, rm, rs */
                   arm_multiply_long(arm_multiply_long_addop(u), no, u);
                }
             }
             else
             {
                /* ADC rd, rn, reg_op */
                arm_data_proc(reg[rn] + reg_sh + c_flag, reg);
             }
             break;

          case 0x0B:
             if((opcode & 0x90) == 0x90)
             {
                switch((opcode >> 5) & 0x03)
                {
                   case 0:
                      /* UMLALS rdlo, rdhi, rm, rs */
                      arm_multiply_long(arm_multiply_long_addop(u), yes, u);
                      break;

                   case 1:
                      /* LDRH rd, [rn], +rm */
                      arm_access_memory(load, no_op, half_reg, u16, yes, + reg[rm]);
                      break;

                   case 2:
                      /* LDRSB rd, [rn], +rm */
                      arm_access_memory(load, no_op, half_reg, s8, yes, + reg[rm]);
                      break;

                   case 3:
                      /* LDRSH rd, [rn], +rm */
                      arm_access_memory(load, no_op, half_reg, s16, yes, + reg[rm]);
                      break;
                }
             }
             else
             {
                /* ADCS rd, rn, reg_op */
                arm_data_proc_add_flags(reg[rn], reg_sh + c_flag, reg);
             }
             break;

          case 0x0C:
             if((opcode & 0x90) == 0x90)
             {
                if(opcode & 0x20)
                {
                   /* STRH rd, [rn], +imm */
                   arm_access_memory(store, no_op, half_imm, u16, yes, + offset);
                }
                else
                {
                   /* SMULL rd, rm, rs */
                   arm_multiply_long(no_op, no, s);
                }
             }
             else
             {
                /* SBC rd, rn, reg_op */
                arm_data_proc(reg[rn] - (reg_sh + (c_flag ^ 1)), reg);
             }
             break;

          case 0x0D:
             if((opcode & 0x90) == 0x90)
             {
                switch((opcode >> 5) & 0x03)
                {
                   case 0:
                      /* SMULLS rdlo, rdhi, rm, rs */
                      arm_multiply_long(no_op, yes, s);
                      break;

                   case 1:
                      /* LDRH rd, [rn], +imm */
                      arm_access_memory(load, no_op, half_imm, u16, yes, + offset);
                      break;

                   case 2:
                      /* LDRSB rd, [rn], +imm */
                      arm_access_memory(load, no_op, half_imm, s8, yes, + offset);
                      break;

                   case 3:
                      /* LDRSH rd, [rn], +imm */
                      arm_access_memory(load, no_op, half_imm, s16, yes, + offset);
                      break;
                }
             }
             else
             {
                /* SBCS rd, rn, reg_op */
                arm_data_proc_sub_flags(reg[rn], (reg_sh + (c_flag ^ 1)), reg);
             }
             break;

          case 0x0E:
             if((opcode & 0x90) == 0x90)
             {
                if(opcode & 0x20)
                {
                   /* STRH rd, [rn], +imm */
                   arm_access_memory(store, no_op, half_imm, u16, yes, + offset);
                }
                else
                {
                   /* SMLAL rd, rm, rs */
                   arm_multiply_long(arm_multiply_long_addop(s), no, s);
                }
             }
             else
             {
                /* RSC rd, rn, reg_op */
                arm_data_proc(reg_sh - reg[rn] + c_flag - 1, reg);
             }
             break;

          case 0x0F:
             if((opcode & 0x90) == 0x90)
             {
                switch((opcode >> 5) & 0x03)
                {
                   case 0:
                      /* SMLALS rdlo, rdhi, rm, rs */
                      arm_multiply_long(arm_multiply_long_addop(s), yes, s);
                      break;

                   case 1:
                      /* LDRH rd, [rn], +imm */
                      arm_access_memory(load, no_op, half_imm, u16, yes, + offset);
                      break;

                   case 2:
                      /* LDRSB rd, [rn], +imm */
                      arm_access_memory(load, no_op, half_imm, s8, yes, + offset);
                      break;

                   case 3:
                      /* LDRSH rd, [rn], +imm */
                      arm_access_memory(load, no_op, half_imm, s16, yes, + offset);
                      break;
                }
             }
             else
             {
                /* RSCS rd, rn, reg_op */
                arm_data_proc_sub_flags((reg_sh + c_flag - 1), reg[rn], reg);
             }
             break;

          case 0x10:
             if((opcode & 0x90) == 0x90)
             {
                if(opcode & 0x20)
                {
                   /* STRH rd, [rn - rm] */
                   arm_access_memory(store, - reg[rm], half_reg, u16, no, no_op);
                }
                else
                {
                   /* SWP rd, rm, [rn] */
                   arm_swap(u32);
                }
             }
             else
             {
                /* MRS rd, cpsr */
                arm_psr(reg, read, reg[REG_CPSR]);
             }
             break;

          case 0x11:
             if((opcode & 0x90) == 0x90)
             {
                switch((opcode >> 5) & 0x03)
                {
                   case 1:
                      /* LDRH rd, [rn - rm] */
                      arm_access_memory(load, - reg[rm], half_reg, u16, no, no_op);
                      break;

                   case 2:
                      /* LDRSB rd, [rn - rm] */
                      arm_access_memory(load, - reg[rm], half_reg, s8, no, no_op);
                      break;

                   case 3:
                      /* LDRSH rd, [rn - rm] */
                      arm_access_memory(load, - reg[rm], half_reg, s16, no, no_op);
                      break;
                }
             }
             else
             {
                /* TST rd, rn, reg_op */
                arm_data_proc_test_logic(reg[rn] & reg_sh, reg);
             }
             break;

          case 0x12:
             if((opcode & 0x90) == 0x90)
             {
                /* STRH rd, [rn - rm]! */
                arm_access_memory(store, - reg[rm], half_reg, u16, yes, no_op);
             }
             else
             {
                if(opcode & 0x10)
                {
                   /* BX rn */
                   arm_decode_branchx(opcode);
                   u32 src = reg[rn];
                   if(src & 0x01)
                   {
                      src -= 1;
                      arm_pc_offset_update_direct(src);
                      reg[REG_CPSR] |= 0x20;
                      goto thumb_loop;
                   }
                   else
                   {
                      arm_pc_offset_update_direct(src);
                   }
                }
                else
                {
                   /* MSR cpsr, rm */
                   arm_psr(reg, store, cpsr);
                }
             }
             break;

          case 0x13:
             if((opcode & 0x90) == 0x90)
             {
                switch((opcode >> 5) & 0x03)
                {
                   case 1:
                      /* LDRH rd, [rn - rm]! */
                      arm_access_memory(load, - reg[rm], half_reg, u16, yes, no_op);
                      break;

                   case 2:
                      /* LDRSB rd, [rn - rm]! */
                      arm_access_memory(load, - reg[rm], half_reg, s8, yes, no_op);
                      break;

                   case 3:
                      /* LDRSH rd, [rn - rm]! */
                      arm_access_memory(load, - reg[rm], half_reg, s16, yes, no_op);
                      break;
                }
             }
             else
             {
                /* TEQ rd, rn, reg_op */
                arm_data_proc_test_logic(reg[rn] ^ reg_sh, reg);
             }
             break;

          case 0x14:
             if((opcode & 0x90) == 0x90)
             {
                if(opcode & 0x20)
                {
                   /* STRH rd, [rn - imm] */
                   arm_access_memory(store, - offset, half_imm, u16, no, no_op);
                }
                else
                {
                   /* SWPB rd, rm, [rn] */
                   arm_swap(u8);
                }
             }
             else
             {
                /* MRS rd, spsr */
                arm_psr(reg, read, spsr[reg[CPU_MODE]]);
             }
             break;

          case 0x15:
             if((opcode & 0x90) == 0x90)
             {
                switch((opcode >> 5) & 0x03)
                {
                   case 1:
                      /* LDRH rd, [rn - imm] */
                      arm_access_memory(load, - offset, half_imm, u16, no, no_op);
                      break;

                   case 2:
                      /* LDRSB rd, [rn - imm] */
                      arm_access_memory(load, - offset, half_imm, s8, no, no_op);
                      break;

                   case 3:
                      /* LDRSH rd, [rn - imm] */
                      arm_access_memory(load, - offset, half_imm, s16, no, no_op);
                      break;
                }
             }
             else
             {
                /* CMP rn, reg_op */
                arm_data_proc_test_sub(reg[rn], reg_sh, reg);
             }
             break;

          case 0x16:
             if((opcode & 0x90) == 0x90)
             {
                /* STRH rd, [rn - imm]! */
                arm_access_memory(store, - offset, half_imm, u16, yes, no_op);
             }
             else
             {
                /* MSR spsr, rm */
                arm_psr(reg, store, spsr);
             }
             break;

          case 0x17:
             if((opcode & 0x90) == 0x90)
             {
                switch((opcode >> 5) & 0x03)
                {
                   case 1:
                      /* LDRH rd, [rn - imm]! */
                      arm_access_memory(load, - offset, half_imm, u16, yes, no_op);
                      break;

                   case 2:
                      /* LDRSB rd, [rn - imm]! */
                      arm_access_memory(load, - offset, half_imm, s8, yes, no_op);
                      break;

                   case 3:
                      /* LDRSH rd, [rn - imm]! */
                      arm_access_memory(load, - offset, half_imm, s16, yes, no_op);
                      break;
                }
             }
             else
             {
                /* CMN rd, rn, reg_op */
                arm_data_proc_test_add(reg[rn], reg_sh, reg);
             }
             break;

          case 0x18:
             if((opcode & 0x90) == 0x90)
             {
                /* STRH rd, [rn + rm] */
                arm_access_memory(store, + reg[rm], half_reg, u16, no, no_op);
             }
             else
             {
                /* ORR rd, rn, reg_op */
                arm_data_proc(reg[rn] | reg_sh, reg);
             }
             break;

          case 0x19:
             if((opcode & 0x90) == 0x90)
             {
                switch((opcode >> 5) & 0x03)
                {
                   case 1:
                      /* LDRH rd, [rn + rm] */
                      arm_access_memory(load, + reg[rm], half_reg, u16, no, no_op);
                      break;

                   case 2:
                      /* LDRSB rd, [rn + rm] */
                      arm_access_memory(load, + reg[rm], half_reg, s8, no, no_op);
                      break;

                   case 3:
                      /* LDRSH rd, [rn + rm] */
                      arm_access_memory(load, + reg[rm], half_reg, s16, no, no_op);
                      break;
                }
             }
             else
             {
                /* ORRS rd, rn, reg_op */
                arm_data_proc_logic_flags(reg[rn] | reg_sh, reg);
             }
             break;

          case 0x1A:
             if((opcode & 0x90) == 0x90)
             {
                /* STRH rd, [rn + rm]! */
                arm_access_memory(store, + reg[rm], half_reg, u16, yes, no_op);
             }
             else
             {
                /* MOV rd, reg_op */
                arm_data_proc(reg_sh, reg);
             }
             break;

          case 0x1B:
             if((opcode & 0x90) == 0x90)
             {
                switch((opcode >> 5) & 0x03)
                {
                   case 1:
                      /* LDRH rd, [rn + rm]! */
                      arm_access_memory(load, + reg[rm], half_reg, u16, yes, no_op);
                      break;

                   case 2:
                      /* LDRSB rd, [rn + rm]! */
                      arm_access_memory(load, + reg[rm], half_reg, s8, yes, no_op);
                      break;

                   case 3:
                      /* LDRSH rd, [rn + rm]! */
                      arm_access_memory(load, + reg[rm], half_reg, s16, yes, no_op);
                      break;
                }
             }
             else
             {
                /* MOVS rd, reg_op */
                arm_data_proc_logic_flags(reg_sh, reg);
             }
             break;

          case 0x1C:
             if((opcode & 0x90) == 0x90)
             {
                /* STRH rd, [rn + imm] */
                arm_access_memory(store, + offset, half_imm, u16, no, no_op);
             }
             else
             {
                /* BIC rd, rn, reg_op */
                arm_data_proc(reg[rn] & (~reg_sh), reg);
             }
             break;

          case 0x1D:
             if((opcode & 0x90) == 0x90)
             {
                switch((opcode >> 5) & 0x03)
                {
                   case 1:
                      /* LDRH rd, [rn + imm] */
                      arm_access_memory(load, + offset, half_imm, u16, no, no_op);
                      break;

                   case 2:
                      /* LDRSB rd, [rn + imm] */
                      arm_access_memory(load, + offset, half_imm, s8, no, no_op);
                      break;

                   case 3:
                      /* LDRSH rd, [rn + imm] */
                      arm_access_memory(load, + offset, half_imm, s16, no, no_op);
                      break;
                }
             }
             else
             {
                /* BICS rd, rn, reg_op */
                arm_data_proc_logic_flags(reg[rn] & (~reg_sh), reg);
             }
             break;

          case 0x1E:
             if((opcode & 0x90) == 0x90)
             {
                /* STRH rd, [rn + imm]! */
                arm_access_memory(store, + offset, half_imm, u16, yes, no_op);
             }
             else
             {
                /* MVN rd, reg_op */
                arm_data_proc(~reg_sh, reg);
             }
             break;

          case 0x1F:
             if((opcode & 0x90) == 0x90)
             {
                switch((opcode >> 5) & 0x03)
                {
                   case 1:
                      /* LDRH rd, [rn + imm]! */
                      arm_access_memory(load, + offset, half_imm, u16, yes, no_op);
                      break;

                   case 2:
                      /* LDRSB rd, [rn + imm]! */
                      arm_access_memory(load, + offset, half_imm, s8, yes, no_op);
                      break;

                   case 3:
                      /* LDRSH rd, [rn + imm]! */
                      arm_access_memory(load, + offset, half_imm, s16, yes, no_op);
                      break;
                }
             }
             else
             {
                /* MVNS rd, rn, reg_op */
                arm_data_proc_logic_flags(~reg_sh, reg);
             }
             break;

          case 0x20:
             /* AND rd, rn, imm */
             arm_data_proc(reg[rn] & imm, imm);
             break;

          case 0x21:
             /* ANDS rd, rn, imm */
             arm_data_proc_logic_flags(reg[rn] & imm, imm);
             break;

          case 0x22:
             /* EOR rd, rn, imm */
             arm_data_proc(reg[rn] ^ imm, imm);
             break;

          case 0x23:
             /* EORS rd, rn, imm */
             arm_data_proc_logic_flags(reg[rn] ^ imm, imm);
             break;

          case 0x24:
             /* SUB rd, rn, imm */
             arm_data_proc(reg[rn] - imm, imm);
             break;

          case 0x25:
             /* SUBS rd, rn, imm */
             arm_data_proc_sub_flags(reg[rn], imm, imm);
             break;

          case 0x26:
             /* RSB rd, rn, imm */
             arm_data_proc(imm - reg[rn], imm);
             break;

          case 0x27:
             /* RSBS rd, rn, imm */
             arm_data_proc_sub_flags(imm, reg[rn], imm);
             break;

          case 0x28:
             /* ADD rd, rn, imm */
             arm_data_proc(reg[rn] + imm, imm);
             break;

          case 0x29:
             /* ADDS rd, rn, imm */
             arm_data_proc_add_flags(reg[rn], imm, imm);
             break;

          case 0x2A:
             /* ADC rd, rn, imm */
             arm_data_proc(reg[rn] + imm + c_flag, imm);
             break;

          case 0x2B:
             /* ADCS rd, rn, imm */
             arm_data_proc_add_flags(reg[rn] + imm, c_flag, imm);
             break;

          case 0x2C:
             /* SBC rd, rn, imm */
             arm_data_proc(reg[rn] - imm + c_flag - 1, imm);
             break;

          case 0x2D:
             /* SBCS rd, rn, imm */
             arm_data_proc_sub_flags(reg[rn], (imm + (c_flag ^ 1)), imm);
             break;

          case 0x2E:
             /* RSC rd, rn, imm */
             arm_data_proc(imm - reg[rn] + c_flag - 1, imm);
             break;

          case 0x2F:
             /* RSCS rd, rn, imm */
             arm_data_proc_sub_flags((imm + c_flag - 1), reg[rn], imm);
             break;

          case 0x30:
          case 0x31:
             /* TST rn, imm */
             arm_data_proc_test_logic(reg[rn] & imm, imm);
             break;

          case 0x32:
             /* MSR cpsr, imm */
             arm_psr(imm, store, cpsr);
             break;

          case 0x33:
             /* TEQ rn, imm */
             arm_data_proc_test_logic(reg[rn] ^ imm, imm);
             break;

          case 0x34:
          case 0x35:
             /* CMP rn, imm */
             arm_data_proc_test_sub(reg[rn], imm, imm);
             break;

          case 0x36:
             /* MSR spsr, imm */
             arm_psr(imm, store, spsr);
             break;

          case 0x37:
             /* CMN rn, imm */
             arm_data_proc_test_add(reg[rn], imm, imm);
             break;

          case 0x38:
             /* ORR rd, rn, imm */
             arm_data_proc(reg[rn] | imm, imm);
             break;

          case 0x39:
             /* ORRS rd, rn, imm */
             arm_data_proc_logic_flags(reg[rn] | imm, imm);
             break;

          case 0x3A:
             /* MOV rd, imm */
             arm_data_proc(imm, imm);
             break;

          case 0x3B:
             /* MOVS rd, imm */
             arm_data_proc_logic_flags(imm, imm);
             break;

          case 0x3C:
             /* BIC rd, rn, imm */
             arm_data_proc(reg[rn] & (~imm), imm);
             break;

          case 0x3D:
             /* BICS rd, rn, imm */
             arm_data_proc_logic_flags(reg[rn] & (~imm), imm);
             break;

          case 0x3E:
             /* MVN rd, imm */
             arm_data_proc(~imm, imm);
             break;

          case 0x3F:
             /* MVNS rd, imm */
             arm_data_proc_logic_flags(~imm, imm);
             break;

          case 0x40:
             /* STR rd, [rn], -imm */
             arm_access_memory(store, no_op, imm, u32, yes, - offset);
             break;

          case 0x41:
             /* LDR rd, [rn], -imm */
             arm_access_memory(load, no_op, imm, u32, yes, - offset);
             break;

          case 0x42:
             /* STRT rd, [rn], -imm */
             arm_access_memory(store, no_op, imm, u32, yes, - offset);
             break;

          case 0x43:
             /* LDRT rd, [rn], -imm */
             arm_access_memory(load, no_op, imm, u32, yes, - offset);
             break;

          case 0x44:
             /* STRB rd, [rn], -imm */
             arm_access_memory(store, no_op, imm, u8, yes, - offset);
             break;

          case 0x45:
             /* LDRB rd, [rn], -imm */
             arm_access_memory(load, no_op, imm, u8, yes, - offset);
             break;

          case 0x46:
             /* STRBT rd, [rn], -imm */
             arm_access_memory(store, no_op, imm, u8, yes, - offset);
             break;

          case 0x47:
             /* LDRBT rd, [rn], -imm */
             arm_access_memory(load, no_op, imm, u8, yes, - offset);
             break;

          case 0x48:
             /* STR rd, [rn], +imm */
             arm_access_memory(store, no_op, imm, u32, yes, + offset);
             break;

          case 0x49:
             /* LDR rd, [rn], +imm */
             arm_access_memory(load, no_op, imm, u32, yes, + offset);
             break;

          case 0x4A:
             /* STRT rd, [rn], +imm */
             arm_access_memory(store, no_op, imm, u32, yes, + offset);
             break;

          case 0x4B:
             /* LDRT rd, [rn], +imm */
             arm_access_memory(load, no_op, imm, u32, yes, + offset);
             break;

          case 0x4C:
             /* STRB rd, [rn], +imm */
             arm_access_memory(store, no_op, imm, u8, yes, + offset);
             break;

          case 0x4D:
             /* LDRB rd, [rn], +imm */
             arm_access_memory(load, no_op, imm, u8, yes, + offset);
             break;

          case 0x4E:
             /* STRBT rd, [rn], +imm */
             arm_access_memory(store, no_op, imm, u8, yes, + offset);
             break;

          case 0x4F:
             /* LDRBT rd, [rn], +imm */
             arm_access_memory(load, no_op, imm, u8, yes, + offset);
             break;

          case 0x50:
             /* STR rd, [rn - imm] */
             arm_access_memory(store, - offset, imm, u32, no, no_op);
             break;

          case 0x51:
             /* LDR rd, [rn - imm] */
             arm_access_memory(load, - offset, imm, u32, no, no_op);
             break;

          case 0x52:
             /* STR rd, [rn - imm]! */
             arm_access_memory(store, - offset, imm, u32, yes, no_op);
             break;

          case 0x53:
             /* LDR rd, [rn - imm]! */
             arm_access_memory(load, - offset, imm, u32, yes, no_op);
             break;

          case 0x54:
             /* STRB rd, [rn - imm] */
             arm_access_memory(store, - offset, imm, u8, no, no_op);
             break;

          case 0x55:
             /* LDRB rd, [rn - imm] */
             arm_access_memory(load, - offset, imm, u8, no, no_op);
             break;

          case 0x56:
             /* STRB rd, [rn - imm]! */
             arm_access_memory(store, - offset, imm, u8, yes, no_op);
             break;

          case 0x57:
             /* LDRB rd, [rn - imm]! */
             arm_access_memory(load, - offset, imm, u8, yes, no_op);
             break;

          case 0x58:
             /* STR rd, [rn + imm] */
             arm_access_memory(store, + offset, imm, u32, no, no_op);
             break;

          case 0x59:
             /* LDR rd, [rn + imm] */
             arm_access_memory(load, + offset, imm, u32, no, no_op);
             break;

          case 0x5A:
             /* STR rd, [rn + imm]! */
             arm_access_memory(store, + offset, imm, u32, yes, no_op);
             break;

          case 0x5B:
             /* LDR rd, [rn + imm]! */
             arm_access_memory(load, + offset, imm, u32, yes, no_op);
             break;

          case 0x5C:
             /* STRB rd, [rn + imm] */
             arm_access_memory(store, + offset, imm, u8, no, no_op);
             break;

          case 0x5D:
             /* LDRB rd, [rn + imm] */
             arm_access_memory(load, + offset, imm, u8, no, no_op);
             break;

          case 0x5E:
             /* STRB rd, [rn + imm]! */
             arm_access_memory(store, + offset, imm, u8, yes, no_op);
             break;

          case 0x5F:
             /* LDRBT rd, [rn + imm]! */
             arm_access_memory(load, + offset, imm, u8, yes, no_op);
             break;

          case 0x60:
             /* STR rd, [rn], -reg_op */
             arm_access_memory(store, no_op, reg, u32, yes, - reg_offset);
             break;

          case 0x61:
             /* LDR rd, [rn], -reg_op */
             arm_access_memory(load, no_op, reg, u32, yes, - reg_offset);
             break;

          case 0x62:
             /* STRT rd, [rn], -reg_op */
             arm_access_memory(store, no_op, reg, u32, yes, - reg_offset);
             break;

          case 0x63:
             /* LDRT rd, [rn], -reg_op */
             arm_access_memory(load, no_op, reg, u32, yes, - reg_offset);
             break;

          case 0x64:
             /* STRB rd, [rn], -reg_op */
             arm_access_memory(store, no_op, reg, u8, yes, - reg_offset);
             break;

          case 0x65:
             /* LDRB rd, [rn], -reg_op */
             arm_access_memory(load, no_op, reg, u8, yes, - reg_offset);
             break;

          case 0x66:
             /* STRBT rd, [rn], -reg_op */
             arm_access_memory(store, no_op, reg, u8, yes, - reg_offset);
             break;

          case 0x67:
             /* LDRBT rd, [rn], -reg_op */
             arm_access_memory(load, no_op, reg, u8, yes, - reg_offset);
             break;

          case 0x68:
             /* STR rd, [rn], +reg_op */
             arm_access_memory(store, no_op, reg, u32, yes, + reg_offset);
             break;

          case 0x69:
             /* LDR rd, [rn], +reg_op */
             arm_access_memory(load, no_op, reg, u32, yes, + reg_offset);
             break;

          case 0x6A:
             /* STRT rd, [rn], +reg_op */
             arm_access_memory(store, no_op, reg, u32, yes, + reg_offset);
             break;

          case 0x6B:
             /* LDRT rd, [rn], +reg_op */
             arm_access_memory(load, no_op, reg, u32, yes, + reg_offset);
             break;

          case 0x6C:
             /* STRB rd, [rn], +reg_op */
             arm_access_memory(store, no_op, reg, u8, yes, + reg_offset);
             break;

          case 0x6D:
             /* LDRB rd, [rn], +reg_op */
             arm_access_memory(load, no_op, reg, u8, yes, + reg_offset);
             break;

          case 0x6E:
             /* STRBT rd, [rn], +reg_op */
             arm_access_memory(store, no_op, reg, u8, yes, + reg_offset);
             break;

          case 0x6F:
             /* LDRBT rd, [rn], +reg_op */
             arm_access_memory(load, no_op, reg, u8, yes, + reg_offset);
             break;

          case 0x70:
             /* STR rd, [rn - reg_op] */
             arm_access_memory(store, - reg_offset, reg, u32, no, no_op);
             break;

          case 0x71:
             /* LDR rd, [rn - reg_op] */
             arm_access_memory(load, - reg_offset, reg, u32, no, no_op);
             break;

          case 0x72:
             /* STR rd, [rn - reg_op]! */
             arm_access_memory(store, - reg_offset, reg, u32, yes, no_op);
             break;

          case 0x73:
             /* LDR rd, [rn - reg_op]! */
             arm_access_memory(load, - reg_offset, reg, u32, yes, no_op);
             break;

          case 0x74:
             /* STRB rd, [rn - reg_op] */
             arm_access_memory(store, - reg_offset, reg, u8, no, no_op);
             break;

          case 0x75:
             /* LDRB rd, [rn - reg_op] */
             arm_access_memory(load, - reg_offset, reg, u8, no, no_op);
             break;

          case 0x76:
             /* STRB rd, [rn - reg_op]! */
             arm_access_memory(store, - reg_offset, reg, u8, yes, no_op);
             break;

          case 0x77:
             /* LDRB rd, [rn - reg_op]! */
             arm_access_memory(load, - reg_offset, reg, u8, yes, no_op);
             break;

          case 0x78:
             /* STR rd, [rn + reg_op] */
             arm_access_memory(store, + reg_offset, reg, u32, no, no_op);
             break;

          case 0x79:
             /* LDR rd, [rn + reg_op] */
             arm_access_memory(load, + reg_offset, reg, u32, no, no_op);
             break;

          case 0x7A:
             /* STR rd, [rn + reg_op]! */
             arm_access_memory(store, + reg_offset, reg, u32, yes, no_op);
             break;

          case 0x7B:
             /* LDR rd, [rn + reg_op]! */
             arm_access_memory(load, + reg_offset, reg, u32, yes, no_op);
             break;

          case 0x7C:
             /* STRB rd, [rn + reg_op] */
             arm_access_memory(store, + reg_offset, reg, u8, no, no_op);
             break;

          case 0x7D:
             /* LDRB rd, [rn + reg_op] */
             arm_access_memory(load, + reg_offset, reg, u8, no, no_op);
             break;

          case 0x7E:
             /* STRB rd, [rn + reg_op]! */
             arm_access_memory(store, + reg_offset, reg, u8, yes, no_op);
             break;

          case 0x7F:
             /* LDRBT rd, [rn + reg_op]! */
             arm_access_memory(load, + reg_offset, reg, u8, yes, no_op);
             break;

          case 0x80:
             /* STMDA rn, rlist */
             arm_block_memory(store, down_a, no, no);
             break;

          case 0x81:
             /* LDMDA rn, rlist */
             arm_block_memory(load, down_a, no, no);
             break;

          case 0x82:
             /* STMDA rn!, rlist */
             arm_block_memory(store, down_a, down, no);
             break;

          case 0x83:
             /* LDMDA rn!, rlist */
             arm_block_memory(load, down_a, down, no);
             break;

          case 0x84:
             /* STMDA rn, rlist^ */
             arm_block_memory(store, down_a, no, yes);
             break;

          case 0x85:
             /* LDMDA rn, rlist^ */
             arm_block_memory(load, down_a, no, yes);
             break;

          case 0x86:
             /* STMDA rn!, rlist^ */
             arm_block_memory(store, down_a, down, yes);
             break;

          case 0x87:
             /* LDMDA rn!, rlist^ */
             arm_block_memory(load, down_a, down, yes);
             break;

          case 0x88:
             /* STMIA rn, rlist */
             arm_block_memory(store, no, no, no);
             break;

          case 0x89:
             /* LDMIA rn, rlist */
             arm_block_memory(load, no, no, no);
             break;

          case 0x8A:
             /* STMIA rn!, rlist */
             arm_block_memory(store, no, up, no);
             break;

          case 0x8B:
             /* LDMIA rn!, rlist */
             arm_block_memory(load, no, up, no);
             break;

          case 0x8C:
             /* STMIA rn, rlist^ */
             arm_block_memory(store, no, no, yes);
             break;

          case 0x8D:
             /* LDMIA rn, rlist^ */
             arm_block_memory(load, no, no, yes);
             break;

          case 0x8E:
             /* STMIA rn!, rlist^ */
             arm_block_memory(store, no, up, yes);
             break;

          case 0x8F:
             /* LDMIA rn!, rlist^ */
             arm_block_memory(load, no, up, yes);
             break;

          case 0x90:
             /* STMDB rn, rlist */
             arm_block_memory(store, down_b, no, no);
             break;

          case 0x91:
             /* LDMDB rn, rlist */
             arm_block_memory(load, down_b, no, no);
             break;

          case 0x92:
             /* STMDB rn!, rlist */
             arm_block_memory(store, down_b, down, no);
             break;

          case 0x93:
             /* LDMDB rn!, rlist */
             arm_block_memory(load, down_b, down, no);
             break;

          case 0x94:
             /* STMDB rn, rlist^ */
             arm_block_memory(store, down_b, no, yes);
             break;

          case 0x95:
             /* LDMDB rn, rlist^ */
             arm_block_memory(load, down_b, no, yes);
             break;

          case 0x96:
             /* STMDB rn!, rlist^ */
             arm_block_memory(store, down_b, down, yes);
             break;

          case 0x97:
             /* LDMDB rn!, rlist^ */
             arm_block_memory(load, down_b, down, yes);
             break;

          case 0x98:
             /* STMIB rn, rlist */
             arm_block_memory(store, up, no, no);
             break;

          case 0x99:
             /* LDMIB rn, rlist */
             arm_block_memory(load, up, no, no);
             break;

          case 0x9A:
             /* STMIB rn!, rlist */
             arm_block_memory(store, up, up, no);
             break;

          case 0x9B:
             /* LDMIB rn!, rlist */
             arm_block_memory(load, up, up, no);
             break;

          case 0x9C:
             /* STMIB rn, rlist^ */
             arm_block_memory(store, up, no, yes);
             break;

          case 0x9D:
             /* LDMIB rn, rlist^ */
             arm_block_memory(load, up, no, yes);
             break;

          case 0x9E:
             /* STMIB rn!, rlist^ */
             arm_block_memory(store, up, up, yes);
             break;

          case 0x9F:
             /* LDMIB rn!, rlist^ */
             arm_block_memory(load, up, up, yes);
             break;

          case 0xA0 ... 0xAF:
             {
                /* B offset */
                arm_decode_branch();
                arm_pc_offset_update(offset + 8);
                break;
             }

          case 0xB0 ... 0xBF:
             {
                /* BL offset */
                arm_decode_branch();
                reg[REG_LR] = pc + 4;
                arm_pc_offset_update(offset + 8);
                break;
             }

#ifdef HAVE_UNUSED
          case 0xC0 ... 0xEF:
             /* coprocessor instructions, reserved on GBA */
             break;
#endif

          case 0xF0 ... 0xFF:
             {
                /* SWI comment */
                u32 swi_comment = opcode & 0x00FFFFFF;

                switch(swi_comment >> 16)
                {
                   /* Jump to BIOS SWI handler */
                   default:
                      reg_mode[MODE_SUPERVISOR][6] = pc + 4;
                      collapse_flags();
                      spsr[MODE_SUPERVISOR] = reg[REG_CPSR];
                      reg[REG_PC] = 0x00000008;
                      arm_update_pc();
                      reg[REG_CPSR] = (reg[REG_CPSR] & ~0x1F) | 0x13;
                      set_cpu_mode(MODE_SUPERVISOR);
                      break;
                }
                break;
             }
       }

skip_instruction:

       /* End of Execute ARM instruction */
       cycles_remaining -= cycles_per_instruction;

       if (pc == idle_loop_target_pc && cycles_remaining > 0) cycles_remaining = 0;
    } while(cycles_remaining > 0);

    collapse_flags();
    cycles_remaining = update_gba();
    if (reg[COMPLETED_FRAME])
       return;
    continue;

    do
    {
thumb_loop:

       collapse_flags();

       /* Process cheats if we are about to execute the cheat hook */
       if (pc == cheat_master_hook)
          process_cheats();

       old_pc = pc;

       /* Execute THUMB instruction */

       using_instruction(thumb);
       check_pc_region();
       pc &= ~0x01;
       opcode = readaddress16(pc_address_block, (pc & 0x7FFF));

       switch((opcode >> 8) & 0xFF)
       {
          case 0x00 ... 0x07:
             /* LSL rd, rs, offset */
             thumb_shift(shift, lsl, imm);
             break;

          case 0x08 ... 0x0F:
             /* LSR rd, rs, offset */
             thumb_shift(shift, lsr, imm);
             break;

          case 0x10 ... 0x17:
             /* ASR rd, rs, offset */
             thumb_shift(shift, asr, imm);
             break;

          case 0x18:
          case 0x19:
             /* ADD rd, rs, rn */
             thumb_add(add_sub, rd, reg[rs], reg[rn]);
             break;

          case 0x1A:
          case 0x1B:
             /* SUB rd, rs, rn */
             thumb_sub(add_sub, rd, reg[rs], reg[rn]);
             break;

          case 0x1C:
          case 0x1D:
             /* ADD rd, rs, imm */
             thumb_add(add_sub_imm, rd, reg[rs], imm);
             break;

          case 0x1E:
          case 0x1F:
             /* SUB rd, rs, imm */
             thumb_sub(add_sub_imm, rd, reg[rs], imm);
             break;

          case 0x20 ... 0x27:
             /* MOV r0..7, imm */
             thumb_logic(imm, ((opcode >> 8) & 7), imm);
             break;

          case 0x28 ... 0x2F:
             /* CMP r0..7, imm */
             thumb_test_sub(imm, reg[(opcode >> 8) & 7], imm);
             break;

          case 0x30 ... 0x37:
             /* ADD r0..7, imm */
             thumb_add(imm, ((opcode >> 8) & 7), reg[(opcode >> 8) & 7], imm);
             break;

          case 0x38 ... 0x3F:
             /* SUB r0..7, imm */
             thumb_sub(imm, ((opcode >> 8) & 7), reg[(opcode >> 8) & 7], imm);
             break;

          case 0x40:
             switch((opcode >> 6) & 0x03)
             {
                case 0x00:
                   /* AND rd, rs */
                   thumb_logic(alu_op, rd, reg[rd] & reg[rs]);
                   break;

                case 0x01:
                   /* EOR rd, rs */
                   thumb_logic(alu_op, rd, reg[rd] ^ reg[rs]);
                   break;

                case 0x02:
                   /* LSL rd, rs */
                   thumb_shift(alu_op, lsl, reg);
                   break;

                case 0x03:
                   /* LSR rd, rs */
                   thumb_shift(alu_op, lsr, reg);
                   break;
             }
             break;

          case 0x41:
             switch((opcode >> 6) & 0x03)
             {
                case 0x00:
                   /* ASR rd, rs */
                   thumb_shift(alu_op, asr, reg);
                   break;

                case 0x01:
                   /* ADC rd, rs */
                   thumb_add(alu_op, rd, reg[rd] + reg[rs], c_flag);
                   break;

                case 0x02:
                   /* SBC rd, rs */
                   thumb_sub(alu_op, rd, reg[rd] - reg[rs], (c_flag ^ 1));
                   break;

                case 0x03:
                   /* ROR rd, rs */
                   thumb_shift(alu_op, ror, reg);
                   break;
             }
             break;

          case 0x42:
             switch((opcode >> 6) & 0x03)
             {
                case 0x00:
                   /* TST rd, rs */
                   thumb_test_logic(alu_op, reg[rd] & reg[rs]);
                   break;

                case 0x01:
                   /* NEG rd, rs */
                   thumb_sub(alu_op, rd, 0, reg[rs]);
                   break;

                case 0x02:
                   /* CMP rd, rs */
                   thumb_test_sub(alu_op, reg[rd], reg[rs]);
                   break;

                case 0x03:
                   /* CMN rd, rs */
                   thumb_test_add(alu_op, reg[rd], reg[rs]);
                   break;
             }
             break;

          case 0x43:
             switch((opcode >> 6) & 0x03)
             {
                case 0x00:
                   /* ORR rd, rs */
                   thumb_logic(alu_op, rd, reg[rd] | reg[rs]);
                   break;

                case 0x01:
                   /* MUL rd, rs */
                   thumb_logic(alu_op, rd, reg[rd] * reg[rs]);
                   break;

                case 0x02:
                   /* BIC rd, rs */
                   thumb_logic(alu_op, rd, reg[rd] & (~reg[rs]));
                   break;

                case 0x03:
                   /* MVN rd, rs */
                   thumb_logic(alu_op, rd, ~reg[rs]);
                   break;
             }
             break;

          case 0x44:
             /* ADD rd, rs */
             thumb_hireg_op(reg[rd] + reg[rs]);
             break;

          case 0x45:
             /* CMP rd, rs */
             {
                thumb_pc_offset(4);
                thumb_decode_hireg_op();
                u32 _sa = reg[rd];
                u32 _sb = reg[rs];
                u32 dest = _sa - _sb;
                thumb_pc_offset(-2);
                calculate_flags_sub(dest, _sa, _sb);
             }
             break;

          case 0x46:
             /* MOV rd, rs */
             thumb_hireg_op(reg[rs]);
             break;

          case 0x47:
             /* BX rs */
             {
                thumb_decode_hireg_op();
                u32 src;
                thumb_pc_offset(4);
                src = reg[rs];
                if(src & 0x01)
                {
                   src -= 1;
                   thumb_pc_offset_update_direct(src);
                }
                else
                {
                   /* Switch to ARM mode */
                   thumb_pc_offset_update_direct(src);
                   reg[REG_CPSR] &= ~0x20;
                   collapse_flags();
                   goto arm_loop;
                }
             }
             break;

          case 0x48 ... 0x4F:
             /* LDR r0..7, [pc + imm] */
             thumb_access_memory(load, imm, (pc & ~2) + (imm * 4) + 4, reg[(opcode >> 8) & 7], u32);
             break;

          case 0x50:
          case 0x51:
             /* STR rd, [rb + ro] */
             thumb_access_memory(store, mem_reg, reg[rb] + reg[ro], reg[rd], u32);
             break;

          case 0x52:
          case 0x53:
             /* STRH rd, [rb + ro] */
             thumb_access_memory(store, mem_reg, reg[rb] + reg[ro], reg[rd], u16);
             break;

          case 0x54:
          case 0x55:
             /* STRB rd, [rb + ro] */
             thumb_access_memory(store, mem_reg, reg[rb] + reg[ro], reg[rd], u8);
             break;

          case 0x56:
          case 0x57:
             /* LDSB rd, [rb + ro] */
             thumb_access_memory(load, mem_reg, reg[rb] + reg[ro], reg[rd], s8);
             break;

          case 0x58:
          case 0x59:
             /* LDR rd, [rb + ro] */
             thumb_access_memory(load, mem_reg, reg[rb] + reg[ro], reg[rd], u32);
             break;

          case 0x5A:
          case 0x5B:
             /* LDRH rd, [rb + ro] */
             thumb_access_memory(load, mem_reg, reg[rb] + reg[ro], reg[rd], u16);
             break;

          case 0x5C:
          case 0x5D:
             /* LDRB rd, [rb + ro] */
             thumb_access_memory(load, mem_reg, reg[rb] + reg[ro], reg[rd], u8);
             break;

          case 0x5E:
          case 0x5F:
             /* LDSH rd, [rb + ro] */
             thumb_access_memory(load, mem_reg, reg[rb] + reg[ro], reg[rd], s16);
             break;

          case 0x60 ... 0x67:
             /* STR rd, [rb + imm] */
             thumb_access_memory(store, mem_imm, reg[rb] + (imm * 4), reg[rd], u32);
             break;

          case 0x68 ... 0x6F:
             /* LDR rd, [rb + imm] */
             thumb_access_memory(load, mem_imm, reg[rb] + (imm * 4), reg[rd], u32);
             break;

          case 0x70 ... 0x77:
             /* STRB rd, [rb + imm] */
             thumb_access_memory(store, mem_imm, reg[rb] + imm, reg[rd], u8);
             break;

          case 0x78 ... 0x7F:
             /* LDRB rd, [rb + imm] */
             thumb_access_memory(load, mem_imm, reg[rb] + imm, reg[rd], u8);
             break;

          case 0x80 ... 0x87:
             /* STRH rd, [rb + imm] */
             thumb_access_memory(store, mem_imm, reg[rb] + (imm * 2), reg[rd], u16);
             break;

          case 0x88 ... 0x8F:
             /* LDRH rd, [rb + imm] */
             thumb_access_memory(load, mem_imm, reg[rb] + (imm * 2), reg[rd], u16);
             break;

          case 0x90 ... 0x97:
             /* STR r0..7, [sp + imm] */
             thumb_access_memory(store, imm, reg[REG_SP] + (imm * 4), reg[(opcode >> 8) & 7], u32);
             break;

          case 0x98 ... 0x9F:
             /* LDR r0..7, [sp + imm] */
             thumb_access_memory(load, imm, reg[REG_SP] + (imm * 4), reg[(opcode >> 8) & 7], u32);
             break;

          case 0xA0 ... 0xA7:
             /* ADD r0..7, pc, +imm */
             thumb_add_noflags(imm, ((opcode >> 8) & 7), (pc & ~2) + 4, (imm * 4));
             break;

          case 0xA8 ... 0xAF:
             /* ADD r0..7, sp, +imm */
             thumb_add_noflags(imm, ((opcode >> 8) & 7), reg[REG_SP], (imm * 4));
             break;

          case 0xB0:
          case 0xB1:
          case 0xB2:
          case 0xB3:
             if((opcode >> 7) & 0x01)
             {
                /* ADD sp, -imm */
                thumb_add_noflags(add_sp, 13, reg[REG_SP], -(imm * 4));
             }
             else
             {
                /* ADD sp, +imm */
                thumb_add_noflags(add_sp, 13, reg[REG_SP], (imm * 4));
             }
             break;

          case 0xB4:
             /* PUSH rlist */
             thumb_block_memory(store, down, no_op, 13);
             break;

          case 0xB5:
             /* PUSH rlist, lr */
             thumb_block_memory(store, push_lr, push_lr, 13);
             break;

          case 0xBC:
             /* POP rlist */
             thumb_block_memory(load, no_op, up, 13);
             break;

          case 0xBD:
             /* POP rlist, pc */
             thumb_block_memory(load, no_op, pop_pc, 13);
             break;

          case 0xC0 ... 0xC7:
             /* STMIA r0..7!, rlist */
             thumb_block_memory(store, no_op, up, ((opcode >> 8) & 7));
             break;

          case 0xC8 ... 0xCF:
             /* LDMIA r0..7!, rlist */
             thumb_block_memory(load, no_op, up, ((opcode >> 8) & 7));
             break;

          case 0xD0:
             /* BEQ label */
             thumb_conditional_branch(z_flag == 1);
             break;

          case 0xD1:
             /* BNE label */
             thumb_conditional_branch(z_flag == 0);
             break;

          case 0xD2:
             /* BCS label */
             thumb_conditional_branch(c_flag == 1);
             break;

          case 0xD3:
             /* BCC label */
             thumb_conditional_branch(c_flag == 0);
             break;

          case 0xD4:
             /* BMI label */
             thumb_conditional_branch(n_flag == 1);
             break;

          case 0xD5:
             /* BPL label */
             thumb_conditional_branch(n_flag == 0);
             break;

          case 0xD6:
             /* BVS label */
             thumb_conditional_branch(v_flag == 1);
             break;

          case 0xD7:
             /* BVC label */
             thumb_conditional_branch(v_flag == 0);
             break;

          case 0xD8:
             /* BHI label */
             thumb_conditional_branch(c_flag & (z_flag ^ 1));
             break;

          case 0xD9:
             /* BLS label */
             thumb_conditional_branch((c_flag == 0) | z_flag);
             break;

          case 0xDA:
             /* BGE label */
             thumb_conditional_branch(n_flag == v_flag);
             break;

          case 0xDB:
             /* BLT label */
             thumb_conditional_branch(n_flag != v_flag);
             break;

          case 0xDC:
             /* BGT label */
             thumb_conditional_branch((z_flag == 0) & (n_flag == v_flag));
             break;

          case 0xDD:
             /* BLE label */
             thumb_conditional_branch(z_flag | (n_flag != v_flag));
             break;

          case 0xDF:
             {
                /* SWI comment */
                u32 swi_comment = opcode & 0xFF;

                switch(swi_comment)
                {
                   default:
                      reg_mode[MODE_SUPERVISOR][6] = pc + 2;
                      spsr[MODE_SUPERVISOR] = reg[REG_CPSR];
                      reg[REG_PC] = 0x00000008;
                      thumb_update_pc();
                      reg[REG_CPSR] = (reg[REG_CPSR] & ~0x3F) | 0x13;
                      set_cpu_mode(MODE_SUPERVISOR);
                      collapse_flags();
                      goto arm_loop;
                }
                break;
             }

          case 0xE0 ... 0xE7:
             {
                /* B label */
                thumb_decode_branch();
                thumb_pc_offset_update(((s32)(offset << 21) >> 20) + 4);
                break;
             }

          case 0xF0 ... 0xF7:
             {
                /* (low word) BL label */
                thumb_decode_branch();
                reg[REG_LR] = pc + 4 + ((s32)(offset << 21) >> 9);
                thumb_pc_offset(2);
                break;
             }

          case 0xF8 ... 0xFF:
             {
                /* (high word) BL label */
                thumb_decode_branch();
                u32 lr = (pc + 2) | 0x01;
                pc = reg[REG_LR] + (offset * 2);
                reg[REG_LR] = lr;
                reg[REG_PC] = pc;
                break;
             }
       }

       /* End of Execute THUMB instruction */
       cycles_remaining -= cycles_per_instruction;

       if (pc == idle_loop_target_pc && cycles_remaining > 0) cycles_remaining = 0;
    } while(cycles_remaining > 0);

    collapse_flags();
    cycles_remaining = update_gba();
    if (reg[COMPLETED_FRAME])
      return;
    continue;

    alert:

    if(cpu_alert != CPU_ALERT_IRQ) {
      collapse_flags();

      while(reg[CPU_HALT_STATE] != CPU_ACTIVE) {
        cycles_remaining = update_gba();
        if (reg[COMPLETED_FRAME])
          return;
      }
    }
  }
}

void init_cpu(void)
{
  u32 i;

  for(i = 0; i < 16; i++)
    reg[i] = 0;

  reg[CPU_HALT_STATE] = CPU_ACTIVE;
  reg[CHANGED_PC_STATUS] = 0;

  if (selected_boot_mode == boot_game) {
    reg[REG_SP] = 0x03007F00;
    reg[REG_PC] = 0x08000000;
    reg[REG_CPSR] = 0x0000001F;   // system mode
    reg[CPU_MODE] = MODE_USER;
  } else {
    reg[REG_SP] = 0x03007F00;
    reg[REG_PC] = 0x00000000;
    reg[REG_CPSR] = 0x00000013 | 0xC0;  // supervisor
    reg[CPU_MODE] = MODE_SUPERVISOR;
  }

  // Stack pointers are set by BIOS, we set them
  // nevertheless, should we not boot from BIOS
  reg_mode[MODE_USER][5] = 0x03007F00;
  reg_mode[MODE_IRQ][5] = 0x03007FA0;
  reg_mode[MODE_FIQ][5] = 0x03007FA0;
  reg_mode[MODE_SUPERVISOR][5] = 0x03007FE0;
}

#define cpu_savestate_builder(type)   \
void cpu_##type##_savestate(void)     \
{                                     \
  state_mem_##type(reg, 0x100);       \
  state_mem_##type##_array(spsr);     \
  state_mem_##type##_array(reg_mode); \
}

cpu_savestate_builder(read)
cpu_savestate_builder(write)
