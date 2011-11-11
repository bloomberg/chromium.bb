// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_JAVA_JAVA_METHOD_H_
#define CONTENT_BROWSER_RENDERER_HOST_JAVA_JAVA_METHOD_H_

#include <jni.h>
#include <string>
#include <vector>

#include "base/android/scoped_java_ref.h"

namespace JavaType {
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
}  // namespace JavaType

// Wrapper around java.lang.reflect.Method. This class must be used on a single
// thread only.
class JavaMethod {
 public:
  explicit JavaMethod(const base::android::JavaRef<jobject>& method);
  ~JavaMethod();

  const std::string& name() const { return name_; }
  size_t num_parameters() const;
  JavaType::Type parameter_type(size_t index) const;
  JavaType::Type return_type() const;
  jmethodID id() const;

 private:
  void EnsureNumParametersIsSetUp() const;
  void EnsureTypesAndIDAreSetUp() const;

  std::string name_;
  mutable base::android::ScopedJavaGlobalRef<jobject> java_method_;
  mutable bool have_calculated_num_parameters_;
  mutable size_t num_parameters_;
  mutable std::vector<JavaType::Type> parameter_types_;
  mutable JavaType::Type return_type_;
  mutable jmethodID id_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(JavaMethod);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_JAVA_JAVA_METHOD_H_
