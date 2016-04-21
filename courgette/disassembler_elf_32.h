// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COURGETTE_DISASSEMBLER_ELF_32_H_
#define COURGETTE_DISASSEMBLER_ELF_32_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "courgette/disassembler.h"
#include "courgette/image_utils.h"
#include "courgette/memory_allocator.h"
#include "courgette/types_elf.h"

namespace courgette {

class AssemblyProgram;

// A Courgette disassembler for 32-bit ELF files. This is only a partial
// implementation that admits subclasses for the architecture-specific parts of
// 32-bit ELF file processing. Specifically:
// - RelToRVA() processes entries in ELF relocation table.
// - ParseRelocationSection() verifies the organization of the ELF relocation
//   table.
// - ParseRel32RelocsFromSection() finds branch targets by looking for relative
//   branch/call opcodes in the particular architecture's machine code.
class DisassemblerElf32 : public Disassembler {
 public:
  // Different instructions encode the target rva differently.  This
  // class encapsulates this behavior.  public for use in unit tests.
  class TypedRVA {
   public:
    explicit TypedRVA(RVA rva) : rva_(rva) { }

    virtual ~TypedRVA() { }

    RVA rva() const { return rva_; }
    RVA relative_target() const { return relative_target_; }
    FileOffset file_offset() const { return file_offset_; }

    void set_relative_target(RVA relative_target) {
      relative_target_ = relative_target;
    }
    void set_file_offset(FileOffset file_offset) {
      file_offset_ = file_offset;
    }

    // Computes the relative jump's offset from the op in p.
    virtual CheckBool ComputeRelativeTarget(const uint8_t* op_pointer) = 0;

    // Emits the courgette instruction corresponding to the RVA type.
    virtual CheckBool EmitInstruction(AssemblyProgram* program,
                                      RVA target_rva) = 0;

    // Returns the size of the instruction containing the RVA.
    virtual uint16_t op_size() const = 0;

    // Comparator for sorting, which assumes uniqueness of RVAs.
    static bool IsLessThan(TypedRVA* a, TypedRVA* b) {
      return a->rva() < b->rva();
    }

  private:
    const RVA rva_;
    RVA relative_target_ = kNoRVA;
    FileOffset file_offset_ = kNoFileOffset;
  };

 public:
  DisassemblerElf32(const void* start, size_t length);

  ~DisassemblerElf32() override { }

  // Disassembler interfaces.
  RVA FileOffsetToRVA(FileOffset file_offset) const override;
  FileOffset RVAToFileOffset(RVA rva) const override;
  RVA PointerToTargetRVA(const uint8_t* p) const override;
  virtual ExecutableType kind() const override = 0;
  bool ParseHeader() override;
  bool Disassemble(AssemblyProgram* target) override;

  virtual e_machine_values ElfEM() const = 0;

  // Public for unittests only
  std::vector<RVA> &Abs32Locations() { return abs32_locations_; }
  ScopedVector<TypedRVA> &Rel32Locations() { return rel32_locations_; }

 protected:
  bool UpdateLength();

  // Misc Section Helpers

  Elf32_Half SectionHeaderCount() const {
    return section_header_table_size_;
  }

  const Elf32_Shdr* SectionHeader(Elf32_Half id) const {
    assert(id >= 0 && id < SectionHeaderCount());
    return &section_header_table_[id];
  }

  const uint8_t* SectionBody(Elf32_Half id) const {
    // TODO(huangs): Assert that section does not have SHT_NOBITS.
    return FileOffsetToPointer(SectionHeader(id)->sh_offset);
  }

  // Gets the |name| of section |shdr|. Returns true on success.
  CheckBool SectionName(const Elf32_Shdr& shdr, std::string* name) const;

  // Misc Segment Helpers

  Elf32_Half ProgramSegmentHeaderCount() const {
    return program_header_table_size_;
  }

  const Elf32_Phdr* ProgramSegmentHeader(Elf32_Half id) const {
    assert(id >= 0 && id < ProgramSegmentHeaderCount());
    return program_header_table_ + id;
  }

  // Misc address space helpers

  CheckBool IsValidTargetRVA(RVA rva) const WARN_UNUSED_RESULT;

  // Converts an ELF relocation instruction into an RVA.
  virtual CheckBool RelToRVA(Elf32_Rel rel, RVA* result)
    const WARN_UNUSED_RESULT = 0;

  CheckBool RVAsToFileOffsets(const std::vector<RVA>& rvas,
                              std::vector<FileOffset>* file_offsets);

  CheckBool RVAsToFileOffsets(ScopedVector<TypedRVA>* typed_rvas);

  // Parsing code for Disassemble().

  virtual CheckBool ParseRelocationSection(const Elf32_Shdr* section_header,
                                           AssemblyProgram* program)
      WARN_UNUSED_RESULT = 0;

  virtual CheckBool ParseRel32RelocsFromSection(const Elf32_Shdr* section)
      WARN_UNUSED_RESULT = 0;

  CheckBool ParseFile(AssemblyProgram* target) WARN_UNUSED_RESULT;

  CheckBool ParseProgbitsSection(
      const Elf32_Shdr* section_header,
      std::vector<FileOffset>::iterator* current_abs_offset,
      std::vector<FileOffset>::iterator end_abs_offset,
      ScopedVector<TypedRVA>::iterator* current_rel,
      ScopedVector<TypedRVA>::iterator end_rel,
      AssemblyProgram* program) WARN_UNUSED_RESULT;

  CheckBool ParseSimpleRegion(FileOffset start_file_offset,
                              FileOffset end_file_offset,
                              AssemblyProgram* program) WARN_UNUSED_RESULT;

  CheckBool ParseAbs32Relocs() WARN_UNUSED_RESULT;

  CheckBool CheckSection(RVA rva) WARN_UNUSED_RESULT;

  CheckBool ParseRel32RelocsFromSections() WARN_UNUSED_RESULT;

  const Elf32_Ehdr* header_;

  Elf32_Half section_header_table_size_;

  // Section header table, ordered by section id.
  std::vector<Elf32_Shdr> section_header_table_;

  // An ordering of |section_header_table_|, sorted by file offset.
  std::vector<Elf32_Half> section_header_file_offset_order_;

  const Elf32_Phdr* program_header_table_;
  Elf32_Half program_header_table_size_;

  // Pointer to string table containing section names.
  const char* default_string_section_;
  size_t default_string_section_size_;

  std::vector<RVA> abs32_locations_;
  ScopedVector<TypedRVA> rel32_locations_;

  DISALLOW_COPY_AND_ASSIGN(DisassemblerElf32);
};

}  // namespace courgette

#endif  // COURGETTE_DISASSEMBLER_ELF_32_H_
