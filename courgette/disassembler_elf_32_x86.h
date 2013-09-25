// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COURGETTE_DISASSEMBLER_ELF_32_X86_H_
#define COURGETTE_DISASSEMBLER_ELF_32_X86_H_

#include "base/basictypes.h"
#include "courgette/disassembler_elf_32.h"
#include "courgette/memory_allocator.h"
#include "courgette/types_elf.h"

namespace courgette {

class AssemblyProgram;

class DisassemblerElf32X86 : public DisassemblerElf32 {
 public:
  class TypedRVAX86 : public TypedRVA {
   public:
    explicit TypedRVAX86(RVA rva) : TypedRVA(rva) {
    }

    virtual CheckBool ComputeRelativeTarget(const uint8* op_pointer) OVERRIDE {
      set_relative_target(Read32LittleEndian(op_pointer) + 4);
      return true;
    }

    virtual CheckBool EmitInstruction(AssemblyProgram* program,
                                       RVA target_rva) OVERRIDE {
      return program->EmitRel32(program->FindOrMakeRel32Label(target_rva));
    }

    virtual uint16 op_size() const OVERRIDE { return 4; }
  };

  explicit DisassemblerElf32X86(const void* start, size_t length);

  virtual ExecutableType kind() { return EXE_ELF_32_X86; }

  virtual e_machine_values ElfEM() { return EM_386; }

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

  DISALLOW_COPY_AND_ASSIGN(DisassemblerElf32X86);
};

}  // namespace courgette

#endif  // COURGETTE_DISASSEMBLER_ELF_32_X86_H_
