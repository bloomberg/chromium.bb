// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_FRAME_ACCESSIBILITY_H_
#define CONTENT_BROWSER_FRAME_HOST_FRAME_ACCESSIBILITY_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/singleton.h"
#include "content/common/content_export.h"

namespace content {

class RenderFrameHostImpl;

// This singleton class maintains the mappings necessary to link child frames
// and guest frames into a single tree for accessibility. This class is only
// used if accessibility is enabled.
class CONTENT_EXPORT FrameAccessibility {
 public:
  static FrameAccessibility* GetInstance();

  // Add a mapping between an accessibility node of |parent_frame_host|
  // and the child frame with the given frame tree node id, in the same
  // frame tree.
  void AddChildFrame(RenderFrameHostImpl* parent_frame_host,
                     int accessibility_node_id,
                     int64 child_frame_tree_node_id);

  // Add a mapping between an accessibility node of |parent_frame_host|
  // and the main frame of the guest Webcontents with the given
  // browser plugin instance id.
  void AddGuestWebContents(RenderFrameHostImpl* parent_frame_host,
                           int accessibility_node_id,
                           int browser_plugin_instance_id);

  // This is called when a RenderFrameHostImpl is deleted, so invalid
  // mappings can be removed from this data structure.
  void OnRenderFrameHostDestroyed(RenderFrameHostImpl* render_frame_host);

  // Given a parent RenderFrameHostImpl and an accessibility node id, look up
  // a child frame or guest frame that was previously associated with this
  // frame and node id. If a mapping exists, return the resulting frame if
  // it's valid. If it doesn't resolve to a valid RenderFrameHostImpl,
  // returns NULL.
  RenderFrameHostImpl* GetChild(RenderFrameHostImpl* parent_frame_host,
                                int accessibility_node_id);

  // Given a RenderFrameHostImpl, check the mapping table to see if it's
  // the child of a node in some other frame. If so, return true and
  // set the parent frame and accessibility node id in the parent frame,
  // otherwise return false.
  bool GetParent(RenderFrameHostImpl* child_frame_host,
                 RenderFrameHostImpl** out_parent_frame_host,
                 int* out_accessibility_node_id);

 private:
  FrameAccessibility();
  virtual ~FrameAccessibility();

  struct ChildFrameMapping {
    ChildFrameMapping();

    RenderFrameHostImpl* parent_frame_host;
    int accessibility_node_id;
    int64 child_frame_tree_node_id;
    int browser_plugin_instance_id;
  };

  std::vector<ChildFrameMapping> mappings_;

  friend struct DefaultSingletonTraits<FrameAccessibility>;
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_FRAME_ACCESSIBILITY_H_
