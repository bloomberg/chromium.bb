// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COURGETTE_DISASSEMBLER_ELF_32_ARM_H_
#define COURGETTE_DISASSEMBLER_ELF_32_ARM_H_

#include "base/basictypes.h"
#include "courgette/disassembler_elf_32.h"
#include "courgette/memory_allocator.h"
#include "courgette/types_elf.h"

namespace courgette {

class AssemblyProgram;

enum ARM_RVA {
  ARM_OFF8,
  ARM_OFF11,
  ARM_OFF24,
  ARM_OFF25,
  ARM_OFF21,
};

class DisassemblerElf32ARM : public DisassemblerElf32 {
 public:
  class TypedRVAARM : public TypedRVA {
   public:
    TypedRVAARM(ARM_RVA type, RVA rva) : TypedRVA(rva), type_(type) { }

    uint16 c_op() const {
      return c_op_;
    }

    virtual CheckBool ComputeRelativeTarget(const uint8* op_pointer);

    virtual CheckBool EmitInstruction(AssemblyProgram* program,
                                      RVA target_rva);

    virtual uint16 op_size() const;

   private:
    ARM_RVA type_;

    uint16 c_op_;  // set by ComputeRelativeTarget()
    const uint8* arm_op_;
  };

  explicit DisassemblerElf32ARM(const void* start, size_t length);

  virtual ExecutableType kind() { return EXE_ELF_32_ARM; }

  virtual e_machine_values ElfEM() { return EM_ARM; }

  static CheckBool Compress(ARM_RVA type, uint32 arm_op, RVA rva,
                            uint16* c_op /* out */, uint32* addr /* out */);

  static CheckBool Decompress(ARM_RVA type, uint16 c_op, uint32 addr,
                              uint32* arm_op /* out */);

 protected:

  virtual CheckBool RelToRVA(Elf32_Rel rel, RVA* result)
    const WARN_UNUSED_RESULT;

  virtual CheckBool ParseRelocationSection(
      const Elf32_Shdr *section_header,
        AssemblyProgram* program) WARN_UNUSED_RESULT;

  virtual CheckBool ParseRel32RelocsFromSection(
      const Elf32_Shdr* section) WARN_UNUSED_RESULT;

#if COURGETTE_HISTOGRAM_TARGETS
  std::map<RVA, int> rel32_target_rvas_;
#endif

  DISALLOW_COPY_AND_ASSIGN(DisassemblerElf32ARM);
};

}  // namespace courgette

#endif  // COURGETTE_DISASSEMBLER_ELF_32_ARM_H_
