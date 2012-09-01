// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/lib/aw_content_browser_client.h"

#include "android_webview/browser/renderer_host/aw_resource_dispatcher_host_delegate.h"

namespace android_webview {

AwContentBrowserClient::AwContentBrowserClient()
    : ChromeContentBrowserClient() {
}

AwContentBrowserClient::~AwContentBrowserClient() {
}

void AwContentBrowserClient::ResourceDispatcherHostCreated() {
  ChromeContentBrowserClient::ResourceDispatcherHostCreated();
  AwResourceDispatcherHostDelegate::ResourceDispatcherHostCreated();
}

}  // namespace android_webview
