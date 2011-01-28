// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/load_from_memory_cache_details.h"

LoadFromMemoryCacheDetails::LoadFromMemoryCacheDetails(
    const GURL& url,
    const std::string& frame_origin,
    const std::string& main_frame_origin,
    int pid,
    int cert_id,
    int cert_status)
    : url_(url),
      frame_origin_(frame_origin),
      main_frame_origin_(main_frame_origin),
      pid_(pid),
      cert_id_(cert_id),
      cert_status_(cert_status) {
}

LoadFromMemoryCacheDetails::~LoadFromMemoryCacheDetails() {
}
