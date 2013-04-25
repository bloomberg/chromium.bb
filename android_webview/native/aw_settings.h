// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NATIVE_AW_SETTINGS_H_
#define ANDROID_WEBVIEW_NATIVE_AW_SETTINGS_H_

#include <jni.h>

#include "base/android/jni_helper.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/web_contents_observer.h"

namespace android_webview {

class AwRenderViewHostExt;

class AwSettings : public content::WebContentsObserver {
 public:
  AwSettings(JNIEnv* env, jobject obj);
  virtual ~AwSettings();

  // Called from Java.
  void Destroy(JNIEnv* env, jobject obj);
  void ResetScrollAndScaleState(JNIEnv* env, jobject obj);
  void SetWebContents(JNIEnv* env, jobject obj, jint web_contents);
  void UpdateEverything(JNIEnv* env, jobject obj);
  void UpdateInitialPageScale(JNIEnv* env, jobject obj);
  void UpdateUserAgent(JNIEnv* env, jobject obj);
  void UpdateWebkitPreferences(JNIEnv* env, jobject obj);

 private:
  struct FieldIds;

  AwRenderViewHostExt* GetAwRenderViewHostExt();
  void UpdateEverything();
  void UpdatePreferredSizeMode();

  // WebContentsObserver overrides:
  virtual void RenderViewCreated(
      content::RenderViewHost* render_view_host) OVERRIDE;

  // Java field references for accessing the values in the Java object.
  scoped_ptr<FieldIds> field_ids_;

  JavaObjectWeakGlobalRef aw_settings_;
};

bool RegisterAwSettings(JNIEnv* env);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_NATIVE_AW_SETTINGS_H_
