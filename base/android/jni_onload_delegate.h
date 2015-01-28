// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_JNI_ONLOAD_DELEGATE_H_
#define BASE_ANDROID_JNI_ONLOAD_DELEGATE_H_

#include <jni.h>

#include "base/base_export.h"

namespace base {
namespace android {

// This delegate class is used to implement component specific JNI registration
// and initialization. All methods are called in JNI_OnLoad().
//
// Both RegisterJNI() and Init() methods are called if the shared library
// is loaded by crazy linker that can't find JNI methods without JNI
// registration, otherwise, only Init() is invoked where dynamic lookup is
// used to find the JNI methods.
//
// It is important to make sure the JNI registration code is only in
// RegisterJNI(), so it could be stripped out when JNI registration isn't
// needed.
class BASE_EXPORT JNIOnLoadDelegate {
 public:
  virtual ~JNIOnLoadDelegate() {}

  // Returns whether the JNI registration succeeded.
  virtual bool RegisterJNI(JNIEnv* env) = 0;

  // Returns whether the initialization succeeded. This method is called after
  // RegisterJNI(), all JNI methods shall ready to be used.
  virtual bool Init() = 0;
};

}  // namespace android
}  // namespace base

#endif  // BASE_ANDROID_JNI_ONLOAD_DELEGATE_H_
