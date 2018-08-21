// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/callback_android.h"
#include "base/android/jni_string.h"
#include "base/json/json_string_value_serializer.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "jni/RenderFrameHostTestExt_jni.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace content {

namespace {

void OnExecuteJavaScriptResult(const JavaRef<jobject>& jcallback,
                               const base::Value* value) {
  std::string result;
  JSONStringValueSerializer serializer(&result);
  bool value_serialized = serializer.SerializeAndOmitBinaryValues(*value);
  DCHECK(value_serialized);
  base::android::RunStringCallbackAndroid(jcallback, result);
}

}  // namespace

void JNI_RenderFrameHostTestExt_ExecuteJavaScriptForTests(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jstring>& jscript,
    const JavaParamRef<jobject>& jcallback,
    jlong render_frame_host_ptr) {
  base::string16 script(ConvertJavaStringToUTF16(env, jscript));
  auto callback = base::BindRepeating(
      &OnExecuteJavaScriptResult,
      base::android::ScopedJavaGlobalRef<jobject>(env, jcallback));
  RenderFrameHost* render_frame_host =
      reinterpret_cast<RenderFrameHost*>(render_frame_host_ptr);
  render_frame_host->ExecuteJavaScriptForTests(script, callback);
}

}  // namespace content
