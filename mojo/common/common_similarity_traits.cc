// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/common/common_similarity_traits.h"

namespace mojo {

// static
String SimilarityTraits<String, base::StringPiece>::CopyFrom(
    const base::StringPiece& input,
    Buffer* buf) {
  String::Builder result(input.size(), buf);
  memcpy(&result[0], input.data(), input.size());
  return result.Finish();
}
// static
base::StringPiece SimilarityTraits<String, base::StringPiece>::CopyTo(
    const String& input) {
  return input.is_null() ? base::StringPiece() :
                           base::StringPiece(&input[0], input.size());
}

}  // namespace mojo
