// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_INTERCEPTED_REQUEST_DATA_H_
#define CONTENT_BROWSER_ANDROID_INTERCEPTED_REQUEST_DATA_H_

#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/memory/ref_counted.h"

// This class represents the Java-side data that is to be used to complete a
// particular URLRequest.
class InterceptedRequestData {
 public:
  InterceptedRequestData(const base::android::JavaRef<jobject>& obj);
  ~InterceptedRequestData();

  base::android::ScopedJavaLocalRef<jobject> GetInputStream(JNIEnv* env) const;
  bool GetMimeType(JNIEnv* env, std::string* mime_type) const;
  bool GetCharset(JNIEnv* env, std::string* charset) const;

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  DISALLOW_COPY_AND_ASSIGN(InterceptedRequestData);
};

bool RegisterInterceptedRequestData(JNIEnv* env);

#endif  // CONTENT_BROWSER_ANDROID_INTERCEPTED_REQUEST_DATA_H_
