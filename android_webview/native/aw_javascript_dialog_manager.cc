// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/aw_javascript_dialog_manager.h"

#include "android_webview/native/aw_contents.h"
#include "android_webview/native/js_result_handler.h"
#include "base/android/jni_android.h"
#include "base/android/jni_helper.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/logging.h"
#include "base/string16.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/web_contents.h"

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

namespace android_webview {

AwJavaScriptDialogManager::AwJavaScriptDialogManager() {}

AwJavaScriptDialogManager::~AwJavaScriptDialogManager() {}

void AwJavaScriptDialogManager::RunJavaScriptDialog(
    content::WebContents* web_contents,
    const GURL& origin_url,
    const std::string& accept_lang,
    content::JavaScriptMessageType message_type,
    const string16& message_text,
    const string16& default_prompt_text,
    const DialogClosedCallback& callback,
    bool* did_suppress_message) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> js_result = createJsResultHandler(
       env,
       &callback);
  AwContents* contents = AwContents::FromWebContents(web_contents);
  contents->RunJavaScriptDialog(message_type, origin_url, message_text,
                                default_prompt_text, js_result);
}

void AwJavaScriptDialogManager::RunBeforeUnloadDialog(
    content::WebContents* web_contents,
    const string16& message_text,
    bool is_reload,
    const DialogClosedCallback& callback) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> js_result = createJsResultHandler(
       env,
       &callback);
  AwContents* contents = AwContents::FromWebContents(web_contents);
  contents->RunBeforeUnloadDialog(web_contents->GetURL(), message_text,
                                  js_result);
}

void AwJavaScriptDialogManager::ResetJavaScriptState(
    content::WebContents* web_contents) {
}

}  // namespace android_webview
