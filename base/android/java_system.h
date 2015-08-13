// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_JAVA_SYSTEM_H_
#define BASE_ANDROID_JAVA_SYSTEM_H_

#include <string>

#include "base/android/jni_android.h"
#include "base/base_export.h"
#include "base/strings/string_piece.h"

namespace base {
namespace android {

// Wrapper class for using the java.lang.System object from jni.
class BASE_EXPORT JavaSystem {
 public:
  // Registers the jni class (once per process).
  static bool Register(JNIEnv* env);

  // See java.lang.System.getProperty().
  static std::string GetProperty(const base::StringPiece& stroperty_name);
};

}  // namespace android
}  // namespace base

#endif  // BASE_ANDROID_JAVA_SYSTEM_H_
