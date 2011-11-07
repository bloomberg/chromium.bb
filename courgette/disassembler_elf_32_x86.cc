// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/disassembler_elf_32_x86.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"

#include "courgette/assembly_program.h"
#include "courgette/courgette.h"
#include "courgette/encoded_program.h"

namespace courgette {

DisassemblerElf32X86::DisassemblerElf32X86(const void* start, size_t length)
  : Disassembler(start, length) {
}

bool DisassemblerElf32X86::ParseHeader() {
  if (length() < sizeof(Elf32_Ehdr))
    return Bad("Too small");

  header_ = (Elf32_Ehdr *)start();

  // Have magic for elf header?
  if (header_->e_ident[0] != 0x7f ||
      header_->e_ident[1] != 'E' ||
      header_->e_ident[2] != 'L' ||
      header_->e_ident[3] != 'F')
    return Bad("No Magic Number");

  if (header_->e_type != ET_EXEC &&
      header_->e_type != ET_DYN)
    return Bad("Not an executable file or shared library");

  if (header_->e_machine != EM_386)
    return Bad("Not a supported architecture");

  if (header_->e_version != 1)
    return Bad("Unknown file version");

  if (header_->e_shentsize != sizeof(Elf32_Shdr))
    return Bad("Unexpected section header size");

  if (header_->e_shoff >= length())
    return Bad("Out of bounds section header table offset");

  section_header_table_ = (Elf32_Shdr *)OffsetToPointer(header_->e_shoff);
  section_header_table_size_ = header_->e_shnum;

  if ((header_->e_shoff + header_->e_shnum ) >= length())
    return Bad("Out of bounds section header table");

  if (header_->e_phoff >= length())
    return Bad("Out of bounds program header table offset");

  program_header_table_ = (Elf32_Phdr *)OffsetToPointer(header_->e_phoff);
  program_header_table_size_ = header_->e_phnum;

  if ((header_->e_phoff + header_->e_phnum) >= length())
    return Bad("Out of bounds program header table");

  default_string_section_ = (const char *)SectionBody((int)header_->e_shstrndx);

  ReduceLength(DiscoverLength());

  return Good();
}

bool DisassemblerElf32X86::Disassemble(AssemblyProgram* target) {
  if (!ok())
    return false;

  // The Image Base is always 0 for ELF Executables
  target->set_image_base(0);

  if (!ParseAbs32Relocs())
    return false;

  if (!ParseRel32RelocsFromSections())
    return false;

  if (!ParseFile(target))
    return false;

  target->DefaultAssignIndexes();

  return true;
}

uint32 DisassemblerElf32X86::DiscoverLength() {
  uint32 result = 0;

  // Find the end of the last section
  for (int section_id = 0; section_id < SectionHeaderCount(); section_id++) {
    const Elf32_Shdr *section_header = SectionHeader(section_id);

    if (section_header->sh_type == SHT_NOBITS)
      continue;

    uint32 section_end = section_header->sh_offset + section_header->sh_size;

    if (section_end > result)
      result = section_end;
  }

  // Find the end of the last segment
  for (int i = 0; i < ProgramSegmentHeaderCount(); i++) {
    const Elf32_Phdr *segment_header = ProgramSegmentHeader(i);

    uint32 segment_end = segment_header->p_offset + segment_header->p_filesz;

    if (segment_end > result)
      result = segment_end;
  }

  uint32 section_table_end = header_->e_shoff +
                             (header_->e_shnum * sizeof(Elf32_Shdr));
  if (section_table_end > result)
    result = section_table_end;

  uint32 segment_table_end = header_->e_phoff +
                             (header_->e_phnum * sizeof(Elf32_Phdr));
  if (segment_table_end > result)
    result = segment_table_end;

  return result;
}

CheckBool DisassemblerElf32X86::IsValidRVA(RVA rva) const {

  // It's valid if it's contained in any program segment
  for (int i = 0; i < ProgramSegmentHeaderCount(); i++) {
    const Elf32_Phdr *segment_header = ProgramSegmentHeader(i);

    if (segment_header->p_type != PT_LOAD)
      continue;

    Elf32_Addr begin = segment_header->p_vaddr;
    Elf32_Addr end = segment_header->p_vaddr + segment_header->p_memsz;

    if (rva >= begin && rva < end)
      return true;
  }

  return false;
}

// Convert an ELF relocation struction into an RVA
CheckBool DisassemblerElf32X86::RelToRVA(Elf32_Rel rel, RVA* result) const {

  // The rightmost byte of r_info is the type...
  elf32_rel_386_type_values type =
      (elf32_rel_386_type_values)(unsigned char)rel.r_info;

  // The other 3 bytes of r_info are the symbol
  uint32 symbol =  rel.r_info >> 8;

  switch(type)
  {
    case R_386_NONE:
    case R_386_32:
    case R_386_PC32:
    case R_386_GOT32:
    case R_386_PLT32:
    case R_386_COPY:
    case R_386_GLOB_DAT:
    case R_386_JMP_SLOT:
      return false;

    case R_386_RELATIVE:
      if (symbol != 0)
        return false;

      // This is a basic ABS32 relocation address
      *result = rel.r_offset;
      return true;

    case R_386_GOTOFF:
    case R_386_GOTPC:
    case R_386_TLS_TPOFF:
      return false;
  }

  return false;
}

// Returns RVA for an in memory address, or NULL.
CheckBool DisassemblerElf32X86::RVAToFileOffset(Elf32_Addr addr,
                                                size_t* result) const {

  for (int i = 0; i < ProgramSegmentHeaderCount(); i++) {
    Elf32_Addr begin = ProgramSegmentMemoryBegin(i);
    Elf32_Addr end = begin + ProgramSegmentMemorySize(i);

    if (addr >= begin  && addr < end) {
      Elf32_Addr offset = addr - begin;

      if (offset < ProgramSegmentFileSize(i)) {
        *result = ProgramSegmentFileOffset(i) + offset;
        return true;
      }
    }
  }

  return false;
}

RVA DisassemblerElf32X86::FileOffsetToRVA(size_t offset) const {
  // File offsets can be 64 bit values, but we are dealing with 32
  // bit executables and so only need to support 32bit file sizes.
  uint32 offset32 = (uint32)offset;

  for (int i = 0; i < SectionHeaderCount(); i++) {

    const Elf32_Shdr *section_header = SectionHeader(i);

    // These can appear to have a size in the file, but don't.
    if (section_header->sh_type == SHT_NOBITS)
      continue;

    Elf32_Off section_begin = section_header->sh_offset;
    Elf32_Off section_end = section_begin + section_header->sh_size;

    if (offset32 >= section_begin && offset32 < section_end) {
      return section_header->sh_addr + (offset32 - section_begin);
    }
  }

  return 0;
}

CheckBool DisassemblerElf32X86::RVAsToOffsets(std::vector<RVA>* rvas,
                                              std::vector<size_t>* offsets) {
  offsets->clear();

  for (std::vector<RVA>::iterator rva = rvas->begin();
       rva != rvas->end();
       rva++) {

    size_t offset;

    if (!RVAToFileOffset(*rva, &offset))
      return false;

    offsets->push_back(offset);
  }

  return true;
}

CheckBool DisassemblerElf32X86::ParseFile(AssemblyProgram* program) {
  bool ok = true;

  // Walk all the bytes in the file, whether or not in a section.
  uint32 file_offset = 0;

  std::vector<size_t> abs_offsets;
  std::vector<size_t> rel_offsets;

  if (ok)
    ok = RVAsToOffsets(&abs32_locations_, &abs_offsets);

  if (ok)
    ok = RVAsToOffsets(&rel32_locations_, &rel_offsets);

  std::vector<size_t>::iterator current_abs_offset = abs_offsets.begin();
  std::vector<size_t>::iterator current_rel_offset = rel_offsets.begin();

  std::vector<size_t>::iterator end_abs_offset = abs_offsets.end();
  std::vector<size_t>::iterator end_rel_offset = rel_offsets.end();

  for (int section_id = 0;
       ok && (section_id < SectionHeaderCount());
       section_id++) {

    const Elf32_Shdr *section_header = SectionHeader(section_id);

    if (ok) {
      ok = ParseSimpleRegion(file_offset,
                             section_header->sh_offset,
                             program);
      file_offset = section_header->sh_offset;
    }

    switch (section_header->sh_type) {
      case SHT_REL:
        if (ok) {
          ok = ParseRelocationSection(section_header, program);
          file_offset = section_header->sh_offset + section_header->sh_size;
        }
        break;
      case SHT_PROGBITS:
        if (ok) {
          ok = ParseProgbitsSection(section_header,
                                    &current_abs_offset, end_abs_offset,
                                    &current_rel_offset, end_rel_offset,
                                    program);
          file_offset = section_header->sh_offset + section_header->sh_size;
        }

        break;
      default:
        break;
    }
  }

  // Rest of the file past the last section
  if (ok) {
    ok = ParseSimpleRegion(file_offset,
                           length(),
                           program);
  }

  // Make certain we consume all of the relocations as expected
  ok = ok && (current_abs_offset == end_abs_offset);

  return ok;
}

CheckBool DisassemblerElf32X86::ParseRelocationSection(
    const Elf32_Shdr *section_header,
      AssemblyProgram* program) {
  // We can reproduce the R_386_RELATIVE entries in one of the relocation
  // table based on other information in the patch, given these
  // conditions....
  //
  // All R_386_RELATIVE entries are:
  //   1) In the same relocation table
  //   2) Are consecutive
  //   3) Are sorted in memory address order
  //
  // Happily, this is normally the case, but it's not required by spec
  // so we check, and just don't do it if we don't match up.

  // The expectation is that one relocation section will contain
  // all of our R_386_RELATIVE entries in the expected order followed
  // by assorted other entries we can't use special handling for.

  bool ok = true;
  bool match = true;

  // Walk all the bytes in the section, matching relocation table or not
  size_t file_offset = section_header->sh_offset;
  size_t section_end = section_header->sh_offset + section_header->sh_size;

  Elf32_Rel *section_relocs_iter =
      (Elf32_Rel *)OffsetToPointer(section_header->sh_offset);

  uint32 section_relocs_count = section_header->sh_size /
                                section_header->sh_entsize;

  if (abs32_locations_.size() > section_relocs_count)
    match = false;

  std::vector<RVA>::iterator reloc_iter = abs32_locations_.begin();

  while (match && (reloc_iter !=  abs32_locations_.end())) {
    if (section_relocs_iter->r_info != R_386_RELATIVE ||
        section_relocs_iter->r_offset != *reloc_iter)
      match = false;
    section_relocs_iter++;
    reloc_iter++;
  }

  if (match) {
    // Skip over relocation tables
    ok = program->EmitElfRelocationInstruction();
    file_offset += sizeof(Elf32_Rel) * abs32_locations_.size();
  }

  if (ok) {
    ok = ParseSimpleRegion(file_offset, section_end, program);
  }

  return ok;
}

CheckBool DisassemblerElf32X86::ParseProgbitsSection(
    const Elf32_Shdr *section_header,
    std::vector<size_t>::iterator* current_abs_offset,
    std::vector<size_t>::iterator end_abs_offset,
    std::vector<size_t>::iterator* current_rel_offset,
    std::vector<size_t>::iterator end_rel_offset,
    AssemblyProgram* program) {

  bool ok = true;

  // Walk all the bytes in the file, whether or not in a section.
  size_t file_offset = section_header->sh_offset;
  size_t section_end = section_header->sh_offset + section_header->sh_size;

  Elf32_Addr origin = section_header->sh_addr;
  size_t origin_offset = section_header->sh_offset;
  ok = program->EmitOriginInstruction(origin);

  while (ok && file_offset < section_end) {

    if (*current_abs_offset != end_abs_offset &&
        file_offset > **current_abs_offset) {
      ok = false;
    }

    while (*current_rel_offset != end_rel_offset &&
           file_offset > **current_rel_offset) {
      (*current_rel_offset)++;
    }

    size_t next_relocation = section_end;

    if (*current_abs_offset != end_abs_offset &&
        next_relocation > **current_abs_offset)
      next_relocation = **current_abs_offset;

    // Rel offsets are heuristically derived, and might (incorrectly) overlap
    // an Abs value, or the end of the section, so +3 to make sure there is
    // room for the full 4 byte value.
    if (*current_rel_offset != end_rel_offset &&
        next_relocation > (**current_rel_offset + 3))
      next_relocation = **current_rel_offset;

    if (ok && (next_relocation > file_offset)) {
      ok = ParseSimpleRegion(file_offset, next_relocation, program);

      file_offset = next_relocation;
      continue;
    }

    if (ok &&
        *current_abs_offset != end_abs_offset &&
        file_offset == **current_abs_offset) {

      const uint8* p = OffsetToPointer(file_offset);
      RVA target_rva = Read32LittleEndian(p);

      ok = program->EmitAbs32(program->FindOrMakeAbs32Label(target_rva));
      file_offset += sizeof(RVA);
      (*current_abs_offset)++;
      continue;
    }

    if (ok &&
        *current_rel_offset != end_rel_offset &&
        file_offset == **current_rel_offset) {

      const uint8* p = OffsetToPointer(file_offset);
      uint32 relative_target = Read32LittleEndian(p);
      // This cast is for 64 bit systems, and is only safe because we
      // are working on 32 bit executables.
      RVA target_rva = (RVA)(origin + (file_offset - origin_offset) +
                             4 + relative_target);

      ok = program->EmitRel32(program->FindOrMakeRel32Label(target_rva));
      file_offset += sizeof(RVA);
      (*current_rel_offset)++;
      continue;
    }
  }

  // Rest of the section (if any)
  if (ok) {
    ok = ParseSimpleRegion(file_offset, section_end, program);
  }

  return ok;
}

CheckBool DisassemblerElf32X86::ParseSimpleRegion(
    size_t start_file_offset,
    size_t end_file_offset,
    AssemblyProgram* program) {

  const uint8* start = OffsetToPointer(start_file_offset);
  const uint8* end = OffsetToPointer(end_file_offset);

  const uint8* p = start;

  bool ok = true;
  while (p < end && ok) {
    ok = program->EmitByteInstruction(*p);
    ++p;
  }

  return ok;
}

CheckBool DisassemblerElf32X86::ParseAbs32Relocs() {
  abs32_locations_.clear();

  // Loop through sections for relocation sections
  for (int section_id = 0; section_id < SectionHeaderCount(); section_id++) {
    const Elf32_Shdr *section_header = SectionHeader(section_id);

    if (section_header->sh_type == SHT_REL) {

      Elf32_Rel *relocs_table = (Elf32_Rel *)SectionBody(section_id);

      int relocs_table_count = section_header->sh_size /
                               section_header->sh_entsize;

      // Elf32_Word relocation_section_id = section_header->sh_info;

      // Loop through relocation objects in the relocation section
      for (int rel_id = 0; rel_id < relocs_table_count; rel_id++) {
        RVA rva;

        // Quite a few of these conversions fail, and we simply skip
        // them, that's okay.
        if (RelToRVA(relocs_table[rel_id], &rva))
          abs32_locations_.push_back(rva);
      }
    }
  }

  std::sort(abs32_locations_.begin(), abs32_locations_.end());
  return true;
}

CheckBool DisassemblerElf32X86::ParseRel32RelocsFromSections() {

  rel32_locations_.clear();

  // Loop through sections for relocation sections
  for (int section_id = 0;
       section_id < SectionHeaderCount();
       section_id++) {

    const Elf32_Shdr *section_header = SectionHeader(section_id);

    if (section_header->sh_type != SHT_PROGBITS)
      continue;

    if (!ParseRel32RelocsFromSection(section_header))
      return false;
  }

  std::sort(rel32_locations_.begin(), rel32_locations_.end());
  return true;
}

CheckBool DisassemblerElf32X86::ParseRel32RelocsFromSection(
    const Elf32_Shdr* section_header) {

  uint32 start_file_offset = section_header->sh_offset;
  uint32 end_file_offset = start_file_offset + section_header->sh_size;

  const uint8* start_pointer = OffsetToPointer(start_file_offset);
  const uint8* end_pointer = OffsetToPointer(end_file_offset);

  // Quick way to convert from Pointer to RVA within a single Section is to
  // subtract 'pointer_to_rva'.
  const uint8* const adjust_pointer_to_rva = start_pointer -
                                             section_header->sh_addr;

  // Find the rel32 relocations.
  const uint8* p = start_pointer;
  while (p < end_pointer) {
    //RVA current_rva = static_cast<RVA>(p - adjust_pointer_to_rva);

    // Heuristic discovery of rel32 locations in instruction stream: are the
    // next few bytes the start of an instruction containing a rel32
    // addressing mode?
    const uint8* rel32 = NULL;

    if (p + 5 < end_pointer) {
      if (*p == 0xE8 || *p == 0xE9) {  // jmp rel32 and call rel32
        rel32 = p + 1;
      }
    }
    if (p + 6 < end_pointer) {
      if (*p == 0x0F  &&  (*(p+1) & 0xF0) == 0x80) {  // Jcc long form
        if (p[1] != 0x8A && p[1] != 0x8B)  // JPE/JPO unlikely
          rel32 = p + 2;
      }
    }
    if (rel32) {
      RVA rel32_rva = static_cast<RVA>(rel32 - adjust_pointer_to_rva);

      RVA target_rva = rel32_rva + 4 + Read32LittleEndian(rel32);
      // To be valid, rel32 target must be within image, and within this
      // section.
      if (IsValidRVA(target_rva)) {
        rel32_locations_.push_back(rel32_rva);
#if COURGETTE_HISTOGRAM_TARGETS
        ++rel32_target_rvas_[target_rva];
#endif
        p += 4;
        continue;
      }
    }
    p += 1;
  }

  return true;
}

}  // namespace courgette
