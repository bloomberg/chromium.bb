// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_APP_CONTENT_JNI_ONLOAD_H_
#define CONTENT_PUBLIC_APP_CONTENT_JNI_ONLOAD_H_

#include <jni.h>

#include "content/common/content_export.h"

namespace base {
namespace android {

class JNIOnLoadDelegate;

}  // namespace android
}  // namespace base

namespace content {
namespace android {

// Returns true if JNI registration and initialization succeeded. Refer to
// JNIOnLoadDelegate for more information.
CONTENT_EXPORT bool OnJNIOnLoad(
    JavaVM* vm,
    base::android::JNIOnLoadDelegate* delegate);

}  // namespace android
}  // namespace content

#endif  // CONTENT_PUBLIC_APP_CONTENT_JNI_ONLOAD_H_
