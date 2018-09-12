// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/android/web_contents_utils.h"

#include "base/json/json_writer.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_frame_metadata_provider.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "jni/WebContentsUtils_jni.h"

using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace content {

namespace {

void JavaScriptResultCallback(const ScopedJavaGlobalRef<jobject>& callback,
                              const base::Value* result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  std::string json;
  base::JSONWriter::Write(*result, &json);
  ScopedJavaLocalRef<jstring> j_json = ConvertUTF8ToJavaString(env, json);
  Java_WebContentsUtils_onEvaluateJavaScriptResult(env, j_json, callback);
}

}  // namespace

// Reports all frame submissions to the browser process, even those that do not
// impact Browser UI.
void JNI_WebContentsUtils_ReportAllFrameSubmissions(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jobject>& jweb_contents,
    jboolean enabled) {
  WebContents* web_contents = WebContents::FromJavaWebContents(jweb_contents);
  RenderFrameMetadataProvider* provider =
      RenderWidgetHostImpl::From(web_contents->GetRenderViewHost()->GetWidget())
          ->render_frame_metadata_provider();
  provider->ReportAllFrameSubmissionsForTesting(enabled);
}

ScopedJavaLocalRef<jobject> JNI_WebContentsUtils_GetFocusedFrame(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jobject>& jweb_contents) {
  WebContents* web_contents = WebContents::FromJavaWebContents(jweb_contents);
  return static_cast<RenderFrameHostImpl*>(web_contents->GetFocusedFrame())
      ->GetJavaRenderFrameHost();
}

void JNI_WebContentsUtils_EvaluateJavaScript(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jobject>& jweb_contents,
    const JavaParamRef<jstring>& script,
    const JavaParamRef<jobject>& callback) {
  EvaluateJavaScript(env, jweb_contents, script, callback);
}

void EvaluateJavaScript(JNIEnv* env,
                        const JavaParamRef<jobject>& jweb_contents,
                        const JavaParamRef<jstring>& script,
                        const JavaParamRef<jobject>& callback) {
  WebContents* web_contents = WebContents::FromJavaWebContents(jweb_contents);
  RenderViewHost* rvh = web_contents->GetRenderViewHost();
  DCHECK(rvh);

  if (!rvh->IsRenderViewLive()) {
    if (!static_cast<WebContentsImpl*>(web_contents)
             ->CreateRenderViewForInitialEmptyDocument()) {
      LOG(ERROR) << "Failed to create RenderView in EvaluateJavaScriptForTests";
      return;
    }
  }

  if (!callback) {
    // No callback requested.
    web_contents->GetMainFrame()->ExecuteJavaScriptForTests(
        ConvertJavaStringToUTF16(env, script));
    return;
  }

  // Secure the Java callback in a scoped object and give ownership of it to the
  // base::Callback.
  ScopedJavaGlobalRef<jobject> j_callback;
  j_callback.Reset(env, callback);
  RenderFrameHost::JavaScriptResultCallback js_callback =
      base::Bind(&JavaScriptResultCallback, j_callback);

  web_contents->GetMainFrame()->ExecuteJavaScriptForTests(
      ConvertJavaStringToUTF16(env, script), js_callback);
}

}  // namespace content
