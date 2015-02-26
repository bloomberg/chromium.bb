// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_APP_CONTENT_JNI_ONLOAD_H_
#define CONTENT_PUBLIC_APP_CONTENT_JNI_ONLOAD_H_

#include <jni.h>

#include "base/android/base_jni_onload.h"
#include "content/common/content_export.h"

namespace content {
namespace android {

// Returns true if JNI registration succeeded.
CONTENT_EXPORT bool OnJNIOnLoadRegisterJNI(
    JavaVM* vm,
    std::vector<base::android::RegisterCallback> callbacks);

// Returns true if initialization succeeded.
CONTENT_EXPORT bool OnJNIOnLoadInit(
    std::vector<base::android::InitCallback> callbacks);

}  // namespace android
}  // namespace content

#endif  // CONTENT_PUBLIC_APP_CONTENT_JNI_ONLOAD_H_
