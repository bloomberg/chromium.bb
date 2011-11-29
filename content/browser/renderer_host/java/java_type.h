// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_JAVA_JAVA_TYPE_H_
#define CONTENT_BROWSER_RENDERER_HOST_JAVA_JAVA_TYPE_H_

#include <string>

struct JavaType {
  // Java's reflection API represents types as a string using an extended
  // 'binary name'.
  static JavaType CreateFromBinaryName(const std::string& binary_name);

  enum Type {
    TypeBoolean,
    TypeByte,
    TypeChar,
    TypeShort,
    TypeInt,
    TypeLong,
    TypeFloat,
    TypeDouble,
    // This is only used as a return type, so we should never convert from
    // JavaScript with this type.
    TypeVoid,
    TypeArray,
    // We special-case strings, as they get special handling when coercing.
    TypeString,
    TypeObject,
  };

  Type type;
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_JAVA_JAVA_TYPE_H_
