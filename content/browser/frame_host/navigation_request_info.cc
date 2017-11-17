// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/navigation_request_info.h"
#include "content/common/service_worker/service_worker_types.h"
#include "third_party/WebKit/common/page/page_visibility_state.mojom.h"

namespace content {

NavigationRequestInfo::NavigationRequestInfo(
    const CommonNavigationParams& common_params,
    const BeginNavigationParams& begin_params,
    const GURL& site_for_cookies,
    bool is_main_frame,
    bool parent_is_main_frame,
    bool are_ancestors_secure,
    int frame_tree_node_id,
    bool is_for_guests_only,
    bool report_raw_headers,
    blink::mojom::PageVisibilityState page_visibility_state)
    : common_params(common_params),
      begin_params(begin_params),
      site_for_cookies(site_for_cookies),
      is_main_frame(is_main_frame),
      parent_is_main_frame(parent_is_main_frame),
      are_ancestors_secure(are_ancestors_secure),
      frame_tree_node_id(frame_tree_node_id),
      is_for_guests_only(is_for_guests_only),
      report_raw_headers(report_raw_headers),
      page_visibility_state(page_visibility_state) {}

NavigationRequestInfo::~NavigationRequestInfo() {}

}  // namespace content
