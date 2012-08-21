// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/chrome_web_contents_delegate_android.h"

#include "base/android/jni_android.h"
#include "chrome/browser/file_select_helper.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/file_chooser_params.h"

using base::android::AttachCurrentThread;
using base::android::GetClass;
using base::android::ScopedJavaLocalRef;
using content::FileChooserParams;
using content::WebContents;

namespace chrome {
namespace android {

ChromeWebContentsDelegateAndroid::ChromeWebContentsDelegateAndroid(JNIEnv* env,
                                                                   jobject obj)
    : WebContentsDelegateAndroid(env, obj) {
}

ChromeWebContentsDelegateAndroid::~ChromeWebContentsDelegateAndroid() {
}

void ChromeWebContentsDelegateAndroid::RunFileChooser(
    WebContents* web_contents,
    const FileChooserParams& params) {
  FileSelectHelper::RunFileChooser(web_contents, params);
}

}  // namespace android
}  // namespace chrome
