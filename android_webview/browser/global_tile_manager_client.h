// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GLOBAL_TILE_MANAGER_CLIENT_H_
#define ANDROID_WEBVIEW_BROWSER_GLOBAL_TILE_MANAGER_CLIENT_H_

#include "content/public/browser/android/synchronous_compositor.h"

namespace android_webview {
// GlobalTileManagerClient requests tile resources from GlobalTileManager.
class GlobalTileManagerClient {
 public:
  // Get tile memory policy for the client.
  virtual content::SynchronousCompositorMemoryPolicy GetMemoryPolicy()
      const = 0;

  // Set tile memory policy of the client. When |effective_immediately| is
  // true, the client will enforce its tile policy immediately.
  virtual void SetMemoryPolicy(
      content::SynchronousCompositorMemoryPolicy new_policy,
      bool effective_immediately) = 0;

 protected:
  virtual ~GlobalTileManagerClient() {}
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GLOBAL_TILE_MANAGER_CLIENT_H_
