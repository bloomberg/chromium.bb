// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_safe_browsing_ui_manager.h"

#include "content/public/browser/browser_thread.h"

using content::BrowserThread;
using content::WebContents;

namespace android_webview {

AwSafeBrowsingUIManager::AwSafeBrowsingUIManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

AwSafeBrowsingUIManager::~AwSafeBrowsingUIManager() {}

void AwSafeBrowsingUIManager::DisplayBlockingPage(
    const UnsafeResource& resource) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebContents* web_contents = resource.web_contents_getter.Run();
  // Check the size of the view
  UIManagerClient* client = UIManagerClient::FromWebContents(web_contents);
  if (!client || !client->CanShowInterstitial()) {
    LOG(WARNING) << "The view is not suitable to show the SB interstitial";
    OnBlockingPageDone(std::vector<UnsafeResource>{resource}, false,
                       web_contents, resource.url.GetWithEmptyPath());
    return;
  }
  safe_browsing::BaseUIManager::DisplayBlockingPage(resource);
}

}  // namespace android_webview
