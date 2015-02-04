// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_JAVASCRIPT_APP_MODAL_DIALOG_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_JAVASCRIPT_APP_MODAL_DIALOG_ANDROID_H_

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/scoped_ptr.h"
#include "components/app_modal/native_app_modal_dialog.h"

class JavaScriptAppModalDialog;

class JavascriptAppModalDialogAndroid
    : public app_modal::NativeAppModalDialog {
 public:
  JavascriptAppModalDialogAndroid(
      JNIEnv* env,
      app_modal::JavaScriptAppModalDialog* dialog,
      gfx::NativeWindow parent);

  // NativeAppModalDialog:
  int GetAppModalDialogButtons() const override;
  void ShowAppModalDialog() override;
  void ActivateAppModalDialog() override;
  void CloseAppModalDialog() override;
  void AcceptAppModalDialog() override;
  void CancelAppModalDialog() override;
  bool IsShowing() const override;

  // Called when java confirms or cancels the dialog.
  void DidAcceptAppModalDialog(JNIEnv* env,
                               jobject obj,
                               jstring prompt_text,
                               bool suppress_js_dialogs);
  void DidCancelAppModalDialog(JNIEnv* env, jobject, bool suppress_js_dialogs);

  const base::android::ScopedJavaGlobalRef<jobject>& GetDialogObject() const;

  static bool RegisterJavascriptAppModalDialog(JNIEnv* env);

 private:
  // The object deletes itself.
  ~JavascriptAppModalDialogAndroid() override;

  scoped_ptr<app_modal::JavaScriptAppModalDialog> dialog_;
  base::android::ScopedJavaGlobalRef<jobject> dialog_jobject_;
  JavaObjectWeakGlobalRef parent_jobject_weak_ref_;

  DISALLOW_COPY_AND_ASSIGN(JavascriptAppModalDialogAndroid);
};


#endif  // CHROME_BROWSER_UI_ANDROID_JAVASCRIPT_APP_MODAL_DIALOG_ANDROID_H_
