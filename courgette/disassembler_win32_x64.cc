// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/disassembler_win32_x64.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"

#include "courgette/assembly_program.h"
#include "courgette/courgette.h"
#include "courgette/encoded_program.h"

namespace courgette {

DisassemblerWin32X64::DisassemblerWin32X64(const void* start, size_t length)
  : Disassembler(start, length),
    incomplete_disassembly_(false),
    is_PE32_plus_(false),
    optional_header_(NULL),
    size_of_optional_header_(0),
    offset_of_data_directories_(0),
    machine_type_(0),
    number_of_sections_(0),
    sections_(NULL),
    has_text_section_(false),
    size_of_code_(0),
    size_of_initialized_data_(0),
    size_of_uninitialized_data_(0),
    base_of_code_(0),
    base_of_data_(0),
    image_base_(0),
    size_of_image_(0),
    number_of_data_directories_(0) {
}

// ParseHeader attempts to match up the buffer with the Windows data
// structures that exist within a Windows 'Portable Executable' format file.
// Returns 'true' if the buffer matches, and 'false' if the data looks
// suspicious.  Rather than try to 'map' the buffer to the numerous windows
// structures, we extract the information we need into the courgette::PEInfo
// structure.
//
bool DisassemblerWin32X64::ParseHeader() {
  if (length() < kOffsetOfFileAddressOfNewExeHeader + 4 /*size*/)
    return Bad("Too small");

  // Have 'MZ' magic for a DOS header?
  if (start()[0] != 'M' || start()[1] != 'Z')
    return Bad("Not MZ");

  // offset from DOS header to PE header is stored in DOS header.
  uint32 offset = ReadU32(start(),
                          kOffsetOfFileAddressOfNewExeHeader);

  if (offset >= length())
    return Bad("Bad offset to PE header");

  const uint8* const pe_header = OffsetToPointer(offset);
  const size_t kMinPEHeaderSize = 4 /*signature*/ + kSizeOfCoffHeader;
  if (pe_header <= start() ||
      pe_header >= end() - kMinPEHeaderSize)
    return Bad("Bad offset to PE header");

  if (offset % 8 != 0)
    return Bad("Misaligned PE header");

  // The 'PE' header is an IMAGE_NT_HEADERS structure as defined in WINNT.H.
  // See http://msdn.microsoft.com/en-us/library/ms680336(VS.85).aspx
  //
  // The first field of the IMAGE_NT_HEADERS is the signature.
  if (!(pe_header[0] == 'P' &&
        pe_header[1] == 'E' &&
        pe_header[2] == 0 &&
        pe_header[3] == 0))
    return Bad("no PE signature");

  // The second field of the IMAGE_NT_HEADERS is the COFF header.
  // The COFF header is also called an IMAGE_FILE_HEADER
  //   http://msdn.microsoft.com/en-us/library/ms680313(VS.85).aspx
  const uint8* const coff_header = pe_header + 4;
  machine_type_       = ReadU16(coff_header, 0);
  number_of_sections_ = ReadU16(coff_header, 2);
  size_of_optional_header_ = ReadU16(coff_header, 16);

  // The rest of the IMAGE_NT_HEADERS is the IMAGE_OPTIONAL_HEADER(32|64)
  const uint8* const optional_header = coff_header + kSizeOfCoffHeader;
  optional_header_ = optional_header;

  if (optional_header + size_of_optional_header_ >= end())
    return Bad("optional header past end of file");

  // Check we can read the magic.
  if (size_of_optional_header_ < 2)
    return Bad("optional header no magic");

  uint16 magic = ReadU16(optional_header, 0);

  if (magic == kImageNtOptionalHdr32Magic) {
    is_PE32_plus_ = false;
    offset_of_data_directories_ =
      kOffsetOfDataDirectoryFromImageOptionalHeader32;
  } else if (magic == kImageNtOptionalHdr64Magic) {
    is_PE32_plus_ = true;
    offset_of_data_directories_ =
      kOffsetOfDataDirectoryFromImageOptionalHeader64;
  } else {
    return Bad("unrecognized magic");
  }

  // Check that we can read the rest of the the fixed fields.  Data directories
  // directly follow the fixed fields of the IMAGE_OPTIONAL_HEADER.
  if (size_of_optional_header_ < offset_of_data_directories_)
    return Bad("optional header too short");

  // The optional header is either an IMAGE_OPTIONAL_HEADER32 or
  // IMAGE_OPTIONAL_HEADER64
  // http://msdn.microsoft.com/en-us/library/ms680339(VS.85).aspx
  //
  // Copy the fields we care about.
  size_of_code_               = ReadU32(optional_header, 4);
  size_of_initialized_data_   = ReadU32(optional_header, 8);
  size_of_uninitialized_data_ = ReadU32(optional_header, 12);
  base_of_code_               = ReadU32(optional_header, 20);
  if (is_PE32_plus_) {
    base_of_data_ = 0;
    image_base_  = ReadU64(optional_header, 24);
  } else {
    base_of_data_ = ReadU32(optional_header, 24);
    image_base_   = ReadU32(optional_header, 28);
  }
  size_of_image_ = ReadU32(optional_header, 56);
  number_of_data_directories_ =
    ReadU32(optional_header, (is_PE32_plus_ ? 108 : 92));

  if (size_of_code_ >= length() ||
      size_of_initialized_data_ >= length() ||
      size_of_code_ + size_of_initialized_data_ >= length()) {
    // This validation fires on some perfectly fine executables.
    //  return Bad("code or initialized data too big");
  }

  // TODO(sra): we can probably get rid of most of the data directories.
  bool b = true;
  // 'b &= ...' could be short circuit 'b = b && ...' but it is not necessary
  // for correctness and it compiles smaller this way.
  b &= ReadDataDirectory(0, &export_table_);
  b &= ReadDataDirectory(1, &import_table_);
  b &= ReadDataDirectory(2, &resource_table_);
  b &= ReadDataDirectory(3, &exception_table_);
  b &= ReadDataDirectory(5, &base_relocation_table_);
  b &= ReadDataDirectory(11, &bound_import_table_);
  b &= ReadDataDirectory(12, &import_address_table_);
  b &= ReadDataDirectory(13, &delay_import_descriptor_);
  b &= ReadDataDirectory(14, &clr_runtime_header_);
  if (!b) {
    return Bad("malformed data directory");
  }

  // Sections follow the optional header.
  sections_ =
      reinterpret_cast<const Section*>(optional_header +
                                       size_of_optional_header_);
  size_t detected_length = 0;

  for (int i = 0;  i < number_of_sections_;  ++i) {
    const Section* section = &sections_[i];

    // TODO(sra): consider using the 'characteristics' field of the section
    // header to see if the section contains instructions.
    if (memcmp(section->name, ".text", 6) == 0)
      has_text_section_ = true;

    uint32 section_end =
        section->file_offset_of_raw_data + section->size_of_raw_data;
    if (section_end > detected_length)
      detected_length = section_end;
  }

  // Pretend our in-memory copy is only as long as our detected length.
  ReduceLength(detected_length);

  if (is_32bit()) {
    return Bad("32 bit executables are not supported by this disassembler");
  }

  if (!has_text_section()) {
    return Bad("Resource-only executables are not yet supported");
  }

  return Good();
}

bool DisassemblerWin32X64::Disassemble(AssemblyProgram* target) {
  if (!ok())
    return false;

  target->set_image_base(image_base());

  if (!ParseAbs32Relocs())
    return false;

  ParseRel32RelocsFromSections();

  if (!ParseFile(target))
    return false;

  target->DefaultAssignIndexes();

  return true;
}

////////////////////////////////////////////////////////////////////////////////

bool DisassemblerWin32X64::ParseRelocs(std::vector<RVA> *relocs) {
  relocs->clear();

  size_t relocs_size = base_relocation_table_.size_;
  if (relocs_size == 0)
    return true;

  // The format of the base relocation table is a sequence of variable sized
  // IMAGE_BASE_RELOCATION blocks.  Search for
  //   "The format of the base relocation data is somewhat quirky"
  // at http://msdn.microsoft.com/en-us/library/ms809762.aspx

  const uint8* relocs_start = RVAToPointer(base_relocation_table_.address_);
  const uint8* relocs_end = relocs_start + relocs_size;

  // Make sure entire base relocation table is within the buffer.
  if (relocs_start < start() ||
      relocs_start >= end() ||
      relocs_end <= start() ||
      relocs_end > end()) {
    return Bad(".relocs outside image");
  }

  const uint8* block = relocs_start;

  // Walk the variable sized blocks.
  while (block + 8 < relocs_end) {
    RVA page_rva = ReadU32(block, 0);
    uint32 size = ReadU32(block, 4);
    if (size < 8 ||        // Size includes header ...
        size % 4  !=  0)   // ... and is word aligned.
      return Bad("unreasonable relocs block");

    const uint8* end_entries = block + size;

    if (end_entries <= block ||
        end_entries <= start() ||
        end_entries > end())
      return Bad(".relocs block outside image");

    // Walk through the two-byte entries.
    for (const uint8* p = block + 8;  p < end_entries;  p += 2) {
      uint16 entry = ReadU16(p, 0);
      int type = entry >> 12;
      int offset = entry & 0xFFF;

      RVA rva = page_rva + offset;
      if (type == 10) {         // IMAGE_REL_BASED_DIR64
        relocs->push_back(rva);
      } else if (type == 0) {  // IMAGE_REL_BASED_ABSOLUTE
        // Ignore, used as padding.
      } else {
        // Does not occur in Windows x64 executables.
        return Bad("unknown type of reloc");
      }
    }

    block += size;
  }

  std::sort(relocs->begin(), relocs->end());

  return true;
}

const Section* DisassemblerWin32X64::RVAToSection(RVA rva) const {
  for (int i = 0; i < number_of_sections_; i++) {
    const Section* section = &sections_[i];
    uint32 offset = rva - section->virtual_address;
    if (offset < section->virtual_size) {
      return section;
    }
  }
  return NULL;
}

int DisassemblerWin32X64::RVAToFileOffset(RVA rva) const {
  const Section* section = RVAToSection(rva);
  if (section) {
    uint32 offset = rva - section->virtual_address;
    if (offset < section->size_of_raw_data) {
      return section->file_offset_of_raw_data + offset;
    } else {
      return kNoOffset;  // In section but not in file (e.g. uninit data).
    }
  }

  // Small RVA values point into the file header in the loaded image.
  // RVA 0 is the module load address which Windows uses as the module handle.
  // RVA 2 sometimes occurs, I'm not sure what it is, but it would map into the
  // DOS header.
  if (rva == 0 || rva == 2)
    return rva;

  NOTREACHED();
  return kNoOffset;
}

const uint8* DisassemblerWin32X64::RVAToPointer(RVA rva) const {
  int file_offset = RVAToFileOffset(rva);
  if (file_offset == kNoOffset)
    return NULL;
  else
    return OffsetToPointer(file_offset);
}

std::string DisassemblerWin32X64::SectionName(const Section* section) {
  if (section == NULL)
    return "<none>";
  char name[9];
  memcpy(name, section->name, 8);
  name[8] = '\0';  // Ensure termination.
  return name;
}

CheckBool DisassemblerWin32X64::ParseFile(AssemblyProgram* program) {
  // Walk all the bytes in the file, whether or not in a section.
  uint32 file_offset = 0;
  while (file_offset < length()) {
    const Section* section = FindNextSection(file_offset);
    if (section == NULL) {
      // No more sections.  There should not be extra stuff following last
      // section.
      //   ParseNonSectionFileRegion(file_offset, pe_info().length(), program);
      break;
    }
    if (file_offset < section->file_offset_of_raw_data) {
      uint32 section_start_offset = section->file_offset_of_raw_data;
      if(!ParseNonSectionFileRegion(file_offset, section_start_offset,
                                    program))
        return false;

      file_offset = section_start_offset;
    }
    uint32 end = file_offset + section->size_of_raw_data;
    if (!ParseFileRegion(section, file_offset, end, program))
      return false;
    file_offset = end;
  }

#if COURGETTE_HISTOGRAM_TARGETS
  HistogramTargets("abs32 relocs", abs32_target_rvas_);
  HistogramTargets("rel32 relocs", rel32_target_rvas_);
#endif

  return true;
}

bool DisassemblerWin32X64::ParseAbs32Relocs() {
  abs32_locations_.clear();
  if (!ParseRelocs(&abs32_locations_))
    return false;

  std::sort(abs32_locations_.begin(), abs32_locations_.end());

#if COURGETTE_HISTOGRAM_TARGETS
  for (size_t i = 0;  i < abs32_locations_.size(); ++i) {
    RVA rva = abs32_locations_[i];
    // The 4 bytes at the relocation are a reference to some address.
    uint32 target_address = Read32LittleEndian(RVAToPointer(rva));
    ++abs32_target_rvas_[target_address - image_base()];
  }
#endif
  return true;
}

void DisassemblerWin32X64::ParseRel32RelocsFromSections() {
  uint32 file_offset = 0;
  while (file_offset < length()) {
    const Section* section = FindNextSection(file_offset);
    if (section == NULL)
      break;
    if (file_offset < section->file_offset_of_raw_data)
      file_offset = section->file_offset_of_raw_data;
    ParseRel32RelocsFromSection(section);
    file_offset += section->size_of_raw_data;
  }
  std::sort(rel32_locations_.begin(), rel32_locations_.end());

#if COURGETTE_HISTOGRAM_TARGETS
  VLOG(1) << "abs32_locations_ " << abs32_locations_.size()
          << "\nrel32_locations_ " << rel32_locations_.size()
          << "\nabs32_target_rvas_ " << abs32_target_rvas_.size()
          << "\nrel32_target_rvas_ " << rel32_target_rvas_.size();

  int common = 0;
  std::map<RVA, int>::iterator abs32_iter = abs32_target_rvas_.begin();
  std::map<RVA, int>::iterator rel32_iter = rel32_target_rvas_.begin();
  while (abs32_iter != abs32_target_rvas_.end() &&
         rel32_iter != rel32_target_rvas_.end()) {
    if (abs32_iter->first < rel32_iter->first)
      ++abs32_iter;
    else if (rel32_iter->first < abs32_iter->first)
      ++rel32_iter;
    else {
      ++common;
      ++abs32_iter;
      ++rel32_iter;
    }
  }
  VLOG(1) << "common " << common;
#endif
}

void DisassemblerWin32X64::ParseRel32RelocsFromSection(const Section* section) {
  // TODO(sra): use characteristic.
  bool isCode = strcmp(section->name, ".text") == 0;
  if (!isCode)
    return;

  uint32 start_file_offset = section->file_offset_of_raw_data;
  uint32 end_file_offset = start_file_offset + section->size_of_raw_data;
  RVA relocs_start_rva = base_relocation_table().address_;

  const uint8* start_pointer = OffsetToPointer(start_file_offset);
  const uint8* end_pointer = OffsetToPointer(end_file_offset);

  RVA start_rva = FileOffsetToRVA(start_file_offset);
  RVA end_rva = start_rva + section->virtual_size;

  // Quick way to convert from Pointer to RVA within a single Section is to
  // subtract 'pointer_to_rva'.
  const uint8* const adjust_pointer_to_rva = start_pointer - start_rva;

  std::vector<RVA>::iterator abs32_pos = abs32_locations_.begin();

  // Find the rel32 relocations.
  const uint8* p = start_pointer;
  while (p < end_pointer) {
    RVA current_rva = static_cast<RVA>(p - adjust_pointer_to_rva);
    if (current_rva == relocs_start_rva) {
      uint32 relocs_size = base_relocation_table().size_;
      if (relocs_size) {
        p += relocs_size;
        continue;
      }
    }

    //while (abs32_pos != abs32_locations_.end() && *abs32_pos < current_rva)
    //  ++abs32_pos;

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
      RVA rel32_rva = static_cast<RVA>(rel32 - adjust_pointer_to_rva);

      // Is there an abs32 reloc overlapping the candidate?
      while (abs32_pos != abs32_locations_.end() && *abs32_pos < rel32_rva - 3)
        ++abs32_pos;
      // Now: (*abs32_pos > rel32_rva - 4) i.e. the lowest addressed 4-byte
      // region that could overlap rel32_rva.
      if (abs32_pos != abs32_locations_.end()) {
        if (*abs32_pos < rel32_rva + 4) {
          // Beginning of abs32 reloc is before end of rel32 reloc so they
          // overlap.  Skip four bytes past the abs32 reloc.
          p += (*abs32_pos + 4) - current_rva;
          continue;
        }
      }

      RVA target_rva = rel32_rva + 4 + Read32LittleEndian(rel32);
      // To be valid, rel32 target must be within image, and within this
      // section.
      if (IsValidRVA(target_rva) &&
          start_rva <= target_rva && target_rva < end_rva) {
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

CheckBool DisassemblerWin32X64::ParseNonSectionFileRegion(
    uint32 start_file_offset,
    uint32 end_file_offset,
    AssemblyProgram* program) {
  if (incomplete_disassembly_)
    return true;

  const uint8* start = OffsetToPointer(start_file_offset);
  const uint8* end = OffsetToPointer(end_file_offset);

  const uint8* p = start;

  while (p < end) {
    if (!program->EmitByteInstruction(*p))
      return false;
    ++p;
  }

  return true;
}

CheckBool DisassemblerWin32X64::ParseFileRegion(
    const Section* section,
    uint32 start_file_offset, uint32 end_file_offset,
    AssemblyProgram* program) {
  RVA relocs_start_rva = base_relocation_table().address_;

  const uint8* start_pointer = OffsetToPointer(start_file_offset);
  const uint8* end_pointer = OffsetToPointer(end_file_offset);

  RVA start_rva = FileOffsetToRVA(start_file_offset);
  RVA end_rva = start_rva + section->virtual_size;

  // Quick way to convert from Pointer to RVA within a single Section is to
  // subtract 'pointer_to_rva'.
  const uint8* const adjust_pointer_to_rva = start_pointer - start_rva;

  std::vector<RVA>::iterator rel32_pos = rel32_locations_.begin();
  std::vector<RVA>::iterator abs32_pos = abs32_locations_.begin();

  if (!program->EmitOriginInstruction(start_rva))
    return false;

  const uint8* p = start_pointer;

  while (p < end_pointer) {
    RVA current_rva = static_cast<RVA>(p - adjust_pointer_to_rva);

    // The base relocation table is usually in the .relocs section, but it could
    // actually be anywhere.  Make sure we skip it because we will regenerate it
    // during assembly.
    if (current_rva == relocs_start_rva) {
      if (!program->EmitPeRelocsInstruction())
        return false;
      uint32 relocs_size = base_relocation_table().size_;
      if (relocs_size) {
        p += relocs_size;
        continue;
      }
    }

    while (abs32_pos != abs32_locations_.end() && *abs32_pos < current_rva)
      ++abs32_pos;

    if (abs32_pos != abs32_locations_.end() && *abs32_pos == current_rva) {
      uint32 target_address = Read32LittleEndian(p);
      RVA target_rva = target_address - image_base();
      // TODO(sra): target could be Label+offset.  It is not clear how to guess
      // which it might be.  We assume offset==0.
      if (!program->EmitAbs32(program->FindOrMakeAbs32Label(target_rva)))
        return false;
      p += 4;
      continue;
    }

    while (rel32_pos != rel32_locations_.end() && *rel32_pos < current_rva)
      ++rel32_pos;

    if (rel32_pos != rel32_locations_.end() && *rel32_pos == current_rva) {
      RVA target_rva = current_rva + 4 + Read32LittleEndian(p);
      if (!program->EmitRel32(program->FindOrMakeRel32Label(target_rva)))
        return false;
      p += 4;
      continue;
    }

    if (incomplete_disassembly_) {
      if ((abs32_pos == abs32_locations_.end() || end_rva <= *abs32_pos) &&
          (rel32_pos == rel32_locations_.end() || end_rva <= *rel32_pos) &&
          (end_rva <= relocs_start_rva || current_rva >= relocs_start_rva)) {
        // No more relocs in this section, don't bother encoding bytes.
        break;
      }
    }

    if (!program->EmitByteInstruction(*p))
      return false;
    p += 1;
  }

  return true;
}

#if COURGETTE_HISTOGRAM_TARGETS
// Histogram is printed to std::cout.  It is purely for debugging the algorithm
// and is only enabled manually in 'exploration' builds.  I don't want to add
// command-line configuration for this feature because this code has to be
// small, which means compiled-out.
void DisassemblerWin32X64::HistogramTargets(const char* kind,
                                            const std::map<RVA, int>& map) {
  int total = 0;
  std::map<int, std::vector<RVA> > h;
  for (std::map<RVA, int>::const_iterator p = map.begin();
       p != map.end();
       ++p) {
    h[p->second].push_back(p->first);
    total += p->second;
  }

  std::cout << total << " " << kind << " to "
            << map.size() << " unique targets" << std::endl;

  std::cout << "indegree: #targets-with-indegree (example)" << std::endl;
  const int kFirstN = 15;
  bool someSkipped = false;
  int index = 0;
  for (std::map<int, std::vector<RVA> >::reverse_iterator p = h.rbegin();
       p != h.rend();
       ++p) {
    ++index;
    if (index <= kFirstN || p->first <= 3) {
      if (someSkipped) {
        std::cout << "..." << std::endl;
      }
      size_t count = p->second.size();
      std::cout << std::dec << p->first << ": " << count;
      if (count <= 2) {
        for (size_t i = 0;  i < count;  ++i)
          std::cout << "  " << DescribeRVA(p->second[i]);
      }
      std::cout << std::endl;
      someSkipped = false;
    } else {
      someSkipped = true;
    }
  }
}
#endif  // COURGETTE_HISTOGRAM_TARGETS


// DescribeRVA is for debugging only.  I would put it under #ifdef DEBUG except
// that during development I'm finding I need to call it when compiled in
// Release mode.  Hence:
// TODO(sra): make this compile only for debug mode.
std::string DisassemblerWin32X64::DescribeRVA(RVA rva) const {
  const Section* section = RVAToSection(rva);
  std::ostringstream s;
  s << std::hex << rva;
  if (section) {
    s << " (";
    s << SectionName(section) << "+"
      << std::hex << (rva - section->virtual_address)
      << ")";
  }
  return s.str();
}

const Section* DisassemblerWin32X64::FindNextSection(uint32 fileOffset) const {
  const Section* best = 0;
  for (int i = 0; i < number_of_sections_; i++) {
    const Section* section = &sections_[i];
    if (section->size_of_raw_data > 0) {  // i.e. has data in file.
      if (fileOffset <= section->file_offset_of_raw_data) {
        if (best == 0 ||
            section->file_offset_of_raw_data < best->file_offset_of_raw_data) {
          best = section;
        }
      }
    }
  }
  return best;
}

RVA DisassemblerWin32X64::FileOffsetToRVA(uint32 file_offset) const {
  for (int i = 0; i < number_of_sections_; i++) {
    const Section* section = &sections_[i];
    uint32 offset = file_offset - section->file_offset_of_raw_data;
    if (offset < section->size_of_raw_data) {
      return section->virtual_address + offset;
    }
  }
  return 0;
}

bool DisassemblerWin32X64::ReadDataDirectory(
    int index,
    ImageDataDirectory* directory) {

  if (index < number_of_data_directories_) {
    size_t offset = index * 8 + offset_of_data_directories_;
    if (offset >= size_of_optional_header_)
      return Bad("number of data directories inconsistent");
    const uint8* data_directory = optional_header_ + offset;
    if (data_directory < start() ||
        data_directory + 8 >= end())
      return Bad("data directory outside image");
    RVA rva = ReadU32(data_directory, 0);
    size_t size  = ReadU32(data_directory, 4);
    if (size > size_of_image_)
      return Bad("data directory size too big");

    // TODO(sra): validate RVA.
    directory->address_ = rva;
    directory->size_ = static_cast<uint32>(size);
    return true;
  } else {
    directory->address_ = 0;
    directory->size_ = 0;
    return true;
  }
}

}  // namespace courgette
