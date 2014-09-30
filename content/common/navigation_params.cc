// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/navigation_params.h"

#include "base/memory/ref_counted_memory.h"

namespace content {
CommonNavigationParams::CommonNavigationParams()
    : transition(ui::PAGE_TRANSITION_LINK),
      navigation_type(FrameMsg_Navigate_Type::NORMAL),
      allow_download(true) {
}

CommonNavigationParams::~CommonNavigationParams() {}

CommonNavigationParams::CommonNavigationParams(
    const GURL& url,
    const Referrer& referrer,
    ui::PageTransition transition,
    FrameMsg_Navigate_Type::Value navigation_type,
    bool allow_download)
    : url(url),
      referrer(referrer),
      transition(transition),
      navigation_type(navigation_type),
      allow_download(allow_download) {
}

RequestNavigationParams::RequestNavigationParams() : is_post(false) {}

RequestNavigationParams::RequestNavigationParams(
    bool is_post,
    const std::string& extra_headers,
    const base::RefCountedMemory* post_data)
    : is_post(is_post),
      extra_headers(extra_headers) {
  if (post_data) {
    browser_initiated_post_data.assign(
        post_data->front(), post_data->front() + post_data->size());
  }
}

RequestNavigationParams::~RequestNavigationParams() {}

CommitNavigationParams::CommitNavigationParams()
    : is_overriding_user_agent(false) {
}

CommitNavigationParams::CommitNavigationParams(const PageState& page_state,
                                               bool is_overriding_user_agent,
                                               base::TimeTicks navigation_start)
    : page_state(page_state),
      is_overriding_user_agent(is_overriding_user_agent),
      browser_navigation_start(navigation_start) {
}

CommitNavigationParams::~CommitNavigationParams() {}

}  // namespace content
