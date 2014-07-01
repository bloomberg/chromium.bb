// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_JAVA_JNI_HELPER_H_
#define CONTENT_BROWSER_ANDROID_JAVA_JNI_HELPER_H_

#include <jni.h>

#include "content/common/content_export.h"

namespace content {

// Gets the method ID from the class name. Clears the pending Java exception
// and returns NULL if the method is not found. Caches results.
// Strings passed to this function are held in the cache and MUST remain valid
// beyond the duration of all future calls to this function, across all
// threads. In practice, this means that the function should only be used with
// string constants.
CONTENT_EXPORT jmethodID GetMethodIDFromClassName(JNIEnv* env,
                                                  const char* class_name,
                                                  const char* method,
                                                  const char* jni_signature);

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_JAVA_JNI_HELPER_H_
