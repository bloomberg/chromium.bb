// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_contents_client_bridge_base.h"

#include "base/memory/ptr_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

using content::BrowserThread;
using content::WebContents;

namespace android_webview {

namespace {

const void* const kAwContentsClientBridgeBase = &kAwContentsClientBridgeBase;

// This class is invented so that the UserData registry that we inject the
// AwContentsClientBridgeBase object does not own and destroy it.
class UserData : public base::SupportsUserData::Data {
 public:
  static AwContentsClientBridgeBase* GetContents(
      content::WebContents* web_contents) {
    if (!web_contents)
      return NULL;
    UserData* data = static_cast<UserData*>(
        web_contents->GetUserData(kAwContentsClientBridgeBase));
    return data ? data->contents_ : NULL;
  }

  explicit UserData(AwContentsClientBridgeBase* ptr) : contents_(ptr) {}
 private:
  AwContentsClientBridgeBase* contents_;

  DISALLOW_COPY_AND_ASSIGN(UserData);
};

}  // namespace

// static
void AwContentsClientBridgeBase::Associate(
    WebContents* web_contents,
    AwContentsClientBridgeBase* handler) {
  web_contents->SetUserData(kAwContentsClientBridgeBase,
                            base::MakeUnique<UserData>(handler));
}

// static
AwContentsClientBridgeBase* AwContentsClientBridgeBase::FromWebContents(
    WebContents* web_contents) {
  return UserData::GetContents(web_contents);
}

// static
AwContentsClientBridgeBase* AwContentsClientBridgeBase::FromWebContentsGetter(
    const content::ResourceRequestInfo::WebContentsGetter&
        web_contents_getter) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  WebContents* web_contents = web_contents_getter.Run();
  return UserData::GetContents(web_contents);
}

// static
AwContentsClientBridgeBase* AwContentsClientBridgeBase::FromID(
    int render_process_id,
    int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromID(render_process_id, render_frame_id);
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);
  return UserData::GetContents(web_contents);
}

AwContentsClientBridgeBase::~AwContentsClientBridgeBase() {
}

}  // namespace android_webview
