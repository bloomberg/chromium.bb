// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_WEB_SETTINGS_H_
#define CONTENT_BROWSER_ANDROID_WEB_SETTINGS_H_
#pragma once

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {

class WebSettings : public WebContentsObserver {
 public:
  WebSettings(JNIEnv* env, jobject obj,
              WebContents* contents,
              bool is_master_mode);
  virtual ~WebSettings();

  static bool RegisterWebSettings(JNIEnv* env);

  void Destroy(JNIEnv* env, jobject obj);

  // Synchronizes the Java settings from native settings.
  void SyncFromNative(JNIEnv* env, jobject obj);
  // Synchronizes the native settings from Java settings.
  void SyncToNative(JNIEnv* env, jobject obj);

 private:
  struct FieldIds;

  void SyncFromNativeImpl();
  void SyncToNativeImpl();

  // WebContentsObserver overrides:
  virtual void RenderViewCreated(RenderViewHost* render_view_host) OVERRIDE;

  // Determines whether a sync to native should be triggered when a new render
  // view is created.
  bool is_master_mode_;

  // Java field references for accessing the values in the Java object.
  scoped_ptr<FieldIds> field_ids_;

  // The Java counterpart to this class.
  base::android::ScopedJavaGlobalRef<jobject> web_settings_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_WEB_SETTINGS_H_
