// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_ZUCCHINI_REL32_UTILS_H_
#define CHROME_INSTALLER_ZUCCHINI_REL32_UTILS_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "chrome/installer/zucchini/address_translator.h"
#include "chrome/installer/zucchini/buffer_view.h"
#include "chrome/installer/zucchini/disassembler.h"
#include "chrome/installer/zucchini/image_utils.h"

namespace zucchini {

// A visitor that emits References (locations and target) from a specified
// portion of an x86 / x64 image, given a list of valid locations.
class Rel32ReaderX86 : public ReferenceReader {
 public:
  // |image| is an image containing  x86 / x64 code in [|lo|, |hi|).
  // |locations| is a sorted list of offsets of rel32 reference locations.
  // |factory| is a factory of offset / RVA translators for |image|.
  Rel32ReaderX86(ConstBufferView image,
                 offset_t lo,
                 offset_t hi,
                 const std::vector<offset_t>* locations,
                 const AddressTranslatorFactory& factory);
  ~Rel32ReaderX86() override;

  // Returns the next reference, or base::nullopt if exhausted.
  base::Optional<Reference> GetNext() override;

 private:
  ConstBufferView image_;
  std::unique_ptr<RVAToOffsetTranslator> target_rva_to_offset_;
  std::unique_ptr<OffsetToRVATranslator> location_offset_to_rva_;
  const offset_t hi_;
  const std::vector<offset_t>::const_iterator last_;
  std::vector<offset_t>::const_iterator current_;

  DISALLOW_COPY_AND_ASSIGN(Rel32ReaderX86);
};

// Writer for x86 / x64 rel32 references.
class Rel32WriterX86 : ReferenceWriter {
 public:
  // |image| wraps the raw bytes of a binary in which rel32 references will be
  // written. |translator| is a suitable address translator for the binary
  // |image|.
  Rel32WriterX86(MutableBufferView image,
                 const AddressTranslatorFactory& factory);
  ~Rel32WriterX86() override;

  void PutNext(Reference ref) override;

 private:
  MutableBufferView image_;
  std::unique_ptr<OffsetToRVATranslator> target_offset_to_rva_;
  std::unique_ptr<OffsetToRVATranslator> location_offset_to_rva_;

  DISALLOW_COPY_AND_ASSIGN(Rel32WriterX86);
};

}  // namespace zucchini

#endif  // CHROME_INSTALLER_ZUCCHINI_REL32_UTILS_H_
