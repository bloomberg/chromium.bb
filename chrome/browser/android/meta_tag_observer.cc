// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/meta_tag_observer.h"

#include "chrome/common/chrome_constants.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

MetaTagObserver::MetaTagObserver(const std::string& meta_tag)
    : meta_tag_(meta_tag) {
  if (meta_tag_.size() > chrome::kMaxMetaTagAttributeLength) {
    VLOG(1) << "Length of the <meta> name attribute is too large.";
    NOTREACHED();
  }
}

MetaTagObserver::~MetaTagObserver() {
}

void MetaTagObserver::DidFinishLoad(content::RenderFrameHost* render_frame_host,
                                    const GURL& validated_url) {
  if (render_frame_host->GetParent())
    return;
  validated_url_ = validated_url;
  Send(new ChromeViewMsg_RetrieveMetaTagContent(routing_id(),
                                                validated_url,
                                                meta_tag_));
}

bool MetaTagObserver::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MetaTagObserver, message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_DidRetrieveMetaTagContent,
                        OnDidRetrieveMetaTagContent)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void MetaTagObserver::OnDidRetrieveMetaTagContent(
    bool success,
    const std::string& tag_name,
    const std::string& tag_content,
    const GURL& expected_url) {
  if (!success || tag_name != meta_tag_ || validated_url_ != expected_url ||
      tag_content.size() >= chrome::kMaxMetaTagAttributeLength) {
    return;
  }
  HandleMetaTagContent(tag_content, expected_url);
}
