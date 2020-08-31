// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/disassembler.h"

#include "base/check_op.h"

namespace zucchini {

/******** EmptyReferenceReader ********/

base::Optional<Reference> EmptyReferenceReader::GetNext() {
  return base::nullopt;
}

/******** EmptyReferenceWriter ********/

void EmptyReferenceWriter::PutNext(Reference /* reference */) {}

/******** ReferenceGroup ********/

std::unique_ptr<ReferenceReader> ReferenceGroup::GetReader(
    offset_t lower,
    offset_t upper,
    Disassembler* disasm) const {
  DCHECK_LE(lower, upper);
  DCHECK_LE(upper, disasm->size());
  return (disasm->*reader_factory_)(lower, upper);
}

std::unique_ptr<ReferenceReader> ReferenceGroup::GetReader(
    Disassembler* disasm) const {
  return (disasm->*reader_factory_)(0, static_cast<offset_t>(disasm->size()));
}

std::unique_ptr<ReferenceWriter> ReferenceGroup::GetWriter(
    MutableBufferView image,
    Disassembler* disasm) const {
  DCHECK_EQ(image.begin(), disasm->image().begin());
  DCHECK_EQ(image.size(), disasm->size());
  return (disasm->*writer_factory_)(image);
}

/******** Disassembler ********/

Disassembler::Disassembler(int num_equivalence_iterations)
    : num_equivalence_iterations_(num_equivalence_iterations) {}

Disassembler::~Disassembler() = default;

}  // namespace zucchini
