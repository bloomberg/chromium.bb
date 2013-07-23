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
};

class DisassemblerElf32ARM : public DisassemblerElf32 {
 public:
  class TypedRVAARM : public TypedRVA {
   public:
    TypedRVAARM(ARM_RVA type, RVA rva) : TypedRVA(rva), type_(type) { }

    virtual CheckBool ComputeRelativeTarget(const uint8* op_pointer) OVERRIDE;

   private:
    ARM_RVA type_;
  };

  explicit DisassemblerElf32ARM(const void* start, size_t length);

  virtual ExecutableType kind() { return EXE_ELF_32_ARM; }

  virtual e_machine_values ElfEM() { return EM_ARM; }

 protected:

  virtual CheckBool RelToRVA(Elf32_Rel rel, RVA* result)
    const WARN_UNUSED_RESULT;

  virtual CheckBool ParseRelocationSection(
      const Elf32_Shdr *section_header,
        AssemblyProgram* program) WARN_UNUSED_RESULT;

  virtual CheckBool ParseRel32RelocsFromSection(
      const Elf32_Shdr* section) WARN_UNUSED_RESULT;

  DISALLOW_COPY_AND_ASSIGN(DisassemblerElf32ARM);
};

}  // namespace courgette

#endif  // COURGETTE_DISASSEMBLER_ELF_32_ARM_H_
