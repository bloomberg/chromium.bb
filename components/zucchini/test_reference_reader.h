// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_TEST_REFERENCE_READER_H_
#define COMPONENTS_ZUCCHINI_TEST_REFERENCE_READER_H_

#include <stddef.h>

#include <vector>

#include "base/optional.h"
#include "components/zucchini/image_utils.h"

namespace zucchini {

// A trivial ReferenceReader that reads injected references.
class TestReferenceReader : public ReferenceReader {
 public:
  explicit TestReferenceReader(const std::vector<Reference>& refs);
  ~TestReferenceReader() override;

  base::Optional<Reference> GetNext() override;

 private:
  std::vector<Reference> references_;
  size_t index_ = 0;
};

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_TEST_REFERENCE_READER_H_
