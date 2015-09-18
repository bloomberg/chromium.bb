// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COURGETTE_REL32_FINDER_WIN32_X86_H_
#define COURGETTE_REL32_FINDER_WIN32_X86_H_

#if COURGETTE_HISTOGRAM_TARGETS
#include <map>
#endif
#include <vector>

#include "base/basictypes.h"
#include "courgette/image_utils.h"

namespace courgette {

// A helper class to scan through a section of code to extract RVAs.
class Rel32FinderWin32X86 {
 public:
  Rel32FinderWin32X86(RVA relocs_start_rva, RVA relocs_end_rva,
                      RVA image_end_rva);
  virtual ~Rel32FinderWin32X86();

  bool IsValidRVA(RVA rva) const { return rva < image_end_rva_; }

  // Swaps data in |rel32_locations_| to |dest|.
  void SwapRel32Locations(std::vector<RVA>* dest);

#if COURGETTE_HISTOGRAM_TARGETS
  // Swaps data in |rel32_target_rvas_| to |dest|.
  void SwapRel32TargetRVAs(std::map<RVA, int>* dest);
#endif

  // Scans through [|start_pointer|, |end_pointer|) for rel32 addresses. Seeks
  // RVAs that satisfy the following:
  // - Do not collide with |abs32_pos| (assumed sorted).
  // - Do not collide with |base_relocation_table|'s RVA range,
  // - Whose targets are in [|start_rva|, |end_rva|).
  // The sorted results are written to |rel32_locations_|.
  virtual void Find(const uint8* start_pointer,
                    const uint8* end_pointer,
                    RVA start_rva,
                    RVA end_rva,
                    const std::vector<RVA>& abs32_locations) = 0;

 protected:
  const RVA relocs_start_rva_;
  const RVA relocs_end_rva_;
  const RVA image_end_rva_;

  std::vector<RVA> rel32_locations_;

#if COURGETTE_HISTOGRAM_TARGETS
  std::map<RVA, int> rel32_target_rvas_;
#endif
};

// The basic implementation performs naive scan for rel32 JMP and Jcc opcodes
// (excluding JPO/JPE) disregarding instruction alignment.
class Rel32FinderWin32X86_Basic : public Rel32FinderWin32X86 {
 public:
  Rel32FinderWin32X86_Basic(RVA relocs_start_rva, RVA relocs_end_rva,
                            RVA image_end_rva);
  virtual ~Rel32FinderWin32X86_Basic();

  // Rel32FinderWin32X86 implementation.
  void Find(const uint8* start_pointer,
            const uint8* end_pointer,
            RVA start_rva,
            RVA end_rva,
            const std::vector<RVA>& abs32_locations) override;
};

}  // namespace courgette

#endif  // COURGETTE_REL32_FINDER_WIN32_X86_H_
