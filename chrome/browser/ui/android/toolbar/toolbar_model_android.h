// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_TOOLBAR_TOOLBAR_MODEL_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_TOOLBAR_TOOLBAR_MODEL_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/toolbar/toolbar_model.h"
#include "chrome/browser/ui/toolbar/toolbar_model_delegate.h"

namespace content {
class WebContents;
}  // content

// Owns a ToolbarModel and provides a way for Java to interact with it.
class ToolbarModelAndroid : public ToolbarModelDelegate {
 public:
  explicit ToolbarModelAndroid(JNIEnv* env, jobject jdelegate);
  virtual ~ToolbarModelAndroid();

  void Destroy(JNIEnv* env, jobject obj);
  base::android::ScopedJavaLocalRef<jstring> GetText(
      JNIEnv* env,
      jobject obj);
  base::android::ScopedJavaLocalRef<jstring> GetQueryExtractionParam(
      JNIEnv* env,
      jobject obj);
  base::android::ScopedJavaLocalRef<jstring> GetCorpusChipText(
      JNIEnv* env,
      jobject obj);

  // ToolbarDelegate:
  virtual content::WebContents* GetActiveWebContents() const OVERRIDE;
  virtual bool InTabbedBrowser() const OVERRIDE;

  static bool RegisterToolbarModelAndroid(JNIEnv* env);

 private:
  scoped_ptr<ToolbarModel> toolbar_model_;
  JavaObjectWeakGlobalRef weak_java_delegate_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ToolbarModelAndroid);
};

#endif  // CHROME_BROWSER_UI_ANDROID_TOOLBAR_TOOLBAR_MODEL_ANDROID_H_
