// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COURGETTE_DISASSEMBLER_WIN32_X86_H_
#define COURGETTE_DISASSEMBLER_WIN32_X86_H_

#include "base/basictypes.h"
#include "courgette/disassembler.h"
#include "courgette/image_info.h"
#include "courgette/memory_allocator.h"

namespace courgette {

class AssemblyProgram;

class DisassemblerWin32X86 : public Disassembler {
 public:
  explicit DisassemblerWin32X86(PEInfo* pe_info);

  virtual bool Disassemble(AssemblyProgram* target);

 protected:
  PEInfo& pe_info() { return *pe_info_; }

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

  PEInfo* pe_info_;
  bool incomplete_disassembly_;  // 'true' if can leave out 'uninteresting' bits

  std::vector<RVA> abs32_locations_;
  std::vector<RVA> rel32_locations_;

#if COURGETTE_HISTOGRAM_TARGETS
  std::map<RVA, int> abs32_target_rvas_;
  std::map<RVA, int> rel32_target_rvas_;
#endif

  DISALLOW_COPY_AND_ASSIGN(DisassemblerWin32X86);
};

}  // namespace courgette
#endif  // COURGETTE_DISASSEMBLER_WIN32_X86_H_
