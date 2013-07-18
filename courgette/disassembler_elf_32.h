// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COURGETTE_DISASSEMBLER_ELF_32_H_
#define COURGETTE_DISASSEMBLER_ELF_32_H_

#include "base/basictypes.h"
#include "courgette/disassembler.h"
#include "courgette/memory_allocator.h"
#include "courgette/types_elf.h"

namespace courgette {

class AssemblyProgram;

// A courgette disassembler for 32-bit ELF files.  This class is only a
// partial implementation.  Subclasses implement the
// architecture-specific parts of processing 32-bit ELF files.  Specifically,
// RelToRVA processes entries in ELF relocation table,
// ParseRelocationSection verifies the organization of the ELF
// relocation table, and ParseRel32RelocsFromSection finds branch
// targets by looking for relative jump/call opcodes in the particular
// architecture's machine code.
class DisassemblerElf32 : public Disassembler {
 public:
  explicit DisassemblerElf32(const void* start, size_t length);

  virtual ExecutableType kind() = 0;

  virtual e_machine_values ElfEM() = 0;

  // Returns 'true' if the buffer appears to point to a valid ELF executable
  // for 32 bit. If ParseHeader() succeeds, other member
  // functions may be called.
  virtual bool ParseHeader();

  virtual bool Disassemble(AssemblyProgram* target);

  // Public for unittests only
  std::vector<RVA> &Abs32Locations() { return abs32_locations_; }
  std::vector<RVA> &Rel32Locations() { return rel32_locations_; }

 protected:

  uint32 DiscoverLength();

  // Misc Section Helpers

  Elf32_Half SectionHeaderCount() const {
    return section_header_table_size_;
  }

  const Elf32_Shdr *SectionHeader(int id) const {
    assert(id >= 0 && id < SectionHeaderCount());
    return section_header_table_ + id;
  }

  const uint8 *SectionBody(int id) const {
    return OffsetToPointer(SectionHeader(id)->sh_offset);
  }

  Elf32_Word SectionBodySize(int id) const {
    return SectionHeader(id)->sh_size;
  }

  // Misc Segment Helpers

  Elf32_Half ProgramSegmentHeaderCount() const {
    return program_header_table_size_;
  }

  const Elf32_Phdr *ProgramSegmentHeader(int id) const {
    assert(id >= 0 && id < ProgramSegmentHeaderCount());
    return program_header_table_ + id;
  }

  // The virtual memory address at which this program segment will be loaded
  Elf32_Addr ProgramSegmentMemoryBegin(int id) const {
    return ProgramSegmentHeader(id)->p_vaddr;
  }

  // The number of virtual memory bytes for this program segment
  Elf32_Word ProgramSegmentMemorySize(int id) const {
    return ProgramSegmentHeader(id)->p_memsz;
  }

  // Pointer into the source file for this program segment
  Elf32_Addr ProgramSegmentFileOffset(int id) const {
    return ProgramSegmentHeader(id)->p_offset;
  }

  // Number of file bytes for this program segment. Is <= ProgramMemorySize.
  Elf32_Word ProgramSegmentFileSize(int id) const {
    return ProgramSegmentHeader(id)->p_filesz;
  }

  // Misc address space helpers

  CheckBool IsValidRVA(RVA rva) const WARN_UNUSED_RESULT;

  // Convert an ELF relocation struction into an RVA
  virtual CheckBool RelToRVA(Elf32_Rel rel, RVA* result)
    const WARN_UNUSED_RESULT = 0;

  // Returns kNoOffset if there is no file offset corresponding to 'rva'.
  CheckBool RVAToFileOffset(RVA rva, size_t* result) const WARN_UNUSED_RESULT;

  RVA FileOffsetToRVA(size_t offset) const WARN_UNUSED_RESULT;

  CheckBool RVAsToOffsets(std::vector<RVA>* rvas /*in*/,
                          std::vector<size_t>* offsets /*out*/);

  // Parsing Code used to really implement Disassemble

  CheckBool ParseFile(AssemblyProgram* target) WARN_UNUSED_RESULT;
  virtual CheckBool ParseRelocationSection(
      const Elf32_Shdr *section_header,
        AssemblyProgram* program) WARN_UNUSED_RESULT = 0;
  CheckBool ParseProgbitsSection(
      const Elf32_Shdr *section_header,
      std::vector<size_t>::iterator* current_abs_offset,
      std::vector<size_t>::iterator end_abs_offset,
      std::vector<size_t>::iterator* current_rel_offset,
      std::vector<size_t>::iterator end_rel_offset,
      AssemblyProgram* program) WARN_UNUSED_RESULT;
  CheckBool ParseSimpleRegion(size_t start_file_offset,
                              size_t end_file_offset,
                              AssemblyProgram* program) WARN_UNUSED_RESULT;

  CheckBool ParseAbs32Relocs() WARN_UNUSED_RESULT;
  CheckBool CheckSection(RVA rva) WARN_UNUSED_RESULT;
  CheckBool ParseRel32RelocsFromSections() WARN_UNUSED_RESULT;
  virtual CheckBool ParseRel32RelocsFromSection(
      const Elf32_Shdr* section) WARN_UNUSED_RESULT = 0;

  Elf32_Ehdr *header_;
  Elf32_Shdr *section_header_table_;
  Elf32_Half section_header_table_size_;

  Elf32_Phdr *program_header_table_;
  Elf32_Half program_header_table_size_;

  // Section header for default
  const char *default_string_section_;

  std::vector<RVA> abs32_locations_;
  std::vector<RVA> rel32_locations_;

  DISALLOW_COPY_AND_ASSIGN(DisassemblerElf32);
};

}  // namespace courgette

#endif  // COURGETTE_DISASSEMBLER_ELF_32_H_
