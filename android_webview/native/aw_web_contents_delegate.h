// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NATIVE_AW_WEB_CONTENTS_DELEGATE_H_
#define ANDROID_WEBVIEW_NATIVE_AW_WEB_CONTENTS_DELEGATE_H_

#include <jni.h>

#include "chrome/browser/component/web_contents_delegate_android/web_contents_delegate_android.h"

namespace android_webview {

// WebView specific WebContentsDelegate.
// Should contain WebContentsDelegate code required by WebView that should not
// be part of the Chromium Android port.
class AwWebContentsDelegate
    : public web_contents_delegate_android::WebContentsDelegateAndroid {
 public:
  AwWebContentsDelegate(JNIEnv* env, jobject obj);
  virtual ~AwWebContentsDelegate();
  virtual content::JavaScriptDialogCreator* GetJavaScriptDialogCreator()
      OVERRIDE;
  virtual void FindReply(content::WebContents* web_contents,
                         int request_id,
                         int number_of_matches,
                         const gfx::Rect& selection_rect,
                         int active_match_ordinal,
                         bool final_update) OVERRIDE;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_NATIVE_AW_WEB_CONTENTS_DELEGATE_H_
