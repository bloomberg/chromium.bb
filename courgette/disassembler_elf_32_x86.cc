// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
  : DisassemblerElf32(start, length) {
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
    if (!program->EmitElfRelocationInstruction())
      return false;
    file_offset += sizeof(Elf32_Rel) * abs32_locations_.size();
  }

  return ParseSimpleRegion(file_offset, section_end, program);
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

    if (p + 5 <= end_pointer) {
      if (*p == 0xE8 || *p == 0xE9) {  // jmp rel32 and call rel32
        rel32 = p + 1;
      }
    }
    if (p + 6 <= end_pointer) {
      if (*p == 0x0F  &&  (*(p+1) & 0xF0) == 0x80) {  // Jcc long form
        if (p[1] != 0x8A && p[1] != 0x8B)  // JPE/JPO unlikely
          rel32 = p + 2;
      }
    }
    if (rel32) {
      RVA rva = static_cast<RVA>(rel32 - adjust_pointer_to_rva);
      TypedRVAX86* rel32_rva = new TypedRVAX86(rva);

      if (!rel32_rva->ComputeRelativeTarget(rel32)) {
        return false;
      }

      RVA target_rva = rel32_rva->rva() + rel32_rva->relative_target();
      // To be valid, rel32 target must be within image, and within this
      // section.
      if (IsValidRVA(target_rva)) {
        rel32_locations_.push_back(rel32_rva);
#if COURGETTE_HISTOGRAM_TARGETS
        ++rel32_target_rvas_[target_rva];
#endif
        p = rel32 + 4;
        continue;
      } else {
        delete rel32_rva;
      }
    }
    p += 1;
  }

  return true;
}

}  // namespace courgette
