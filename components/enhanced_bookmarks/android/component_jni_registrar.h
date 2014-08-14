// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENHANCED_BOOKMARKS_ANDROID_COMPONENT_JNI_REGISTRAR_H_
#define COMPONENTS_ENHANCED_BOOKMARKS_ANDROID_COMPONENT_JNI_REGISTRAR_H_

#include <jni.h>

namespace enhanced_bookmarks {
namespace android {

bool RegisterEnhancedBookmarks(JNIEnv* env);

}  // namespace android
}  // namespace enhanced_bookmarks

#endif  // COMPONENTS_ENHANCED_BOOKMARKS_ANDROID_COMPONENT_JNI_REGISTRAR_H_
