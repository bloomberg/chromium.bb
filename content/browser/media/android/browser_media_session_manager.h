// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_ANDROID_BROWSER_MEDIA_SESSION_MANAGER_H_
#define CONTENT_BROWSER_MEDIA_ANDROID_BROWSER_MEDIA_SESSION_MANAGER_H_

#include "base/macros.h"
#include "content/common/content_export.h"

namespace IPC {
class Message;
}

namespace content {

class RenderFrameHost;
struct MediaMetadata;

class CONTENT_EXPORT BrowserMediaSessionManager {
 public:
  explicit BrowserMediaSessionManager(RenderFrameHost* render_frame_host);

  // Message handlers.
  virtual void OnActivate(int session_id, int request_id);
  virtual void OnDeactivate(int session_id, int request_id);
  virtual void OnSetMetadata(int session_id, const MediaMetadata& metadata);

  int GetRoutingID() const;

  bool Send(IPC::Message* msg);

 private:
  RenderFrameHost* const render_frame_host_;

  DISALLOW_COPY_AND_ASSIGN(BrowserMediaSessionManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_ANDROID_BROWSER_MEDIA_SESSION_MANAGER_H_
