// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/aw_web_contents_delegate.h"

#include "base/lazy_instance.h"
#include "android_webview/native/aw_javascript_dialog_creator.h"

namespace android_webview {

static base::LazyInstance<AwJavaScriptDialogCreator>::Leaky
    g_javascript_dialog_creator = LAZY_INSTANCE_INITIALIZER;

AwWebContentsDelegate::AwWebContentsDelegate(
    JNIEnv* env,
    jobject obj)
    : WebContentsDelegateAndroid(env, obj) {
}

AwWebContentsDelegate::~AwWebContentsDelegate() {
}

content::JavaScriptDialogCreator*
AwWebContentsDelegate::GetJavaScriptDialogCreator() {
  return g_javascript_dialog_creator.Pointer();
}

}  // namespace android_webview
