// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_NAVIGATOR_DELEGATE_H_
#define CONTENT_BROWSER_FRAME_HOST_NAVIGATOR_DELEGATE_H_

namespace content {

class RenderFrameHost;

// A delegate API used by Navigator to notify its embedder of navigation
// related events.
class NavigatorDelegate {
  // TODO(nasko): This class will be used to dispatch notifications to
  // WebContentsImpl, such as DidStartProvisionalLoad and
  // NotifyNavigationStateChanged. Longer term, most of the
  // NavigationControllerDelegate methods will likely move here.
};

}  // namspace content

#endif  // CONTENT_BROWSER_FRAME_HOST_NAVIGATOR_DELEGATE_H_
