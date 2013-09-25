// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COURGETTE_DISASSEMBLER_WIN32_X64_H_
#define COURGETTE_DISASSEMBLER_WIN32_X64_H_

#include "base/basictypes.h"
#include "courgette/disassembler.h"
#include "courgette/memory_allocator.h"
#include "courgette/types_win_pe.h"

#ifdef COURGETTE_HISTOGRAM_TARGETS
#include <map>
#endif

namespace courgette {

class AssemblyProgram;

class DisassemblerWin32X64 : public Disassembler {
 public:
  explicit DisassemblerWin32X64(const void* start, size_t length);

  virtual ExecutableType kind() { return EXE_WIN_32_X64; }

  // Returns 'true' if the buffer appears to point to a Windows 32 bit
  // executable, 'false' otherwise.  If ParseHeader() succeeds, other member
  // functions may be called.
  virtual bool ParseHeader();

  virtual bool Disassemble(AssemblyProgram* target);

  //
  // Exposed for test purposes
  //

  bool has_text_section() const { return has_text_section_; }
  uint32 size_of_code() const { return size_of_code_; }
  bool is_32bit() const { return !is_PE32_plus_; }

  // Returns 'true' if the base relocation table can be parsed.
  // Output is a vector of the RVAs corresponding to locations within executable
  // that are listed in the base relocation table.
  bool ParseRelocs(std::vector<RVA> *addresses);

  // Returns Section containing the relative virtual address, or NULL if none.
  const Section* RVAToSection(RVA rva) const;

  static const int kNoOffset = -1;
  // Returns kNoOffset if there is no file offset corresponding to 'rva'.
  int RVAToFileOffset(RVA rva) const;

  // Returns same as FileOffsetToPointer(RVAToFileOffset(rva)) except that NULL
  // is returned if there is no file offset corresponding to 'rva'.
  const uint8* RVAToPointer(RVA rva) const;

  static std::string SectionName(const Section* section);

 protected:
  CheckBool ParseFile(AssemblyProgram* target) WARN_UNUSED_RESULT;
  bool ParseAbs32Relocs();
  void ParseRel32RelocsFromSections();
  void ParseRel32RelocsFromSection(const Section* section);

  CheckBool ParseNonSectionFileRegion(uint32 start_file_offset,
      uint32 end_file_offset, AssemblyProgram* program) WARN_UNUSED_RESULT;
  CheckBool ParseFileRegion(const Section* section,
      uint32 start_file_offset, uint32 end_file_offset,
      AssemblyProgram* program) WARN_UNUSED_RESULT;

#if COURGETTE_HISTOGRAM_TARGETS
  void HistogramTargets(const char* kind, const std::map<RVA, int>& map);
#endif

  // Most addresses are represented as 32-bit RVAs.  The one address we can't
  // do this with is the image base address.  'image_base' is valid only for
  // 32-bit executables. 'image_base_64' is valid for 32- and 64-bit executable.
  uint64 image_base() const { return image_base_; }

  const ImageDataDirectory& base_relocation_table() const {
    return base_relocation_table_;
  }

  bool IsValidRVA(RVA rva) const { return rva < size_of_image_; }

  // Returns description of the RVA, e.g. ".text+0x1243".  For debugging only.
  std::string DescribeRVA(RVA rva) const;

  // Finds the first section at file_offset or above.  Does not return sections
  // that have no raw bytes in the file.
  const Section* FindNextSection(uint32 file_offset) const;

  // There are 2 'coordinate systems' for reasoning about executables.
  //   FileOffset - the the offset within a single .EXE or .DLL *file*.
  //   RVA - relative virtual address (offset within *loaded image*)
  // FileOffsetToRVA and RVAToFileOffset convert between these representations.

  RVA FileOffsetToRVA(uint32 offset) const;


 private:

  bool ReadDataDirectory(int index, ImageDataDirectory* dir);

  bool incomplete_disassembly_;  // 'true' if can leave out 'uninteresting' bits

  std::vector<RVA> abs32_locations_;
  std::vector<RVA> rel32_locations_;

  //
  // Fields that are always valid.
  //

  //
  // Information that is valid after successful ParseHeader.
  //
  bool is_PE32_plus_;   // PE32_plus is for 64 bit executables.

  // Location and size of IMAGE_OPTIONAL_HEADER in the buffer.
  const uint8 *optional_header_;
  uint16 size_of_optional_header_;
  uint16 offset_of_data_directories_;

  uint16 machine_type_;
  uint16 number_of_sections_;
  const Section *sections_;
  bool has_text_section_;

  uint32 size_of_code_;
  uint32 size_of_initialized_data_;
  uint32 size_of_uninitialized_data_;
  RVA base_of_code_;
  RVA base_of_data_;

  uint64 image_base_;
  uint32 size_of_image_;
  int number_of_data_directories_;

  ImageDataDirectory export_table_;
  ImageDataDirectory import_table_;
  ImageDataDirectory resource_table_;
  ImageDataDirectory exception_table_;
  ImageDataDirectory base_relocation_table_;
  ImageDataDirectory bound_import_table_;
  ImageDataDirectory import_address_table_;
  ImageDataDirectory delay_import_descriptor_;
  ImageDataDirectory clr_runtime_header_;

#if COURGETTE_HISTOGRAM_TARGETS
  std::map<RVA, int> abs32_target_rvas_;
  std::map<RVA, int> rel32_target_rvas_;
#endif


  DISALLOW_COPY_AND_ASSIGN(DisassemblerWin32X64);
};

}  // namespace courgette
#endif  // COURGETTE_DISASSEMBLER_WIN32_X64_H_
