// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/frame_navigation_entry.h"

#include <utility>

namespace content {

FrameNavigationEntry::FrameNavigationEntry(int frame_tree_node_id)
    : frame_tree_node_id_(frame_tree_node_id),
      item_sequence_number_(-1),
      document_sequence_number_(-1) {
}

FrameNavigationEntry::FrameNavigationEntry(
    int frame_tree_node_id,
    const std::string& frame_unique_name,
    int64_t item_sequence_number,
    int64_t document_sequence_number,
    scoped_refptr<SiteInstanceImpl> site_instance,
    const GURL& url,
    const Referrer& referrer)
    : frame_tree_node_id_(frame_tree_node_id),
      frame_unique_name_(frame_unique_name),
      item_sequence_number_(item_sequence_number),
      document_sequence_number_(document_sequence_number),
      site_instance_(std::move(site_instance)),
      url_(url),
      referrer_(referrer) {}

FrameNavigationEntry::~FrameNavigationEntry() {
}

FrameNavigationEntry* FrameNavigationEntry::Clone() const {
  FrameNavigationEntry* copy = new FrameNavigationEntry(frame_tree_node_id_);
  copy->UpdateEntry(frame_unique_name_, item_sequence_number_,
                    document_sequence_number_, site_instance_.get(), url_,
                    referrer_, page_state_);
  return copy;
}

void FrameNavigationEntry::UpdateEntry(const std::string& frame_unique_name,
                                       int64_t item_sequence_number,
                                       int64_t document_sequence_number,
                                       SiteInstanceImpl* site_instance,
                                       const GURL& url,
                                       const Referrer& referrer,
                                       const PageState& page_state) {
  frame_unique_name_ = frame_unique_name;
  item_sequence_number_ = item_sequence_number;
  document_sequence_number_ = document_sequence_number;
  site_instance_ = site_instance;
  url_ = url;
  referrer_ = referrer;
  page_state_ = page_state;
}

void FrameNavigationEntry::set_item_sequence_number(
    int64_t item_sequence_number) {
  // TODO(creis): Assert that this does not change after being assigned, once
  // location.replace is classified as NEW_PAGE rather than EXISTING_PAGE.
  // Same for document sequence number.  See https://crbug.com/596707.
  item_sequence_number_ = item_sequence_number;
}

void FrameNavigationEntry::set_document_sequence_number(
    int64_t document_sequence_number) {
  document_sequence_number_ = document_sequence_number;
}

}  // namespace content
