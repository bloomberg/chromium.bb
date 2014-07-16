// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_NAVIGATION_REQUEST_INFO_H_
#define CONTENT_BROWSER_FRAME_HOST_NAVIGATION_REQUEST_INFO_H_

#include <string>

#include "base/basictypes.h"
#include "content/common/frame_messages.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/common/referrer.h"
#include "url/gurl.h"

namespace content {

// A struct to hold the parameters needed to start a navigation request in
// ResourceDispatcherHost. It is initialized on the UI thread, and then passed
// to the IO thread by a NavigationRequest object.
struct NavigationRequestInfo {
  NavigationRequestInfo(const FrameHostMsg_BeginNavigation_Params& params);

  const FrameHostMsg_BeginNavigation_Params navigation_params;

  // ---------------------------------------------------------------------------
  // The following parameters should be filled in by RenderFrameHostManager
  // before the navigation request is sent to the ResourceDispatcherHost.

  // Usually the URL of the document in the top-level window, which may be
  // checked by the third-party cookie blocking policy.
  GURL first_party_for_cookies;
  bool is_main_frame;
  bool parent_is_main_frame;

  // True if the frame is visible.
  bool is_showing;
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_NAVIGATION_REQUEST_INFO_H_
