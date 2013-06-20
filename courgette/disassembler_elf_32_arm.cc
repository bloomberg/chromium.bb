// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/disassembler_elf_32_arm.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"

#include "courgette/assembly_program.h"
#include "courgette/courgette.h"
#include "courgette/encoded_program.h"

namespace courgette {

DisassemblerElf32ARM::DisassemblerElf32ARM(const void* start, size_t length)
  : DisassemblerElf32(start, length) {
}

// Convert an ELF relocation struction into an RVA
CheckBool DisassemblerElf32ARM::RelToRVA(Elf32_Rel rel, RVA* result) const {

  // The rightmost byte of r_info is the type...
  elf32_rel_arm_type_values type =
      (elf32_rel_arm_type_values)(unsigned char)rel.r_info;

  // The other 3 bytes of r_info are the symbol
  uint32 symbol =  rel.r_info >> 8;

  switch(type)
  {
    case R_ARM_RELATIVE:
      if (symbol != 0)
        return false;

      // This is a basic ABS32 relocation address
      *result = rel.r_offset;
      return true;

    default:
      return false;
  }

  return false;
}

CheckBool DisassemblerElf32ARM::ParseRelocationSection(
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
    if (section_relocs_iter->r_info != R_ARM_RELATIVE ||
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

CheckBool DisassemblerElf32ARM::ParseRel32RelocsFromSection(
    const Elf32_Shdr* section_header) {
  // TODO(paulgazz) find relative jumps in ARM assembly
  return true;
}

}  // namespace courgette
