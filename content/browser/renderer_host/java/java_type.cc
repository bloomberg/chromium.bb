// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/java/java_type.h"

JavaType JavaType::CreateFromBinaryName(const std::string& binary_name) {
  JavaType result;
  if (binary_name == "boolean") {
    result.type = JavaType::TypeBoolean;
  } else if (binary_name == "byte") {
    result.type = JavaType::TypeByte;
  } else if (binary_name == "char") {
    result.type = JavaType::TypeChar;
  } else if (binary_name == "short") {
    result.type = JavaType::TypeShort;
  } else if (binary_name == "int") {
    result.type = JavaType::TypeInt;
  } else if (binary_name == "long") {
    result.type = JavaType::TypeLong;
  } else if (binary_name == "float") {
    result.type = JavaType::TypeFloat;
  } else if (binary_name == "double") {
    result.type = JavaType::TypeDouble;
  } else if (binary_name == "void") {
    result.type = JavaType::TypeVoid;
  } else if (binary_name[0] == '[') {
    result.type = JavaType::TypeArray;
  } else if (binary_name == "java.lang.String") {
    result.type = JavaType::TypeString;
  } else {
    result.type = JavaType::TypeObject;
  }
  return result;
}
