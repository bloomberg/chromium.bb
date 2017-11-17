// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_NAVIGATION_REQUEST_INFO_H_
#define CONTENT_BROWSER_FRAME_HOST_NAVIGATION_REQUEST_INFO_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/common/navigation_params.h"
#include "content/public/common/referrer.h"
#include "third_party/WebKit/common/page/page_visibility_state.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

// A struct to hold the parameters needed to start a navigation request in
// ResourceDispatcherHost. It is initialized on the UI thread, and then passed
// to the IO thread by a NavigationRequest object.
struct CONTENT_EXPORT NavigationRequestInfo {
  NavigationRequestInfo(
      const CommonNavigationParams& common_params,
      const BeginNavigationParams& begin_params,
      const GURL& site_for_cookies,
      bool is_main_frame,
      bool parent_is_main_frame,
      bool are_ancestors_secure,
      int frame_tree_node_id,
      bool is_for_guests_only,
      bool report_raw_headers,
      blink::mojom::PageVisibilityState page_visibility_state);
  ~NavigationRequestInfo();

  const CommonNavigationParams common_params;
  const BeginNavigationParams begin_params;

  // Usually the URL of the document in the top-level window, which may be
  // checked by the third-party cookie blocking policy.
  const GURL site_for_cookies;

  const bool is_main_frame;
  const bool parent_is_main_frame;

  // Whether all ancestor frames of the frame that is navigating have a secure
  // origin. True for main frames.
  const bool are_ancestors_secure;

  const int frame_tree_node_id;

  const bool is_for_guests_only;

  const bool report_raw_headers;

  blink::mojom::PageVisibilityState page_visibility_state;
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_NAVIGATION_REQUEST_INFO_H_
