// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_BASE_JNI_ONLOAD_H_
#define BASE_ANDROID_BASE_JNI_ONLOAD_H_

#include <jni.h>
#include <vector>

#include "base/base_export.h"

namespace base {
namespace android {

class JNIOnLoadDelegate;

// Returns whether JNI registration and initialization succeeded. Caller shall
// put the JNIOnLoadDelegate into |delegates| in reverse order. Refer
// JNIOnLoadDelegate for more information.
BASE_EXPORT bool OnJNIOnLoad(JavaVM* vm,
                             std::vector<JNIOnLoadDelegate*>* delegates);

}  // namespace android
}  // namespace base

#endif  // BASE_ANDROID_BASE_JNI_ONLOAD_H_
