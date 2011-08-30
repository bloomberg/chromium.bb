// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/request_extra_data.h"

RequestExtraData::RequestExtraData(bool is_main_frame,
                                   int64 frame_id,
                                   PageTransition::Type transition_type)
    : is_main_frame_(is_main_frame),
      frame_id_(frame_id),
      transition_type_(transition_type) {
}

RequestExtraData::~RequestExtraData() {
}
