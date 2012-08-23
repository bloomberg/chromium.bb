// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_LIB_AW_CONTENT_BROWSER_CLIENT_H_
#define ANDROID_WEBVIEW_LIB_AW_CONTENT_BROWSER_CLIENT_H_

#include "chrome/browser/chrome_content_browser_client.h"

namespace android_webview {

// TODO(boliu): Remove chrome/ dependency and inherit from
// content::ContentBrowserClient directly.
class AwContentBrowserClient : public chrome::ChromeContentBrowserClient {
 public:
  AwContentBrowserClient();
  virtual ~AwContentBrowserClient();

  // Overriden methods from ContentBrowserClient.
  virtual void ResourceDispatcherHostCreated() OVERRIDE;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_LIB_AW_CONTENT_BROWSER_CLIENT_H_
