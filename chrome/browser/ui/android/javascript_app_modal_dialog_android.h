// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_JAVASCRIPT_APP_MODAL_DIALOG_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_JAVASCRIPT_APP_MODAL_DIALOG_ANDROID_H_

#include "base/android/jni_helper.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/app_modal_dialogs/native_app_modal_dialog.h"

class JavaScriptAppModalDialog;

class JavascriptAppModalDialogAndroid : public NativeAppModalDialog {
 public:
  JavascriptAppModalDialogAndroid(JNIEnv* env,
                                  JavaScriptAppModalDialog* dialog,
                                  gfx::NativeWindow parent);

  // NativeAppModalDialog:
  virtual int GetAppModalDialogButtons() const OVERRIDE;
  virtual void ShowAppModalDialog() OVERRIDE;
  virtual void ActivateAppModalDialog() OVERRIDE;
  virtual void CloseAppModalDialog() OVERRIDE;
  virtual void AcceptAppModalDialog() OVERRIDE;
  virtual void CancelAppModalDialog() OVERRIDE;

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
  virtual ~JavascriptAppModalDialogAndroid();

  scoped_ptr<JavaScriptAppModalDialog> dialog_;
  base::android::ScopedJavaGlobalRef<jobject> dialog_jobject_;
  JavaObjectWeakGlobalRef parent_jobject_weak_ref_;

  DISALLOW_COPY_AND_ASSIGN(JavascriptAppModalDialogAndroid);
};


#endif  // CHROME_BROWSER_UI_ANDROID_JAVASCRIPT_APP_MODAL_DIALOG_ANDROID_H_
