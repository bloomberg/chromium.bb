// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_NET_HELPERS_H_
#define ANDROID_WEBVIEW_BROWSER_NET_HELPERS_H_

#include <memory>

namespace android_webview {

class AwContentsIoThreadClient;

// Gets the net-layer load_flags which reflect |client|'s cache mode.
int GetCacheModeForClient(AwContentsIoThreadClient* client);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_NET_HELPERS_H_
