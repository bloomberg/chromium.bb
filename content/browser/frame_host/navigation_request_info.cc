// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/navigation_request_info.h"

#include "content/common/resource_request_body.h"

namespace content {

NavigationRequestInfo::NavigationRequestInfo(
    const FrameHostMsg_BeginNavigation_Params& params)
    : navigation_params(params),
      is_main_frame(true),
      parent_is_main_frame(false) {
}

}  // namespace content
