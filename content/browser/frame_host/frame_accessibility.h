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

  // Given a parent RenderFrameHostImpl and an accessibility node id, look up
  // all child frames or guest frames that were previously associated with this
  // frame, and populate |child_frame_hosts| with all of them that resolve
  // to a valid RenderFrameHostImpl.
  void GetAllChildFrames(RenderFrameHostImpl* parent_frame_host,
                         std::vector<RenderFrameHostImpl*>* child_frame_hosts);

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

  RenderFrameHostImpl* GetRFHIFromFrameTreeNodeId(
      RenderFrameHostImpl* parent_frame_host,
      int64 child_frame_tree_node_id);

  // This structure stores the parent-child relationship between an
  // accessibility node in a parent frame and its child frame, where the
  // child frame may be an iframe or the main frame of a guest browser
  // plugin. It allows the accessibility code to walk both down and up
  // the "composed" accessibility tree.
  struct ChildFrameMapping {
    ChildFrameMapping();

    // The parent frame host. Required.
    RenderFrameHostImpl* parent_frame_host;

    // The id of the accessibility node that's the host of the child frame,
    // for example the accessibility node for the <iframe> element or the
    // <embed> element.
    int accessibility_node_id;

    // If the child frame is an iframe, this is the iframe's FrameTreeNode id,
    // otherwise 0.
    int64 child_frame_tree_node_id;

    // If the child frame is a browser plugin, this is its instance id,
    // otherwise 0.
    int browser_plugin_instance_id;
  };

  std::vector<ChildFrameMapping> mappings_;

  friend struct DefaultSingletonTraits<FrameAccessibility>;
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_FRAME_ACCESSIBILITY_H_
