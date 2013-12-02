// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_COMMON_COMMON_SIMILARITY_TRAITS_H_
#define MOJO_COMMON_COMMON_SIMILARITY_TRAITS_H_

#include "base/strings/string_piece.h"
#include "mojo/common/mojo_common_export.h"
#include "mojo/public/bindings/lib/bindings.h"

namespace mojo {

template <>
class MOJO_COMMON_EXPORT SimilarityTraits<String, base::StringPiece> {
 public:
  static String CopyFrom(const base::StringPiece& input, Buffer* buf);
  static base::StringPiece CopyTo(const String& input);
};

}  // namespace mojo

#endif  // MOJO_COMMON_COMMON_SIMILARITY_TRAITS_H_
