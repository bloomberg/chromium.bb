// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_NET_DISK_CACHE_REMOVER_H_
#define ANDROID_WEBVIEW_BROWSER_NET_DISK_CACHE_REMOVER_H_

namespace content {

class RenderProcessHost;

}  // namespace content

namespace android_webview {

// Clear all http disk cache for this renderer. This method is asynchronous and
// will noop if a previous call has not finished.
void RemoveHttpDiskCache(content::RenderProcessHost* render_process_host);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_NET_DISK_CACHE_REMOVER_H_
