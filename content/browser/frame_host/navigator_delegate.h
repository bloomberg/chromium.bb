// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_NAVIGATOR_DELEGATE_H_
#define CONTENT_BROWSER_FRAME_HOST_NAVIGATOR_DELEGATE_H_

#include "content/public/browser/invalidate_type.h"

namespace content {

class RenderFrameHost;

// A delegate API used by Navigator to notify its embedder of navigation
// related events.
class NavigatorDelegate {
 public:
  // The RenderFrameHost started a provisional load for the frame
  // represented by |render_frame_host|.
  virtual void DidStartProvisionalLoad(
      RenderFrameHostImpl* render_frame_host,
      int64 frame_id,
      int64 parent_frame_id,
      bool is_main_frame,
      const GURL& validated_url,
      bool is_error_page,
      bool is_iframe_srcdoc) {}

  // Notification to the Navigator embedder that navigation state has
  // changed. This method corresponds to
  // WebContents::NotifyNavigationStateChanged.
  virtual void NotifyChangedNavigationState(InvalidateTypes changed_flags) {}
};

}  // namspace content

#endif  // CONTENT_BROWSER_FRAME_HOST_NAVIGATOR_DELEGATE_H_
