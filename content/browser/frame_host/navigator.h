// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_NAVIGATOR_H_
#define CONTENT_BROWSER_FRAME_HOST_NAVIGATOR_H_

#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"

class GURL;

namespace content {

class NavigationControllerImpl;
class NavigatorDelegate;
class RenderFrameHostImpl;

// Implementations of this interface are responsible for performing navigations
// in a node of the FrameTree. Its lifetime is bound to all FrameTreeNode
// objects that are using it and will be released once all nodes that use it are
// freed. The Navigator is bound to a single frame tree and cannot be used by
// multiple instances of FrameTree.
// TODO(nasko): Move all navigation methods, such as didStartProvisionalLoad
// from WebContentsImpl to this interface.
class CONTENT_EXPORT Navigator : public base::RefCounted<Navigator> {
 public:
  // The RenderFrameHostImpl started a provisional load.
  virtual void DidStartProvisionalLoad(RenderFrameHostImpl* render_frame_host,
                                       int64 frame_id,
                                       int64 parent_frame_id,
                                       bool main_frame,
                                       const GURL& url) {};

 protected:
  friend class base::RefCounted<Navigator>;
  virtual ~Navigator() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_NAVIGATOR_H_
