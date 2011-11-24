// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/request_extra_data.h"

using WebKit::WebReferrerPolicy;

RequestExtraData::RequestExtraData(WebReferrerPolicy referrer_policy,
                                   bool is_main_frame,
                                   int64 frame_id,
                                   bool parent_is_main_frame,
                                   int64 parent_frame_id,
                                   content::PageTransition transition_type)
    : webkit_glue::WebURLRequestExtraDataImpl(referrer_policy),
      is_main_frame_(is_main_frame),
      frame_id_(frame_id),
      parent_is_main_frame_(parent_is_main_frame),
      parent_frame_id_(parent_frame_id),
      transition_type_(transition_type) {
}

RequestExtraData::~RequestExtraData() {
}
