// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/test_reference_reader.h"

namespace zucchini {

TestReferenceReader::TestReferenceReader(const std::vector<Reference>& refs)
    : references_(refs) {}

TestReferenceReader::~TestReferenceReader() = default;

base::Optional<Reference> TestReferenceReader::GetNext() {
  if (index_ == references_.size())
    return base::nullopt;
  return references_[index_++];
}

}  // namespace zucchini
