// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/frame_navigation_entry.h"

namespace content {

FrameNavigationEntry::FrameNavigationEntry(int64 frame_tree_node_id)
    : frame_tree_node_id_(frame_tree_node_id),
      item_sequence_number_(-1),
      document_sequence_number_(-1) {
}

FrameNavigationEntry::FrameNavigationEntry(int64 frame_tree_node_id,
                                           int64 item_sequence_number,
                                           int64 document_sequence_number,
                                           SiteInstanceImpl* site_instance,
                                           const GURL& url,
                                           const Referrer& referrer)
    : frame_tree_node_id_(frame_tree_node_id),
      item_sequence_number_(item_sequence_number),
      document_sequence_number_(document_sequence_number),
      site_instance_(site_instance),
      url_(url),
      referrer_(referrer) {
}

FrameNavigationEntry::~FrameNavigationEntry() {
}

FrameNavigationEntry* FrameNavigationEntry::Clone() const {
  FrameNavigationEntry* copy = new FrameNavigationEntry(frame_tree_node_id_);
  copy->UpdateEntry(item_sequence_number_, document_sequence_number_,
                    site_instance_.get(), url_, referrer_, page_state_);
  return copy;
}

void FrameNavigationEntry::UpdateEntry(int64 item_sequence_number,
                                       int64 document_sequence_number,
                                       SiteInstanceImpl* site_instance,
                                       const GURL& url,
                                       const Referrer& referrer,
                                       const PageState& page_state) {
  item_sequence_number_ = item_sequence_number;
  document_sequence_number_ = document_sequence_number;
  site_instance_ = site_instance;
  url_ = url;
  referrer_ = referrer;
  page_state_ = page_state;
}

}  // namespace content
