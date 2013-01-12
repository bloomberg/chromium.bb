// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NATIVE_AW_WEB_CONTENTS_DELEGATE_H_
#define ANDROID_WEBVIEW_NATIVE_AW_WEB_CONTENTS_DELEGATE_H_

#include <jni.h>

#include "components/web_contents_delegate_android/web_contents_delegate_android.h"

namespace android_webview {

// WebView specific WebContentsDelegate.
// Should contain WebContentsDelegate code required by WebView that should not
// be part of the Chromium Android port.
class AwWebContentsDelegate
    : public components::WebContentsDelegateAndroid {
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
  virtual bool CanDownload(content::RenderViewHost* source,
                           int request_id,
                           const std::string& request_method) OVERRIDE;
  virtual void OnStartDownload(content::WebContents* source,
                               content::DownloadItem* download) OVERRIDE;
  virtual void AddNewContents(content::WebContents* source,
                              content::WebContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture,
                              bool* was_blocked) OVERRIDE;
  virtual void CloseContents(content::WebContents* source) OVERRIDE;
  virtual void ActivateContents(content::WebContents* contents) OVERRIDE;
};

bool RegisterAwWebContentsDelegate(JNIEnv* env);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_NATIVE_AW_WEB_CONTENTS_DELEGATE_H_
