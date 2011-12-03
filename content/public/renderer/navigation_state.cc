// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/navigation_state.h"

namespace content {

NavigationState::NavigationState(content::PageTransition transition_type,
                                 bool is_content_initiated,
                                 int32 pending_page_id,
                                 int pending_history_list_offset)
    : transition_type_(transition_type),
      request_committed_(false),
      is_content_initiated_(is_content_initiated),
      pending_page_id_(pending_page_id),
      pending_history_list_offset_(pending_history_list_offset),
      was_within_same_page_(false),
      transferred_request_child_id_(-1),
      transferred_request_request_id_(-1) {
}

NavigationState::~NavigationState() {}

}  // namespace content
