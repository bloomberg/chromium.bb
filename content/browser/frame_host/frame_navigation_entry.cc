// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/frame_navigation_entry.h"

#include <utility>

namespace content {

FrameNavigationEntry::FrameNavigationEntry(int frame_tree_node_id)
    : frame_tree_node_id_(frame_tree_node_id),
      item_sequence_number_(-1),
      document_sequence_number_(-1),
      post_id_(-1) {}

FrameNavigationEntry::FrameNavigationEntry(
    int frame_tree_node_id,
    const std::string& frame_unique_name,
    int64_t item_sequence_number,
    int64_t document_sequence_number,
    scoped_refptr<SiteInstanceImpl> site_instance,
    const GURL& url,
    const Referrer& referrer,
    const std::string& method,
    int64_t post_id)
    : frame_tree_node_id_(frame_tree_node_id),
      frame_unique_name_(frame_unique_name),
      item_sequence_number_(item_sequence_number),
      document_sequence_number_(document_sequence_number),
      site_instance_(std::move(site_instance)),
      url_(url),
      referrer_(referrer),
      method_(method),
      post_id_(post_id) {}

FrameNavigationEntry::~FrameNavigationEntry() {
}

FrameNavigationEntry* FrameNavigationEntry::Clone() const {
  FrameNavigationEntry* copy = new FrameNavigationEntry(frame_tree_node_id_);
  copy->UpdateEntry(frame_unique_name_, item_sequence_number_,
                    document_sequence_number_, site_instance_.get(), url_,
                    referrer_, page_state_, method_, post_id_);
  return copy;
}

void FrameNavigationEntry::UpdateEntry(const std::string& frame_unique_name,
                                       int64_t item_sequence_number,
                                       int64_t document_sequence_number,
                                       SiteInstanceImpl* site_instance,
                                       const GURL& url,
                                       const Referrer& referrer,
                                       const PageState& page_state,
                                       const std::string& method,
                                       int64_t post_id) {
  frame_unique_name_ = frame_unique_name;
  item_sequence_number_ = item_sequence_number;
  document_sequence_number_ = document_sequence_number;
  site_instance_ = site_instance;
  url_ = url;
  referrer_ = referrer;
  page_state_ = page_state;
  method_ = method;
  post_id_ = post_id;
}

void FrameNavigationEntry::set_item_sequence_number(
    int64_t item_sequence_number) {
  // Once assigned, the item sequence number shouldn't change.
  DCHECK(item_sequence_number_ == -1 ||
         item_sequence_number_ == item_sequence_number);
  item_sequence_number_ = item_sequence_number;
}

void FrameNavigationEntry::set_document_sequence_number(
    int64_t document_sequence_number) {
  // Once assigned, the document sequence number shouldn't change.
  DCHECK(document_sequence_number_ == -1 ||
         document_sequence_number_ == document_sequence_number);
  document_sequence_number_ = document_sequence_number;
}

}  // namespace content
