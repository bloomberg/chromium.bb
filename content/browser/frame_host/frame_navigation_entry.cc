// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/frame_navigation_entry.h"

namespace content {

FrameNavigationEntry::FrameNavigationEntry(int64 frame_tree_node_id)
    : frame_tree_node_id_(frame_tree_node_id) {
}

FrameNavigationEntry::FrameNavigationEntry(int64 frame_tree_node_id,
                                           SiteInstanceImpl* site_instance,
                                           const GURL& url,
                                           const Referrer& referrer)
    : frame_tree_node_id_(frame_tree_node_id),
      site_instance_(site_instance),
      url_(url),
      referrer_(referrer) {
}

FrameNavigationEntry::~FrameNavigationEntry() {
}

FrameNavigationEntry* FrameNavigationEntry::Clone() const {
  FrameNavigationEntry* copy = new FrameNavigationEntry(frame_tree_node_id_);
  copy->UpdateEntry(site_instance_.get(), url_, referrer_);
  return copy;
}

void FrameNavigationEntry::UpdateEntry(SiteInstanceImpl* site_instance,
                                       const GURL& url,
                                       const Referrer& referrer) {
  site_instance_ = site_instance;
  url_ = url;
  referrer_ = referrer;
}

}  // namespace content
