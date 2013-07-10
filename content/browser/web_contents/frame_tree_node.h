// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_FRAME_TREE_NODE_H_
#define CONTENT_BROWSER_WEB_CONTENTS_FRAME_TREE_NODE_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_vector.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

namespace content {

// Any page that contains iframes has a tree structure of the frames in the
// renderer process. We are mirroring this tree in the browser process. This
// class represents a node in this tree and is a wrapper for all objects that
// are frame-specific (as opposed to page-specific).
class CONTENT_EXPORT FrameTreeNode {
 public:
  FrameTreeNode(int64 frame_id, const std::string& name);
  ~FrameTreeNode();

  // This method takes ownership of the child pointer.
  void AddChild(FrameTreeNode* child);
  void RemoveChild(int64 child_id);

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

 private:
  // The unique identifier for the frame in the page.
  int64 frame_id_;

  // The assigned name of the frame. This name can be empty, unlike the unique
  // name generated internally in the DOM tree.
  std::string frame_name_;

  // The immediate children of this specific frame.
  ScopedVector<FrameTreeNode> children_;

  // Track the current frame's last committed URL, so we can estimate the
  // process impact of out-of-process iframes.
  // TODO(creis): Remove this when we can store subframe URLs in the
  // NavigationController.
  GURL current_url_;

  DISALLOW_COPY_AND_ASSIGN(FrameTreeNode);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_FRAME_TREE_NODE_H_
