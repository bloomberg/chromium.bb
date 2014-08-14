// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_COMMON_ANDROID_COMPONENT_JNI_REGISTRAR_H_
#define COMPONENTS_BOOKMARKS_COMMON_ANDROID_COMPONENT_JNI_REGISTRAR_H_

#include <jni.h>

namespace bookmarks {
namespace android {

bool RegisterBookmarks(JNIEnv* env);

}  // namespace android
}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_COMMON_ANDROID_COMPONENT_JNI_REGISTRAR_H_
