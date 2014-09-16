// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_COMMON_COMMON_TYPE_CONVERTERS_H_
#define MOJO_COMMON_COMMON_TYPE_CONVERTERS_H_

#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "mojo/common/mojo_common_export.h"
#include "mojo/public/cpp/bindings/string.h"
#include "mojo/public/cpp/bindings/type_converter.h"

class GURL;

namespace mojo {

template <>
struct MOJO_COMMON_EXPORT TypeConverter<String, base::StringPiece> {
  static String Convert(const base::StringPiece& input);
};

template <>
struct MOJO_COMMON_EXPORT TypeConverter<base::StringPiece, String> {
  static base::StringPiece Convert(const String& input);
};

template <>
struct MOJO_COMMON_EXPORT TypeConverter<String, base::string16> {
  static String Convert(const base::string16& input);
};

template <>
struct MOJO_COMMON_EXPORT TypeConverter<base::string16, String> {
  static base::string16 Convert(const String& input);
};

template <>
struct MOJO_COMMON_EXPORT TypeConverter<String, GURL> {
  static String Convert(const GURL& input);
};

template <>
struct MOJO_COMMON_EXPORT TypeConverter<GURL, String> {
  static GURL Convert(const String& input);
};

}  // namespace mojo

#endif  // MOJO_COMMON_COMMON_TYPE_CONVERTERS_H_
