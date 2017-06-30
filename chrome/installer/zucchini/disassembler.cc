// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/zucchini/disassembler.h"

#include "base/logging.h"

namespace zucchini {

std::unique_ptr<ReferenceReader> ReferenceGroup::GetReader(
    offset_t lower,
    offset_t upper,
    Disassembler* disasm) const {
  DCHECK(lower <= upper);
  DCHECK(upper <= disasm->size());
  return (disasm->*reader_factory_)(lower, upper);
}

std::unique_ptr<ReferenceReader> ReferenceGroup::GetReader(
    Disassembler* disasm) const {
  return (disasm->*reader_factory_)(0, static_cast<offset_t>(disasm->size()));
}

std::unique_ptr<ReferenceWriter> ReferenceGroup::GetWriter(
    MutableBufferView image,
    Disassembler* disasm) const {
  DCHECK(image.begin() == disasm->GetImage().begin());
  DCHECK(image.size() == disasm->size());
  return (disasm->*writer_factory_)(image);
}

ReferenceTypeTraits ReferenceGroup::Traits() const {
  return traits_;
}

offset_t ReferenceGroup::Width() const {
  return traits_.width;
}

TypeTag ReferenceGroup::TypeTag() const {
  return traits_.type_tag;
}

PoolTag ReferenceGroup::PoolTag() const {
  return traits_.pool_tag;
}

Disassembler::Disassembler() = default;
Disassembler::~Disassembler() = default;

}  // namespace zucchini
