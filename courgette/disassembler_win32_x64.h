// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COURGETTE_DISASSEMBLER_WIN32_X64_H_
#define COURGETTE_DISASSEMBLER_WIN32_X64_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "courgette/disassembler.h"
#include "courgette/image_utils.h"
#include "courgette/memory_allocator.h"
#include "courgette/types_win_pe.h"

namespace courgette {

class AssemblyProgram;

class DisassemblerWin32X64 : public Disassembler {
 public:
  explicit DisassemblerWin32X64(const void* start, size_t length);

  // Disassembler interfaces.
  RVA FileOffsetToRVA(FileOffset file_offset) const override;
  FileOffset RVAToFileOffset(RVA rva) const override;
  RVA PointerToTargetRVA(const uint8_t* p) const override;
  ExecutableType kind() const override { return EXE_WIN_32_X64; }
  bool ParseHeader() override;
  bool Disassemble(AssemblyProgram* target) override;

  // Exposed for test purposes
  bool has_text_section() const { return has_text_section_; }
  uint32_t size_of_code() const { return size_of_code_; }
  bool is_32bit() const { return !is_PE32_plus_; }

  // Returns 'true' if the base relocation table can be parsed.
  // Output is a vector of the RVAs corresponding to locations within executable
  // that are listed in the base relocation table.
  bool ParseRelocs(std::vector<RVA> *addresses);

  // Returns Section containing the relative virtual address, or null if none.
  const Section* RVAToSection(RVA rva) const;

  // (4) -> (5) (see AddressTranslator comment): Returns the RVA of the VA
  // specified by |address|, or kNoRVA if |address| lies outside of the image.
  RVA Address64ToRVA(uint64_t address) const;

  static std::string SectionName(const Section* section);

 protected:
  // Disassembler interfaces.
  RvaVisitor* CreateAbs32TargetRvaVisitor() override;
  RvaVisitor* CreateRel32TargetRvaVisitor() override;
  void RemoveUnusedRel32Locations(AssemblyProgram* program) override;

  CheckBool ParseFile(AssemblyProgram* target) WARN_UNUSED_RESULT;
  bool ParseAbs32Relocs();
  void ParseRel32RelocsFromSections();
  void ParseRel32RelocsFromSection(const Section* section);

  CheckBool ParseNonSectionFileRegion(FileOffset start_file_offset,
                                      FileOffset end_file_offset,
                                      AssemblyProgram* program)
      WARN_UNUSED_RESULT;
  CheckBool ParseFileRegion(const Section* section,
                            FileOffset start_file_offset,
                            FileOffset end_file_offset,
                            AssemblyProgram* program) WARN_UNUSED_RESULT;

#if COURGETTE_HISTOGRAM_TARGETS
  void HistogramTargets(const char* kind, const std::map<RVA, int>& map);
#endif

  // Most addresses are represented as 32-bit RVAs. The one address we can't
  // do this with is the image base address.
  uint64_t image_base() const { return image_base_; }

  const ImageDataDirectory& base_relocation_table() const {
    return base_relocation_table_;
  }

  // Returns description of the RVA, e.g. ".text+0x1243". For debugging only.
  std::string DescribeRVA(RVA rva) const;

  // Finds the first section at file_offset or above. Does not return sections
  // that have no raw bytes in the file.
  const Section* FindNextSection(FileOffset file_offset) const;

 private:
  bool ReadDataDirectory(int index, ImageDataDirectory* dir);

  bool incomplete_disassembly_;  // true if can omit "uninteresting" bits.

  std::vector<RVA> abs32_locations_;
  std::vector<RVA> rel32_locations_;

  //
  // Information that is valid after ParseHeader() succeeds.
  //
  bool is_PE32_plus_;  // PE32_plus is for 64 bit executables.

  // Location and size of IMAGE_OPTIONAL_HEADER in the buffer.
  const uint8_t* optional_header_;
  uint16_t size_of_optional_header_;
  uint16_t offset_of_data_directories_;

  uint16_t machine_type_;
  uint16_t number_of_sections_;
  const Section *sections_;
  bool has_text_section_;

  uint32_t size_of_code_;
  uint32_t size_of_initialized_data_;
  uint32_t size_of_uninitialized_data_;
  RVA base_of_code_;
  RVA base_of_data_;

  uint64_t image_base_;
  uint32_t size_of_image_;
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
