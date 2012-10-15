// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/aw_web_contents_delegate.h"

#include "base/lazy_instance.h"
#include "android_webview/browser/find_helper.h"
#include "android_webview/native/aw_contents.h"
#include "android_webview/native/aw_javascript_dialog_creator.h"
#include "content/public/browser/android/download_controller_android.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_request_headers.h"

using content::WebContents;

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

void AwWebContentsDelegate::FindReply(WebContents* web_contents,
                                      int request_id,
                                      int number_of_matches,
                                      const gfx::Rect& selection_rect,
                                      int active_match_ordinal,
                                      bool final_update) {
  AwContents* aw_contents = AwContents::FromWebContents(web_contents);
  if (!aw_contents)
    return;

  aw_contents->GetFindHelper()->HandleFindReply(request_id,
                                                number_of_matches,
                                                active_match_ordinal,
                                                final_update);
}

bool AwWebContentsDelegate::CanDownload(content::RenderViewHost* source,
                                        int request_id,
                                        const std::string& request_method) {
  if (request_method == net::HttpRequestHeaders::kGetMethod) {
    content::DownloadControllerAndroid::Get()->CreateGETDownload(
        source, request_id);
  }
  return false;
}

void AwWebContentsDelegate::OnStartDownload(WebContents* source,
                                            content::DownloadItem* download) {
  NOTREACHED();  // We always return false in CanDownload.
}

}  // namespace android_webview
