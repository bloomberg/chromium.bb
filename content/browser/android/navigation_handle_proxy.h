// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_NAVIGATION_HANDLE_PROXY_H_
#define CONTENT_BROWSER_ANDROID_NAVIGATION_HANDLE_PROXY_H_

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "net/http/http_request_headers.h"

#include <string>
#include <vector>

namespace content {

class NavigationHandle;

// JNI bridge for using a content::NavigationHandle from Java.
class NavigationHandleProxy {
 public:
  explicit NavigationHandleProxy(content::NavigationHandle* navigation_handle)
      : navigation_handle_(navigation_handle) {}

  // Called from Java.
  void SetRequestHeader(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj,
                        const base::android::JavaParamRef<jstring>& name,
                        const base::android::JavaParamRef<jstring>& value);

  // Called from Java.
  void RemoveRequestHeader(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj,
                           const base::android::JavaParamRef<jstring>& name);

  jlong JavaThis() const { return reinterpret_cast<jlong>(this); }

 private:
  content::NavigationHandle* navigation_handle_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_NAVIGATION_HANDLE_PROXY_H_
