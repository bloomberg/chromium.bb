// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/javascript_app_modal_dialog_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/utf_string_conversions.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/app_modal_dialogs/app_modal_dialog_queue.h"
#include "chrome/browser/ui/app_modal_dialogs/javascript_app_modal_dialog.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/javascript_message_type.h"
#include "jni/JavascriptAppModalDialog_jni.h"
#include "ui/gfx/android/window_android.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF16ToJavaString;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using content::BrowserThread;

// static
NativeAppModalDialog* NativeAppModalDialog::CreateNativeJavaScriptPrompt(
    JavaScriptAppModalDialog* dialog,
    gfx::NativeWindow parent_window) {
  return new JavascriptAppModalDialogAndroid(AttachCurrentThread(),
      dialog, parent_window);
}

JavascriptAppModalDialogAndroid::JavascriptAppModalDialogAndroid(
    JNIEnv* env,
    JavaScriptAppModalDialog* dialog,
    gfx::NativeWindow parent)
    : dialog_(dialog),
      parent_jobject_weak_ref_(env, parent->GetJavaObject().obj()) {
}

int JavascriptAppModalDialogAndroid::GetAppModalDialogButtons() const {
  NOTIMPLEMENTED();
  return 0;
}

void JavascriptAppModalDialogAndroid::ShowAppModalDialog() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  JNIEnv* env = AttachCurrentThread();
  // Keep a strong ref to the parent window while we make the call to java to
  // display the dialog.
  ScopedJavaLocalRef<jobject> parent_jobj = parent_jobject_weak_ref_.get(env);
  if (parent_jobj.is_null()) {
    CancelAppModalDialog();
    return;
  }

  ScopedJavaLocalRef<jobject> dialog_object;
  ScopedJavaLocalRef<jstring> title =
      ConvertUTF16ToJavaString(env, dialog_->title());
  ScopedJavaLocalRef<jstring> message =
      ConvertUTF16ToJavaString(env, dialog_->message_text());

  switch (dialog_->javascript_message_type()) {
    case content::JAVASCRIPT_MESSAGE_TYPE_ALERT: {
      dialog_object = Java_JavascriptAppModalDialog_createAlertDialog(env,
          title.obj(), message.obj(),
          dialog_->display_suppress_checkbox());
      break;
    }
    case content::JAVASCRIPT_MESSAGE_TYPE_CONFIRM: {
      if (dialog_->is_before_unload_dialog()) {
        dialog_object = Java_JavascriptAppModalDialog_createBeforeUnloadDialog(
            env, title.obj(), message.obj(), dialog_->is_reload(),
            dialog_->display_suppress_checkbox());
      } else {
        dialog_object = Java_JavascriptAppModalDialog_createConfirmDialog(env,
            title.obj(), message.obj(),
            dialog_->display_suppress_checkbox());
      }
      break;
    }
    case content::JAVASCRIPT_MESSAGE_TYPE_PROMPT: {
      ScopedJavaLocalRef<jstring> default_prompt_text =
          ConvertUTF16ToJavaString(env, dialog_->default_prompt_text());
      dialog_object = Java_JavascriptAppModalDialog_createPromptDialog(env,
          title.obj(), message.obj(),
          dialog_->display_suppress_checkbox(), default_prompt_text.obj());
      break;
    }
    default:
      NOTREACHED();
  }

  // Keep a ref to the java side object until we get a confirm or cancel.
  dialog_jobject_.Reset(dialog_object);

  Java_JavascriptAppModalDialog_showJavascriptAppModalDialog(env,
      dialog_object.obj(), parent_jobj.obj(),
      reinterpret_cast<jint>(this));
}

void JavascriptAppModalDialogAndroid::ActivateAppModalDialog() {
  ShowAppModalDialog();
}

void JavascriptAppModalDialogAndroid::CloseAppModalDialog() {
  CancelAppModalDialog();
}

void JavascriptAppModalDialogAndroid::AcceptAppModalDialog() {
  string16 prompt_text;
  dialog_->OnAccept(prompt_text, false);
  delete this;
}

void JavascriptAppModalDialogAndroid::DidAcceptAppModalDialog(
    JNIEnv* env, jobject, jstring prompt, bool should_suppress_js_dialogs) {
  string16 prompt_text = base::android::ConvertJavaStringToUTF16(env, prompt);
  dialog_->OnAccept(prompt_text, should_suppress_js_dialogs);
  delete this;
}

void JavascriptAppModalDialogAndroid::CancelAppModalDialog() {
  dialog_->OnCancel(false);
  delete this;
}

void JavascriptAppModalDialogAndroid::DidCancelAppModalDialog(
    JNIEnv* env, jobject, bool should_suppress_js_dialogs) {
  dialog_->OnCancel(should_suppress_js_dialogs);
  delete this;
}

const ScopedJavaGlobalRef<jobject>&
    JavascriptAppModalDialogAndroid::GetDialogObject() const {
  return dialog_jobject_;
}

// static
jobject GetCurrentModalDialog(JNIEnv* env, jclass clazz) {
  AppModalDialog* dialog = AppModalDialogQueue::GetInstance()->active_dialog();
  if (!dialog || !dialog->native_dialog())
    return NULL;

  JavascriptAppModalDialogAndroid* js_dialog =
      static_cast<JavascriptAppModalDialogAndroid*>(dialog->native_dialog());
  return js_dialog->GetDialogObject().obj();
}

// static
bool JavascriptAppModalDialogAndroid::RegisterJavascriptAppModalDialog(
    JNIEnv* env) {
  return RegisterNativesImpl(env);
}

JavascriptAppModalDialogAndroid::~JavascriptAppModalDialogAndroid() {
  JNIEnv* env = AttachCurrentThread();
  // In case the dialog is still displaying, tell it to close itself.
  // This can happen if you trigger a dialog but close the Tab before it's
  // shown, and then accept the dialog.
  if (!dialog_jobject_.is_null())
    Java_JavascriptAppModalDialog_dismiss(env, dialog_jobject_.obj());
}
