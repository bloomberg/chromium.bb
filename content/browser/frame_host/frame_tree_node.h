// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_FRAME_TREE_NODE_H_
#define CONTENT_BROWSER_FRAME_HOST_FRAME_TREE_NODE_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/frame_host/render_frame_host_manager.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

namespace content {

class Navigator;
class RenderFrameHostImpl;

// When a page contains iframes, its renderer process maintains a tree structure
// of those frames. We are mirroring this tree in the browser process. This
// class represents a node in this tree and is a wrapper for all objects that
// are frame-specific (as opposed to page-specific).
class CONTENT_EXPORT FrameTreeNode {
 public:
  static const int64 kInvalidFrameId;

  FrameTreeNode(Navigator* navigator,
                RenderFrameHostDelegate* render_frame_delegate,
                RenderViewHostDelegate* render_view_delegate,
                RenderWidgetHostDelegate* render_widget_delegate,
                RenderFrameHostManager::Delegate* manager_delegate,
                int64 frame_id,
                const std::string& name);

  ~FrameTreeNode();

  void AddChild(scoped_ptr<FrameTreeNode> child);
  void RemoveChild(FrameTreeNode* child);

  // TODO(nasko): This method should be removed once RenderFrameHosts are
  // created by RenderFrameHostManager.
  void set_render_frame_host(
      RenderFrameHostImpl* render_frame_host,
      bool owns_render_frame_host) {
    render_frame_host_ = render_frame_host;
    owns_render_frame_host_ = owns_render_frame_host;
  }

  // Transitional API allowing the RenderFrameHost of a FrameTreeNode
  // representing the main frame to be provided by someone else. After
  // this is called, the FrameTreeNode no longer owns its RenderFrameHost.
  //
  // This should only be used for the main frame (aka root) in a frame tree.
  //
  // TODO(ajwong): Remove this method once the main frame RenderFrameHostImpl is
  // no longer owned by the RenderViewHostImpl.
  void ResetForMainFrame(RenderFrameHostImpl* new_render_frame_host);

  Navigator* navigator() {
    return navigator_.get();
  }

  RenderFrameHostManager* render_manager() {
    return &render_manager_;
  }

  int64 frame_tree_node_id() const {
    return frame_tree_node_id_;
  }

  // DO NOT USE.  Only used by FrameTree until we replace renderer-specific
  // frame IDs with RenderFrameHost routing IDs.
  void set_frame_id(int64 frame_id) {
    DCHECK_EQ(frame_id_, kInvalidFrameId);
    frame_id_ = frame_id;
  }
  int64 frame_id() const {
    return frame_id_;
  }

  const std::string& frame_name() const {
    return frame_name_;
  }

  size_t child_count() const {
    return children_.size();
  }

  FrameTreeNode* child_at(size_t index) const {
    return children_[index];
  }

  const GURL& current_url() const {
    return current_url_;
  }

  void set_current_url(const GURL& url) {
    current_url_ = url;
  }

  RenderFrameHostImpl* render_frame_host() const {
    return render_frame_host_;
  }

 private:
  // The next available browser-global FrameTreeNode ID.
  static int64 next_frame_tree_node_id_;

  // The Navigator object responsible for managing navigations at this node
  // of the frame tree.
  scoped_refptr<Navigator> navigator_;

  // Manages creation and swapping of RenderViewHosts for this frame.  This must
  // be declared before |children_| so that it gets deleted after them.  That's
  // currently necessary so that RenderFrameHostImpl's destructor can call
  // GetProcess.
  // TODO(creis): This will eliminate the need for |render_frame_host_| below.
  RenderFrameHostManager render_manager_;

  // A browser-global identifier for the frame in the page, which stays stable
  // even if the frame does a cross-process navigation.
  const int64 frame_tree_node_id_;

  // The renderer-specific identifier for the frame in the page.
  // TODO(creis): Remove this in favor of the RenderFrameHost's routing ID once
  // we create FrameTreeNodes for all frames (even without a flag), since this
  // value can change after cross-process navigations.
  int64 frame_id_;

  // The assigned name of the frame. This name can be empty, unlike the unique
  // name generated internally in the DOM tree.
  std::string frame_name_;

  // The immediate children of this specific frame.
  ScopedVector<FrameTreeNode> children_;

  // When ResetForMainFrame() is called, this is set to false and the
  // |render_frame_host_| below is not deleted on destruction.
  //
  // For the mainframe, the FrameTree does not own the |render_frame_host_|.
  // This is a transitional wart because RenderFrameHostManager does not yet
  // have the bookkeeping logic to handle creating a pending RenderFrameHost
  // along with a pending RenderViewHost. Thus, for the main frame, the
  // RenderViewHost currently retains ownership and the FrameTreeNode should
  // not delete it on destruction.
  bool owns_render_frame_host_;

  // The active RenderFrameHost for this frame. The FrameTreeNode does not
  // always own this pointer.  See comments above |owns_render_frame_host_|.
  // TODO(ajwong): Replace with RenderFrameHostManager.
  RenderFrameHostImpl* render_frame_host_;

  // Track the current frame's last committed URL, so we can estimate the
  // process impact of out-of-process iframes.
  // TODO(creis): Remove this when we can store subframe URLs in the
  // NavigationController.
  GURL current_url_;

  DISALLOW_COPY_AND_ASSIGN(FrameTreeNode);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_FRAME_TREE_NODE_H_
