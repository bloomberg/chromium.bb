// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/disassembler_elf_32.h"

#include <algorithm>

#include "base/logging.h"
#include "courgette/assembly_program.h"
#include "courgette/courgette.h"

namespace courgette {

namespace {

// Sorts |section_headers| by file offset and stores the resulting permutation
// of section ids in |order|.
std::vector<Elf32_Half> GetSectionHeaderFileOffsetOrder(
    const std::vector<Elf32_Shdr>& section_headers) {
  size_t size = section_headers.size();
  std::vector<Elf32_Half> order(size);
  for (size_t i = 0; i < size; ++i)
    order[i] = static_cast<Elf32_Half>(i);

  auto comp = [&](int idx1, int idx2) {
    return section_headers[idx1].sh_offset < section_headers[idx2].sh_offset;
  };
  std::stable_sort(order.begin(), order.end(), comp);
  return order;
}

}  // namespace

DisassemblerElf32::DisassemblerElf32(const void* start, size_t length)
    : Disassembler(start, length),
      header_(nullptr),
      section_header_table_size_(0),
      program_header_table_(nullptr),
      program_header_table_size_(0),
      default_string_section_(nullptr) {
}

RVA DisassemblerElf32::FileOffsetToRVA(FileOffset offset) const {
  // File offsets can be 64-bit values, but we are dealing with 32-bit
  // executables and so only need to support 32-bit file sizes.
  uint32_t offset32 = static_cast<uint32_t>(offset);

  // Visit section headers ordered by file offset.
  for (Elf32_Half section_id : section_header_file_offset_order_) {
    const Elf32_Shdr* section_header = SectionHeader(section_id);
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

FileOffset DisassemblerElf32::RVAToFileOffset(RVA rva) const {
  for (Elf32_Half section_id = 0; section_id < SectionHeaderCount();
       ++section_id) {
    const Elf32_Shdr* section_header = SectionHeader(section_id);
    // These can appear to have a size in the file, but don't.
    if (section_header->sh_type == SHT_NOBITS)
      continue;
    Elf32_Addr begin = section_header->sh_addr;
    Elf32_Addr end = begin + section_header->sh_size;

    if (rva >= begin && rva < end)
      return section_header->sh_offset + (rva - begin);
  }
  return kNoFileOffset;
}

RVA DisassemblerElf32::PointerToTargetRVA(const uint8_t* p) const {
  // TODO(huangs): Add check (e.g., IsValidTargetRVA(), but more efficient).
  return Read32LittleEndian(p);
}

bool DisassemblerElf32::ParseHeader() {
  if (length() < sizeof(Elf32_Ehdr))
    return Bad("Too small");

  header_ = reinterpret_cast<const Elf32_Ehdr*>(start());

  // Have magic for ELF header?
  if (header_->e_ident[0] != 0x7f ||
      header_->e_ident[1] != 'E' ||
      header_->e_ident[2] != 'L' ||
      header_->e_ident[3] != 'F')
    return Bad("No Magic Number");

  if (header_->e_type != ET_EXEC &&
      header_->e_type != ET_DYN)
    return Bad("Not an executable file or shared library");

  if (header_->e_machine != ElfEM())
    return Bad("Not a supported architecture");

  if (header_->e_version != 1)
    return Bad("Unknown file version");

  if (header_->e_shentsize != sizeof(Elf32_Shdr))
    return Bad("Unexpected section header size");

  if (!IsArrayInBounds(header_->e_shoff, header_->e_shnum, sizeof(Elf32_Shdr)))
    return Bad("Out of bounds section header table");

  // Extract |section_header_table_|, ordered by section id.
  const Elf32_Shdr* section_header_table_raw =
      reinterpret_cast<const Elf32_Shdr*>(
          FileOffsetToPointer(header_->e_shoff));
  section_header_table_size_ = header_->e_shnum;
  section_header_table_.assign(section_header_table_raw,
      section_header_table_raw + section_header_table_size_);

  section_header_file_offset_order_ =
      GetSectionHeaderFileOffsetOrder(section_header_table_);

  if (!IsArrayInBounds(header_->e_phoff, header_->e_phnum, sizeof(Elf32_Phdr)))
    return Bad("Out of bounds program header table");

  program_header_table_ = reinterpret_cast<const Elf32_Phdr*>(
      FileOffsetToPointer(header_->e_phoff));
  program_header_table_size_ = header_->e_phnum;

  if (header_->e_shstrndx >= header_->e_shnum)
    return Bad("Out of bounds string section index");

  default_string_section_ = reinterpret_cast<const char*>(
      SectionBody(static_cast<int>(header_->e_shstrndx)));

  if (!UpdateLength())
    return Bad("Out of bounds section or segment");

  return Good();
}

bool DisassemblerElf32::Disassemble(AssemblyProgram* target) {
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

bool DisassemblerElf32::UpdateLength() {
  Elf32_Off result = 0;

  // Find the end of the last section
  for (Elf32_Half section_id = 0; section_id < SectionHeaderCount();
       ++section_id) {
    const Elf32_Shdr* section_header = SectionHeader(section_id);

    if (section_header->sh_type == SHT_NOBITS)
      continue;

    if (!IsArrayInBounds(section_header->sh_offset, section_header->sh_size, 1))
      return false;

    Elf32_Off section_end = section_header->sh_offset + section_header->sh_size;
    result = std::max(result, section_end);
  }

  // Find the end of the last segment
  for (Elf32_Half segment_id = 0; segment_id < ProgramSegmentHeaderCount();
       ++segment_id) {
    const Elf32_Phdr* segment_header = ProgramSegmentHeader(segment_id);

    if (!IsArrayInBounds(segment_header->p_offset, segment_header->p_filesz, 1))
      return false;

    Elf32_Off segment_end = segment_header->p_offset + segment_header->p_filesz;
    result = std::max(result, segment_end);
  }

  Elf32_Off section_table_end =
      header_->e_shoff + (header_->e_shnum * sizeof(Elf32_Shdr));
  result = std::max(result, section_table_end);

  Elf32_Off segment_table_end =
      header_->e_phoff + (header_->e_phnum * sizeof(Elf32_Phdr));
  result = std::max(result, segment_table_end);

  ReduceLength(result);
  return true;
}

CheckBool DisassemblerElf32::IsValidTargetRVA(RVA rva) const {
  if (rva == kUnassignedRVA)
    return false;

  // It's valid if it's contained in any program segment
  for (Elf32_Half segment_id = 0; segment_id < ProgramSegmentHeaderCount();
       ++segment_id) {
    const Elf32_Phdr* segment_header = ProgramSegmentHeader(segment_id);

    if (segment_header->p_type != PT_LOAD)
      continue;

    Elf32_Addr begin = segment_header->p_vaddr;
    Elf32_Addr end = segment_header->p_vaddr + segment_header->p_memsz;

    if (rva >= begin && rva < end)
      return true;
  }

  return false;
}

CheckBool DisassemblerElf32::RVAsToFileOffsets(
    const std::vector<RVA>& rvas,
    std::vector<FileOffset>* file_offsets) {
  file_offsets->clear();
  for (RVA rva : rvas) {
    FileOffset file_offset = RVAToFileOffset(rva);
    if (file_offset == kNoFileOffset)
      return false;
    file_offsets->push_back(file_offset);
  }
  return true;
}

CheckBool DisassemblerElf32::RVAsToFileOffsets(
    ScopedVector<TypedRVA>* typed_rvas) {
  for (TypedRVA* typed_rva : *typed_rvas) {
    FileOffset file_offset = RVAToFileOffset(typed_rva->rva());
    if (file_offset == kNoFileOffset)
      return false;
    typed_rva->set_file_offset(file_offset);
  }
  return true;
}

CheckBool DisassemblerElf32::ParseFile(AssemblyProgram* program) {
  // Walk all the bytes in the file, whether or not in a section.
  FileOffset file_offset = 0;

  std::vector<FileOffset> abs_offsets;

  if (!RVAsToFileOffsets(abs32_locations_, &abs_offsets))
    return false;

  if (!RVAsToFileOffsets(&rel32_locations_))
    return false;

  std::vector<FileOffset>::iterator current_abs_offset = abs_offsets.begin();
  ScopedVector<TypedRVA>::iterator current_rel = rel32_locations_.begin();

  std::vector<FileOffset>::iterator end_abs_offset = abs_offsets.end();
  ScopedVector<TypedRVA>::iterator end_rel = rel32_locations_.end();

  // Visit section headers ordered by file offset.
  for (Elf32_Half section_id : section_header_file_offset_order_) {
    const Elf32_Shdr* section_header = SectionHeader(section_id);

    if (section_header->sh_type == SHT_NOBITS)
      continue;

    if (!ParseSimpleRegion(file_offset, section_header->sh_offset, program))
      return false;

    file_offset = section_header->sh_offset;

    switch (section_header->sh_type) {
      case SHT_REL:
        if (!ParseRelocationSection(section_header, program))
          return false;
        file_offset = section_header->sh_offset + section_header->sh_size;
        break;
      case SHT_PROGBITS:
        if (!ParseProgbitsSection(section_header,
                                  &current_abs_offset,
                                  end_abs_offset,
                                  &current_rel,
                                  end_rel,
                                  program)) {
          return false;
        }
        file_offset = section_header->sh_offset + section_header->sh_size;
        break;
      case SHT_INIT_ARRAY:
        // Fall through
      case SHT_FINI_ARRAY:
        while (current_abs_offset != end_abs_offset &&
               *current_abs_offset >= section_header->sh_offset &&
               *current_abs_offset <
                   section_header->sh_offset + section_header->sh_size) {
          // Skip any abs_offsets appear in the unsupported INIT_ARRAY section
          VLOG(1) << "Skipping relocation entry for unsupported section: "
                  << section_header->sh_type;
          ++current_abs_offset;
        }
        break;
      default:
        if (current_abs_offset != end_abs_offset &&
            *current_abs_offset >= section_header->sh_offset &&
            *current_abs_offset <
                section_header->sh_offset + section_header->sh_size) {
          VLOG(1) << "Relocation address in unrecognized ELF section: "
                  << section_header->sh_type;
        }
        break;
    }
  }

  // Rest of the file past the last section
  if (!ParseSimpleRegion(file_offset, length(), program))
    return false;

  // Make certain we consume all of the relocations as expected
  return (current_abs_offset == end_abs_offset);
}

CheckBool DisassemblerElf32::ParseProgbitsSection(
    const Elf32_Shdr* section_header,
    std::vector<FileOffset>::iterator* current_abs_offset,
    std::vector<FileOffset>::iterator end_abs_offset,
    ScopedVector<TypedRVA>::iterator* current_rel,
    ScopedVector<TypedRVA>::iterator end_rel,
    AssemblyProgram* program) {
  // Walk all the bytes in the file, whether or not in a section.
  FileOffset file_offset = section_header->sh_offset;
  FileOffset section_end = section_header->sh_offset + section_header->sh_size;

  Elf32_Addr origin = section_header->sh_addr;
  FileOffset origin_offset = section_header->sh_offset;
  if (!program->EmitOriginInstruction(origin))
    return false;

  while (file_offset < section_end) {
    if (*current_abs_offset != end_abs_offset &&
        file_offset > **current_abs_offset)
      return false;

    while (*current_rel != end_rel &&
           file_offset > (**current_rel)->file_offset()) {
      ++(*current_rel);
    }

    FileOffset next_relocation = section_end;

    if (*current_abs_offset != end_abs_offset &&
        next_relocation > **current_abs_offset)
      next_relocation = **current_abs_offset;

    // Rel offsets are heuristically derived, and might (incorrectly) overlap
    // an Abs value, or the end of the section, so +3 to make sure there is
    // room for the full 4 byte value.
    if (*current_rel != end_rel &&
        next_relocation > ((**current_rel)->file_offset() + 3))
      next_relocation = (**current_rel)->file_offset();

    if (next_relocation > file_offset) {
      if (!ParseSimpleRegion(file_offset, next_relocation, program))
        return false;

      file_offset = next_relocation;
      continue;
    }

    if (*current_abs_offset != end_abs_offset &&
        file_offset == **current_abs_offset) {
      RVA target_rva = PointerToTargetRVA(FileOffsetToPointer(file_offset));
      DCHECK_NE(kNoRVA, target_rva);

      if (!program->EmitAbs32(program->FindOrMakeAbs32Label(target_rva)))
        return false;
      file_offset += sizeof(RVA);
      ++(*current_abs_offset);
      continue;
    }

    if (*current_rel != end_rel &&
        file_offset == (**current_rel)->file_offset()) {
      uint32_t relative_target = (**current_rel)->relative_target();
      // This cast is for 64 bit systems, and is only safe because we
      // are working on 32 bit executables.
      RVA target_rva = (RVA)(origin + (file_offset - origin_offset) +
                             relative_target);

      if (!(**current_rel)->EmitInstruction(program, target_rva))
        return false;
      file_offset += (**current_rel)->op_size();
      ++(*current_rel);
      continue;
    }
  }

  // Rest of the section (if any)
  return ParseSimpleRegion(file_offset, section_end, program);
}

CheckBool DisassemblerElf32::ParseSimpleRegion(FileOffset start_file_offset,
                                               FileOffset end_file_offset,
                                               AssemblyProgram* program) {
  // Callers don't guarantee start < end
  if (start_file_offset >= end_file_offset)
    return true;

  const size_t len = end_file_offset - start_file_offset;

  if (!program->EmitBytesInstruction(FileOffsetToPointer(start_file_offset),
                                     len)) {
    return false;
  }

  return true;
}

CheckBool DisassemblerElf32::ParseAbs32Relocs() {
  abs32_locations_.clear();

  // Loop through sections for relocation sections
  for (Elf32_Half section_id = 0; section_id < SectionHeaderCount();
       ++section_id) {
    const Elf32_Shdr* section_header = SectionHeader(section_id);

    if (section_header->sh_type == SHT_REL) {
      const Elf32_Rel* relocs_table =
          reinterpret_cast<const Elf32_Rel*>(SectionBody(section_id));

      int relocs_table_count = section_header->sh_size /
                               section_header->sh_entsize;

      // Elf32_Word relocation_section_id = section_header->sh_info;

      // Loop through relocation objects in the relocation section
      for (int rel_id = 0; rel_id < relocs_table_count; ++rel_id) {
        RVA rva;

        // Quite a few of these conversions fail, and we simply skip
        // them, that's okay.
        if (RelToRVA(relocs_table[rel_id], &rva) && CheckSection(rva))
          abs32_locations_.push_back(rva);
      }
    }
  }

  std::sort(abs32_locations_.begin(), abs32_locations_.end());
  DCHECK(abs32_locations_.empty() ||
         abs32_locations_.back() != kUnassignedRVA);
  return true;
}

CheckBool DisassemblerElf32::CheckSection(RVA rva) {
  FileOffset file_offset = RVAToFileOffset(rva);
  if (file_offset == kNoFileOffset)
    return false;

  for (Elf32_Half section_id = 0; section_id < SectionHeaderCount();
       ++section_id) {
    const Elf32_Shdr* section_header = SectionHeader(section_id);

    if (file_offset >= section_header->sh_offset &&
        file_offset < (section_header->sh_offset + section_header->sh_size)) {
      switch (section_header->sh_type) {
        case SHT_REL:  // Falls through.
        case SHT_PROGBITS:
          return true;
      }
    }
  }

  return false;
}

CheckBool DisassemblerElf32::ParseRel32RelocsFromSections() {
  rel32_locations_.clear();

  // Loop through sections for relocation sections
  for (Elf32_Half section_id = 0; section_id < SectionHeaderCount();
       ++section_id) {
    const Elf32_Shdr* section_header = SectionHeader(section_id);

    // TODO(huangs): Add better checks to skip non-code sections.
    // Some debug sections can have sh_type=SHT_PROGBITS but sh_addr=0.
    if (section_header->sh_type != SHT_PROGBITS ||
        section_header->sh_addr == 0)
      continue;

    if (!ParseRel32RelocsFromSection(section_header))
      return false;
  }

  std::sort(rel32_locations_.begin(),
            rel32_locations_.end(),
            TypedRVA::IsLessThan);
  DCHECK(rel32_locations_.empty() ||
         rel32_locations_.back()->rva() != kUnassignedRVA);
  return true;
}

}  // namespace courgette
