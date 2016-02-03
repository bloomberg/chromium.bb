// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_TOOLBAR_TOOLBAR_MODEL_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_TOOLBAR_TOOLBAR_MODEL_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/toolbar/chrome_toolbar_model_delegate.h"
#include "components/toolbar/toolbar_model.h"

namespace content {
class WebContents;
}  // content

// Owns a ToolbarModel and provides a way for Java to interact with it.
class ToolbarModelAndroid : public ChromeToolbarModelDelegate {
 public:
  explicit ToolbarModelAndroid(JNIEnv* env, jobject jdelegate);
  ~ToolbarModelAndroid() override;

  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  base::android::ScopedJavaLocalRef<jstring> GetText(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  base::android::ScopedJavaLocalRef<jstring> GetCorpusChipText(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  jboolean WouldReplaceURL(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj);

  // ChromeToolbarModelDelegate:
  content::WebContents* GetActiveWebContents() const override;

  static bool RegisterToolbarModelAndroid(JNIEnv* env);

 private:
  scoped_ptr<ToolbarModel> toolbar_model_;
  JavaObjectWeakGlobalRef weak_java_delegate_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ToolbarModelAndroid);
};

#endif  // CHROME_BROWSER_UI_ANDROID_TOOLBAR_TOOLBAR_MODEL_ANDROID_H_
