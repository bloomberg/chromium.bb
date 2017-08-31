// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/zucchini/test_disassembler.h"

#include "base/memory/ptr_util.h"
#include "chrome/installer/zucchini/test_reference_reader.h"

namespace zucchini {

TestDisassembler::TestDisassembler(const ReferenceTypeTraits& traits1,
                                   const std::vector<Reference>& refs1,
                                   const ReferenceTypeTraits& traits2,
                                   const std::vector<Reference>& refs2,
                                   const ReferenceTypeTraits& traits3,
                                   const std::vector<Reference>& refs3)
    : traits_{traits1, traits2, traits3}, refs_{refs1, refs2, refs3} {}

TestDisassembler::~TestDisassembler() = default;

ExecutableType TestDisassembler::GetExeType() const {
  return kExeTypeUnknown;
}

std::string TestDisassembler::GetExeTypeString() const {
  return "(Unknown)";
}

std::vector<ReferenceGroup> TestDisassembler::MakeReferenceGroups() const {
  return {
      {traits_[0], &TestDisassembler::MakeReadRefs1,
       &TestDisassembler::MakeWriteRefs1},
      {traits_[1], &TestDisassembler::MakeReadRefs2,
       &TestDisassembler::MakeWriteRefs2},
      {traits_[2], &TestDisassembler::MakeReadRefs3,
       &TestDisassembler::MakeWriteRefs3},
  };
}

bool TestDisassembler::Parse(ConstBufferView image) {
  return true;
}

std::unique_ptr<ReferenceReader> TestDisassembler::MakeReadRefs(int type) {
  return base::MakeUnique<TestReferenceReader>(refs_[type]);
}

std::unique_ptr<ReferenceWriter> TestDisassembler::MakeWriteRefs(
    MutableBufferView image) {
  class NoOpWriter : public ReferenceWriter {
   public:
    // ReferenceWriter:
    void PutNext(Reference) override {}
  };
  return base::MakeUnique<NoOpWriter>();
}

}  // namespace zucchini
