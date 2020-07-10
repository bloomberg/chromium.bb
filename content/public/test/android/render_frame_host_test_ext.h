// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_ANDROID_RENDER_FRAME_HOST_TEST_EXT_H_
#define CONTENT_PUBLIC_TEST_ANDROID_RENDER_FRAME_HOST_TEST_EXT_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/supports_user_data.h"

namespace content {

class RenderFrameHostImpl;

class RenderFrameHostTestExt : public base::SupportsUserData::Data {
 public:
  explicit RenderFrameHostTestExt(RenderFrameHostImpl* rfhi);

  void ExecuteJavaScript(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj,
                         const base::android::JavaParamRef<jstring>& jscript,
                         const base::android::JavaParamRef<jobject>& jcallback);

 private:
  RenderFrameHostImpl* const render_frame_host_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_ANDROID_RENDER_FRAME_HOST_TEST_EXT_H_
