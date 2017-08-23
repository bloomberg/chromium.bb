// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_ZUCCHINI_DISASSEMBLER_WIN32_H_
#define CHROME_INSTALLER_ZUCCHINI_DISASSEMBLER_WIN32_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "chrome/installer/zucchini/address_translator.h"
#include "chrome/installer/zucchini/buffer_view.h"
#include "chrome/installer/zucchini/disassembler.h"
#include "chrome/installer/zucchini/image_utils.h"
#include "chrome/installer/zucchini/type_win_pe.h"

namespace zucchini {

class Rel32FinderX86;
class Rel32FinderX64;

struct Win32X86Traits {
  static constexpr ExecutableType kExeType = kExeTypeWin32X86;
  enum : uint16_t { kMagic = 0x10B };
  enum : uint16_t { kRelocType = 3 };
  enum : offset_t { kVAWidth = 4 };
  static const char kExeTypeString[];

  using ImageOptionalHeader = pe::ImageOptionalHeader;
  using RelFinder = Rel32FinderX86;
  using Address = uint32_t;
};

struct Win32X64Traits {
  static constexpr ExecutableType kExeType = kExeTypeWin32X64;
  enum : uint16_t { kMagic = 0x20B };
  enum : uint16_t { kRelocType = 10 };
  enum : offset_t { kVAWidth = 8 };
  static const char kExeTypeString[];

  using ImageOptionalHeader = pe::ImageOptionalHeader64;
  using RelFinder = Rel32FinderX64;
  using Address = uint64_t;
};

template <class Traits>
class DisassemblerWin32 : public Disassembler, public AddressTranslatorFactory {
 public:
  enum ReferenceType : uint8_t { kRel32, kTypeCount };

  // Applies quick checks to determine whether |image| *may* point to the start
  // of an executable. Returns true iff the check passes.
  static bool QuickDetect(ConstBufferView image);

  // Main instantiator and initializer.
  static std::unique_ptr<DisassemblerWin32> Make(ConstBufferView image);

  DisassemblerWin32();
  ~DisassemblerWin32() override;

  // Disassembler:
  ExecutableType GetExeType() const override;
  std::string GetExeTypeString() const override;
  std::vector<ReferenceGroup> MakeReferenceGroups() const override;

  // AddressTranslatorFactory:
  // The returned objects must not outlive |this|, since it uses the pointer.
  std::unique_ptr<RVAToOffsetTranslator> MakeRVAToOffsetTranslator()
      const override;
  std::unique_ptr<OffsetToRVATranslator> MakeOffsetToRVATranslator()
      const override;

  // Functions that return reader / writer for references.
  std::unique_ptr<ReferenceReader> MakeReadRel32(offset_t lower,
                                                 offset_t upper);
  std::unique_ptr<ReferenceWriter> MakeWriteRel32(MutableBufferView image);

 private:
  // Disassembler:
  bool Parse(ConstBufferView image) override;

  // Parses the file header. Returns true iff successful.
  bool ParseHeader();

  // Parsers to extract references. These are lazily called, and return whether
  // parsing was successful (failures are non-fatal).
  bool ParseAndStoreRel32();

  // Translation utilities.
  const pe::ImageSectionHeader* RVAToSection(rva_t rva) const;
  const pe::ImageSectionHeader* OffsetToSection(offset_t offset) const;
  offset_t RVAToOffset(rva_t rva) const;
  rva_t OffsetToRVA(offset_t offset) const;
  rva_t AddressToRVA(typename Traits::Address address) const;
  typename Traits::Address RVAToAddress(rva_t rva) const;

  // In-memory copy of sections.
  std::vector<pe::ImageSectionHeader> sections_;

  // Image base address to translate between RVA and VA.
  typename Traits::Address image_base_ = 0;

  // An exclusive upper bound on all RVAs used in the image.
  rva_t rva_bound_ = 0;

  // Some RVAs have no matching offset on disk (e.g., RVAs to .bss sections).
  // However, Zucchini reprsents all references using offsets. To accommodate
  // these RVAs, Zucchini assigns "fake offsets" to these RVAs so they can be
  // processed and restored. |fake_offset_adjust_| is the adjustment factor.
  // This is made into a large value (e.g., max of |rva_bound_| and image size)
  // to prevent collision with regular offsets.
  uint32_t fake_offset_adjust_ = 0;

  // Maps for address translation, sorted by the first element. The second
  // element of each std::pair is an index in into |sections_|.
  std::vector<std::pair<rva_t, int>> section_rva_map_;
  std::vector<std::pair<offset_t, int>> section_offset_map_;

  // Reference storage.
  std::vector<offset_t> rel32_locations_;

  // Initialization states of reference storage, used for lazy initialization.
  // TODO(huangs): Investigate whether lazy initialization is useful for memory
  // reduction. This is a carryover from Courgette. To be sure we should run
  // experiment after Zucchini is able to do ensemble patching.
  bool has_parsed_rel32_ = false;

  DISALLOW_COPY_AND_ASSIGN(DisassemblerWin32);
};

using DisassemblerWin32X86 = DisassemblerWin32<Win32X86Traits>;
using DisassemblerWin32X64 = DisassemblerWin32<Win32X64Traits>;

}  // namespace zucchini

#endif  // CHROME_INSTALLER_ZUCCHINI_DISASSEMBLER_WIN32_H_
