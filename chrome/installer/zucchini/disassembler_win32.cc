// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/zucchini/disassembler_win32.h"

#include <algorithm>
#include <cstddef>
#include <functional>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "chrome/installer/zucchini/algorithm.h"
#include "chrome/installer/zucchini/buffer_source.h"
#include "chrome/installer/zucchini/io_utils.h"
#include "chrome/installer/zucchini/rel32_finder.h"
#include "chrome/installer/zucchini/rel32_utils.h"

namespace zucchini {

namespace {

// Decides whether |image| points to a Win32 PE file. If this is a possibility,
// assigns |source| to enable further parsing and returns true. Otherwise
// leaves |source| at an undefined state and returns false.
template <class Traits>
bool ReadWin32Header(ConstBufferView image, BufferSource* source) {
  *source = BufferSource(image);

  // Check "MZ" magic of DOS header.
  if (!source->CheckNextBytes({'M', 'Z'}))
    return false;

  const auto* dos_header = source->GetPointer<pe::ImageDOSHeader>();
  if (!dos_header || (dos_header->e_lfanew & 7) != 0)
    return false;

  // Offset to PE header is in DOS header.
  *source = std::move(BufferSource(image).Skip(dos_header->e_lfanew));
  // Check 'PE\0\0' magic from PE header.
  if (!source->ConsumeBytes({'P', 'E', 0, 0}))
    return false;

  return true;
}

// Decides whether |section| (assumed value) is a section that contains code.
template <class Traits>
bool IsWin32CodeSection(const pe::ImageSectionHeader& section) {
  return (section.characteristics & kCodeCharacteristics) ==
         kCodeCharacteristics;
}

}  // namespace

// static
constexpr ExecutableType Win32X86Traits::kExeType;
const char Win32X86Traits::kExeTypeString[] = "Windows PE x86";

constexpr ExecutableType Win32X64Traits::kExeType;
const char Win32X64Traits::kExeTypeString[] = "Windows PE x64";

/******** DisassemblerWin32 ********/

// static.
// This implements basic checks without storing data.
template <class Traits>
bool DisassemblerWin32<Traits>::QuickDetect(ConstBufferView image) {
  BufferSource source;
  return ReadWin32Header<Traits>(image, &source);
}

// static
template <class Traits>
std::unique_ptr<DisassemblerWin32<Traits>> DisassemblerWin32<Traits>::Make(
    ConstBufferView image) {
  std::unique_ptr<DisassemblerWin32> disasm =
      base::MakeUnique<DisassemblerWin32>();
  if (disasm->Parse(image))
    return disasm;
  return nullptr;
}

template <class Traits>
DisassemblerWin32<Traits>::DisassemblerWin32() = default;

template <class Traits>
DisassemblerWin32<Traits>::~DisassemblerWin32() = default;

template <class Traits>
ExecutableType DisassemblerWin32<Traits>::GetExeType() const {
  return Traits::kExeType;
}

template <class Traits>
std::string DisassemblerWin32<Traits>::GetExeTypeString() const {
  return Traits::kExeTypeString;
}

template <class Traits>
std::vector<ReferenceGroup> DisassemblerWin32<Traits>::MakeReferenceGroups()
    const {
  return {
      {ReferenceTypeTraits{4, TypeTag(kRel32), PoolTag(kRel32)},
       &DisassemblerWin32::MakeReadRel32, &DisassemblerWin32::MakeWriteRel32},
  };
}

template <class Traits>
std::unique_ptr<ReferenceReader> DisassemblerWin32<Traits>::MakeReadRel32(
    offset_t lower,
    offset_t upper) {
  ParseAndStoreRel32();
  return base::MakeUnique<Rel32ReaderX86>(image_, lower, upper,
                                          &rel32_locations_, *this);
}

template <class Traits>
std::unique_ptr<ReferenceWriter> DisassemblerWin32<Traits>::MakeWriteRel32(
    MutableBufferView image) {
  return base::MakeUnique<Rel32WriterX86>(image, *this);
}

template <class Traits>
std::unique_ptr<RVAToOffsetTranslator>
DisassemblerWin32<Traits>::MakeRVAToOffsetTranslator() const {
  // Use enclosed class to wrap RVAToSection(). Intermediate results |section_|
  // and |adjust_| are cached, as optimization.
  class RVAToOffset : public RVAToOffsetTranslator {
   public:
    explicit RVAToOffset(const DisassemblerWin32* self) : self_(self) {}

    // RVAToOffsetTranslator:
    offset_t Convert(rva_t rva) override {
      if (section_ == nullptr || rva < section_->virtual_address ||
          rva >=
              section_->virtual_address + std::min(section_->size_of_raw_data,
                                                   section_->virtual_size)) {
        section_ = self_->RVAToSection(rva);
        if (section_ != nullptr &&
            rva < section_->virtual_address + section_->virtual_size) {
          // |rva| is in |section_|, so |adjust_| can be computed normall.
          adjust_ =
              section_->file_offset_of_raw_data - section_->virtual_address;
        } else {
          // |rva| has no offset, so use |self_->fake_offset_adjust_|.
          adjust_ = self_->fake_offset_adjust_;
        }
      }
      return rva_t(rva + adjust_);
    }

   private:
    const DisassemblerWin32* self_;
    const pe::ImageSectionHeader* section_ = nullptr;
    std::ptrdiff_t adjust_ = 0;
  };
  return base::MakeUnique<RVAToOffset>(this);
}

template <class Traits>
std::unique_ptr<OffsetToRVATranslator>
DisassemblerWin32<Traits>::MakeOffsetToRVATranslator() const {
  // Use enclosed class to wrap SectionToRVA(). Intermediate results |section_|
  // and |adjust_| are cached, as optimization.
  class OffsetToRVA : public OffsetToRVATranslator {
   public:
    explicit OffsetToRVA(const DisassemblerWin32* self) : self_(self) {}

    // OffsetToRVATranslator:
    rva_t Convert(offset_t offset) override {
      if (offset >= self_->fake_offset_adjust_) {
        // |offset| is an untranslated RVA that was shifted outside the image.
        return offset_t(offset - self_->fake_offset_adjust_);
      }
      if (section_ == nullptr || offset < section_->file_offset_of_raw_data ||
          offset >=
              section_->file_offset_of_raw_data + section_->size_of_raw_data) {
        section_ = self_->OffsetToSection(offset);
        DCHECK_NE(section_, nullptr);
        adjust_ = section_->virtual_address - section_->file_offset_of_raw_data;
      }
      return offset_t(offset + adjust_);
    }

   private:
    const DisassemblerWin32* self_;
    const pe::ImageSectionHeader* section_ = nullptr;
    std::ptrdiff_t adjust_ = 0;
  };
  return base::MakeUnique<OffsetToRVA>(this);
}

template <class Traits>
bool DisassemblerWin32<Traits>::Parse(ConstBufferView image) {
  image_ = image;
  return ParseHeader();
}

template <class Traits>
bool DisassemblerWin32<Traits>::ParseHeader() {
  BufferSource source;

  if (!ReadWin32Header<Traits>(image_, &source))
    return false;

  auto* coff_header = source.GetPointer<pe::ImageFileHeader>();
  if (!coff_header ||
      coff_header->size_of_optional_header <
          offsetof(typename Traits::ImageOptionalHeader, data_directory)) {
    return false;
  }

  auto* optional_header =
      source.GetPointer<typename Traits::ImageOptionalHeader>();
  if (!optional_header || optional_header->magic != Traits::kMagic)
    return false;

  const size_t kDataDirBase =
      offsetof(typename Traits::ImageOptionalHeader, data_directory);
  size_t size_of_optional_header = coff_header->size_of_optional_header;
  if (size_of_optional_header < kDataDirBase)
    return false;

  const size_t data_dir_bound =
      (size_of_optional_header - kDataDirBase) / sizeof(pe::ImageDataDirectory);
  if (optional_header->number_of_rva_and_sizes > data_dir_bound)
    return false;

  // TODO(huangs): Read base relocation table here.

  image_base_ = optional_header->image_base;

  // |optional_header->size_of_image| is the size of the image when loaded into
  // memory, and not the actual size on disk.
  rva_bound_ = optional_header->size_of_image;
  if (rva_bound_ >= kRVABound)
    return false;

  // An exclusive upper bound of all offsets used in the image. This gets
  // updated as sections get visited.
  offset_t offset_bound =
      base::checked_cast<offset_t>(source.begin() - image_.begin());

  // Extract and validate all sections.
  size_t sections_count = coff_header->number_of_sections;
  auto* sections_array =
      source.GetArray<pe::ImageSectionHeader>(sections_count);
  if (!sections_array)
    return false;
  sections_.assign(sections_array, sections_array + sections_count);
  section_rva_map_.resize(sections_count);
  section_offset_map_.resize(sections_count);
  bool has_text_section = false;
  decltype(pe::ImageSectionHeader::virtual_address) prev_virtual_address = 0;
  for (size_t i = 0; i < sections_count; ++i) {
    const pe::ImageSectionHeader& section = sections_[i];
    // Apply strict checks on section bounds.
    if (!image_.covers(
            {section.file_offset_of_raw_data, section.size_of_raw_data})) {
      return false;
    }
    if (!RangeIsBounded(section.virtual_address, section.virtual_size,
                        rva_bound_)) {
      return false;
    }

    // PE sections should be sorted by RVAs. For robustness, we don't rely on
    // this, so even if unsorted we don't care. Output warning though.
    if (prev_virtual_address > section.virtual_address)
      LOG(WARNING) << "RVA anomaly found for Section " << i;
    prev_virtual_address = section.virtual_address;

    section_rva_map_[i] = std::make_pair(section.virtual_address, i);
    section_offset_map_[i] = std::make_pair(section.file_offset_of_raw_data, i);
    offset_t end_offset =
        section.file_offset_of_raw_data + section.size_of_raw_data;
    offset_bound = std::max(end_offset, offset_bound);
    if (IsWin32CodeSection<Traits>(section))
      has_text_section = true;
  }

  CHECK_LE(offset_bound, image_.size());
  if (!has_text_section)
    return false;

  // Resize |image_| to include only contents claimed by sections. Note that
  // this may miss digital signatures at end of PE files, but for patching this
  // is of minor concern.
  image_.shrink(offset_bound);

  fake_offset_adjust_ = std::max(offset_bound, rva_bound_);

  std::sort(section_rva_map_.begin(), section_rva_map_.end());
  std::sort(section_offset_map_.begin(), section_offset_map_.end());
  return true;
}

template <class Traits>
bool DisassemblerWin32<Traits>::ParseAndStoreRel32() {
  if (has_parsed_rel32_)
    return true;
  has_parsed_rel32_ = true;

  // TODO(huangs): ParseAndStoreAbs32() once it's available.

  for (const pe::ImageSectionHeader& section : sections_) {
    if (!IsWin32CodeSection<Traits>(section))
      continue;

    std::ptrdiff_t from_offset_to_rva =
        section.virtual_address - section.file_offset_of_raw_data;
    rva_t start_rva = section.virtual_address;
    rva_t end_rva = start_rva + section.virtual_size;

    ConstBufferView region =
        image_[{section.file_offset_of_raw_data, section.size_of_raw_data}];
    // TODO(huangs): Initialize with |abs32_locations_|, once it's available.
    Abs32GapFinder gap_finder(image_, region, std::vector<offset_t>(),
                              Traits::kVAWidth);
    typename Traits::RelFinder finder(image_);
    // Iterate over gaps between abs32 references, to avoid collision.
    for (auto gap = gap_finder.GetNext(); gap.has_value();
         gap = gap_finder.GetNext()) {
      finder.Reset(gap.value());
      // Iterate over heuristically detected rel32 references, validate, and add
      // to |rel32_locations_|.
      for (auto rel32 = finder.GetNext(); rel32.has_value();
           rel32 = finder.GetNext()) {
        offset_t rel32_offset = offset_t(rel32->location - image_.begin());
        rva_t rel32_rva = rva_t(rel32_offset + from_offset_to_rva);
        rva_t target_rva =
            rel32_rva + 4 + *reinterpret_cast<const uint32_t*>(rel32->location);
        if (target_rva < rva_bound_ &&  // Subsumes rva != kUnassignedRVA.
            (rel32->can_point_outside_section ||
             (start_rva <= target_rva && target_rva < end_rva))) {
          finder.Accept();
          rel32_locations_.push_back(rel32_offset);
        }
      }
    }
  }
  rel32_locations_.shrink_to_fit();
  // |sections_| entries are usually sorted by offset, but there's no guarantee.
  // So sort explicitly, to be sure.
  std::sort(rel32_locations_.begin(), rel32_locations_.end());
  return true;
}

template <class Traits>
const pe::ImageSectionHeader* DisassemblerWin32<Traits>::RVAToSection(
    rva_t rva) const {
  auto it = std::upper_bound(
      section_rva_map_.begin(), section_rva_map_.end(), rva,
      [](rva_t a, const std::pair<rva_t, int>& b) { return a < b.first; });
  if (it == section_rva_map_.begin())
    return nullptr;
  --it;
  const pe::ImageSectionHeader* section = &sections_[it->second];
  return rva - it->first < section->size_of_raw_data ? section : nullptr;
}

template <class Traits>
const pe::ImageSectionHeader* DisassemblerWin32<Traits>::OffsetToSection(
    offset_t offset) const {
  auto it = std::upper_bound(section_offset_map_.begin(),
                             section_offset_map_.end(), offset,
                             [](offset_t a, const std::pair<offset_t, int>& b) {
                               return a < b.first;
                             });
  if (it == section_offset_map_.begin())
    return nullptr;
  --it;
  const pe::ImageSectionHeader* section = &sections_[it->second];
  return offset - it->first < section->size_of_raw_data ? section : nullptr;
}

template <class Traits>
offset_t DisassemblerWin32<Traits>::RVAToOffset(rva_t rva) const {
  const pe::ImageSectionHeader* section = RVAToSection(rva);
  if (section == nullptr ||
      rva >= section->virtual_address + section->virtual_size) {
    return rva + fake_offset_adjust_;
  }
  return offset_t(rva - section->virtual_address) +
         section->file_offset_of_raw_data;
}

template <class Traits>
rva_t DisassemblerWin32<Traits>::OffsetToRVA(offset_t offset) const {
  if (offset >= fake_offset_adjust_)
    return rva_t(offset - fake_offset_adjust_);
  const pe::ImageSectionHeader* section = OffsetToSection(offset);
  DCHECK_NE(section, nullptr);
  return rva_t(offset - section->file_offset_of_raw_data) +
         section->virtual_address;
}

template <class Traits>
rva_t DisassemblerWin32<Traits>::AddressToRVA(
    typename Traits::Address address) const {
  return rva_t(address - image_base_);
}

template <class Traits>
typename Traits::Address DisassemblerWin32<Traits>::RVAToAddress(
    rva_t rva) const {
  return rva + image_base_;
}

// Explicit instantiation for supported classes.
template class DisassemblerWin32<Win32X86Traits>;
template class DisassemblerWin32<Win32X64Traits>;

}  // namespace zucchini
