// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/request_extra_data.h"

using blink::WebString;

namespace content {

RequestExtraData::RequestExtraData(
    blink::WebPageVisibilityState visibility_state,
    const WebString& custom_user_agent,
    bool was_after_preconnect_request,
    int render_frame_id,
    bool is_main_frame,
    int64 frame_id,
    const GURL& frame_origin,
    bool parent_is_main_frame,
    int64 parent_frame_id,
    bool allow_download,
    PageTransition transition_type,
    bool should_replace_current_entry,
    int transferred_request_child_id,
    int transferred_request_request_id)
    : webkit_glue::WebURLRequestExtraDataImpl(custom_user_agent,
                                              was_after_preconnect_request),
      visibility_state_(visibility_state),
      render_frame_id_(render_frame_id),
      is_main_frame_(is_main_frame),
      frame_id_(frame_id),
      frame_origin_(frame_origin),
      parent_is_main_frame_(parent_is_main_frame),
      parent_frame_id_(parent_frame_id),
      allow_download_(allow_download),
      transition_type_(transition_type),
      should_replace_current_entry_(should_replace_current_entry),
      transferred_request_child_id_(transferred_request_child_id),
      transferred_request_request_id_(transferred_request_request_id) {
}

RequestExtraData::~RequestExtraData() {
}

}  // namespace content
