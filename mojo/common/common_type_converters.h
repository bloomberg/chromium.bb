// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_COMMON_COMMON_TYPE_CONVERTERS_H_
#define MOJO_COMMON_COMMON_TYPE_CONVERTERS_H_

#include "base/strings/string_piece.h"
#include "mojo/common/mojo_common_export.h"
#include "mojo/public/bindings/array.h"
#include "mojo/public/bindings/type_converter.h"

namespace mojo {

template <>
class MOJO_COMMON_EXPORT TypeConverter<String, base::StringPiece> {
 public:
  static String ConvertFrom(const base::StringPiece& input, Buffer* buf);
  static base::StringPiece ConvertTo(const String& input);
};

}  // namespace mojo

#endif  // MOJO_COMMON_COMMON_TYPE_CONVERTERS_H_
