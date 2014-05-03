// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GLOBAL_TILE_MANAGER_CLIENT_H_
#define ANDROID_WEBVIEW_BROWSER_GLOBAL_TILE_MANAGER_CLIENT_H_

namespace android_webview {
// GlobalTileManagerClient requests tile resources from GlobalTileManager.
class GlobalTileManagerClient {
 public:
  // Get the number of tiles allocated to the client.
  virtual size_t GetNumTiles() const = 0;

  // Set the number of tiles allocated to the client. When
  // |effective_immediately| is true, the client will enforce its tile policy
  // immediately.
  virtual void SetNumTiles(size_t num_tiles, bool effective_immediately) = 0;

 protected:
  virtual ~GlobalTileManagerClient() {}
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GLOBAL_TILE_MANAGER_CLIENT_H_
