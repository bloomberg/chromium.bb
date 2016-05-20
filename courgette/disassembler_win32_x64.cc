// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/disassembler_win32_x64.h"

#include <algorithm>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "courgette/assembly_program.h"
#include "courgette/courgette.h"

#if COURGETTE_HISTOGRAM_TARGETS
#include <iostream>
#endif

namespace courgette {

DisassemblerWin32X64::DisassemblerWin32X64(const void* start, size_t length)
    : DisassemblerWin32(start, length) {}

RVA DisassemblerWin32X64::PointerToTargetRVA(const uint8_t* p) const {
  return Address64ToRVA(Read64LittleEndian(p));
}

RVA DisassemblerWin32X64::Address64ToRVA(uint64_t address) const {
  if (address < image_base() || address >= image_base() + size_of_image_)
    return kNoRVA;
  return base::checked_cast<RVA>(address - image_base());
}

CheckBool DisassemblerWin32X64::EmitAbs(Label* label,
                                        AssemblyProgram* program) {
  return program->EmitAbs64(label);
}

void DisassemblerWin32X64::ParseRel32RelocsFromSection(const Section* section) {
  // TODO(sra): use characteristic.
  bool isCode = strcmp(section->name, ".text") == 0;
  if (!isCode)
    return;

  FileOffset start_file_offset = section->file_offset_of_raw_data;
  FileOffset end_file_offset = start_file_offset + section->size_of_raw_data;
  RVA relocs_start_rva = base_relocation_table().address_;

  const uint8_t* start_pointer = FileOffsetToPointer(start_file_offset);
  const uint8_t* end_pointer = FileOffsetToPointer(end_file_offset);

  RVA start_rva = FileOffsetToRVA(start_file_offset);
  RVA end_rva = start_rva + section->virtual_size;

  // Quick way to convert from Pointer to RVA within a single Section is to
  // subtract |pointer_to_rva|.
  const uint8_t* const adjust_pointer_to_rva = start_pointer - start_rva;

  std::vector<RVA>::iterator abs32_pos = abs32_locations_.begin();

  // Find the rel32 relocations.
  const uint8_t* p = start_pointer;
  while (p < end_pointer) {
    RVA current_rva = static_cast<RVA>(p - adjust_pointer_to_rva);
    if (current_rva == relocs_start_rva) {
      uint32_t relocs_size = base_relocation_table().size_;
      if (relocs_size) {
        p += relocs_size;
        continue;
      }
    }

    // Heuristic discovery of rel32 locations in instruction stream: are the
    // next few bytes the start of an instruction containing a rel32
    // addressing mode?
    const uint8_t* rel32 = nullptr;
    bool is_rip_relative = false;

    if (p + 5 <= end_pointer) {
      if (*p == 0xE8 || *p == 0xE9)  // jmp rel32 and call rel32
        rel32 = p + 1;
    }
    if (p + 6 <= end_pointer) {
      if (*p == 0x0F && (*(p + 1) & 0xF0) == 0x80) {  // Jcc long form
        if (p[1] != 0x8A && p[1] != 0x8B)  // JPE/JPO unlikely
          rel32 = p + 2;
      } else if (*p == 0xFF && (*(p + 1) == 0x15 || *(p + 1) == 0x25)) {
        // rip relative call/jmp
        rel32 = p + 2;
        is_rip_relative = true;
      }
    }
    if (p + 7 <= end_pointer) {
      if ((*p & 0xFB) == 0x48 && *(p + 1) == 0x8D &&
          (*(p + 2) & 0xC7) == 0x05) {
        // rip relative lea
        rel32 = p + 3;
        is_rip_relative = true;
      } else if ((*p & 0xFB) == 0x48 && *(p + 1) == 0x8B &&
                 (*(p + 2) & 0xC7) == 0x05) {
        // rip relative mov
        rel32 = p + 3;
        is_rip_relative = true;
      }
    }

    if (rel32) {
      RVA rel32_rva = static_cast<RVA>(rel32 - adjust_pointer_to_rva);

      // Is there an abs32 reloc overlapping the candidate?
      while (abs32_pos != abs32_locations_.end() && *abs32_pos < rel32_rva - 3)
        ++abs32_pos;
      // Now: (*abs32_pos > rel32_rva - 4) i.e. the lowest addressed 4-byte
      // region that could overlap rel32_rva.
      if (abs32_pos != abs32_locations_.end()) {
        if (*abs32_pos < rel32_rva + 4) {
          // Beginning of abs32 reloc is before end of rel32 reloc so they
          // overlap. Skip four bytes past the abs32 reloc.
          p += (*abs32_pos + 4) - current_rva;
          continue;
        }
      }

      // + 4 since offset is relative to start of next instruction.
      RVA target_rva = rel32_rva + 4 + Read32LittleEndian(rel32);
      // To be valid, rel32 target must be within image, and within this
      // section.
      if (target_rva < size_of_image_ &&  // Subsumes rva != kUnassignedRVA.
          (is_rip_relative ||
           (start_rva <= target_rva && target_rva < end_rva))) {
        rel32_locations_.push_back(rel32_rva);
#if COURGETTE_HISTOGRAM_TARGETS
        ++rel32_target_rvas_[target_rva];
#endif
        p = rel32 + 4;
        continue;
      }
    }
    p += 1;
  }
}

}  // namespace courgette
