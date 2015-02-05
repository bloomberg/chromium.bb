// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/navigation_request_info.h"

namespace content {

NavigationRequestInfo::NavigationRequestInfo(
    const CommonNavigationParams& common_params,
    const BeginNavigationParams& begin_params,
    const GURL& first_party_for_cookies,
    bool is_main_frame,
    bool parent_is_main_frame,
    scoped_refptr<ResourceRequestBody> request_body)
    : common_params(common_params),
      begin_params(begin_params),
      first_party_for_cookies(first_party_for_cookies),
      is_main_frame(is_main_frame),
      parent_is_main_frame(parent_is_main_frame),
      request_body(request_body) {
}

NavigationRequestInfo::~NavigationRequestInfo() {}

}  // namespace content
