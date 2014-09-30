// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_NAVIGATION_PARAMS_H_
#define CONTENT_COMMON_NAVIGATION_PARAMS_H_

#include <string>

#include "base/basictypes.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/common/frame_message_enums.h"
#include "content/public/common/page_state.h"
#include "content/public/common/referrer.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace base {
class RefCountedMemory;
}

namespace content {
class NavigationEntry;

// The following structures hold parameters used during a navigation. In
// particular they are used by FrameMsg_Navigate, FrameMsg_CommitNavigation and
// FrameHostMsg_BeginNavigation.
// TODO(clamy): Depending on the avancement of the history refactoring move the
// history parameters from FrameMsg_Navigate into one of the structs.

// Used by all navigation IPCs.
struct CONTENT_EXPORT CommonNavigationParams {
  CommonNavigationParams();
  CommonNavigationParams(const GURL& url,
                         const Referrer& referrer,
                         ui::PageTransition transition,
                         FrameMsg_Navigate_Type::Value navigation_type,
                         bool allow_download);
  ~CommonNavigationParams();

  // The URL to navigate to.
  // PlzNavigate: May be modified when the navigation is ready to commit.
  GURL url;

  // The URL to send in the "Referer" header field. Can be empty if there is
  // no referrer.
  Referrer referrer;

  // The type of transition.
  ui::PageTransition transition;

  // Type of navigation.
  FrameMsg_Navigate_Type::Value navigation_type;

  // Allows the URL to be downloaded (true by default).
  // Avoid downloading when in view-source mode.
  bool allow_download;
};

// Used by FrameMsg_Navigate.
// PlzNavigate: sent to the renderer when requesting a navigation.
struct CONTENT_EXPORT RequestNavigationParams {
  RequestNavigationParams();
  RequestNavigationParams(bool is_post,
                          const std::string& extra_headers,
                          const base::RefCountedMemory* post_data);
  ~RequestNavigationParams();

  // Whether the navigation is a POST request (as opposed to a GET).
  bool is_post;

  // Extra headers (separated by \n) to send during the request.
  std::string extra_headers;

  // If is_post is true, holds the post_data information from browser. Empty
  // otherwise.
  std::vector<unsigned char> browser_initiated_post_data;
};

// Used by FrameMsg_Navigate.
// PlzNavigate: sent to the renderer when the navigation is ready to commit.
struct CONTENT_EXPORT CommitNavigationParams {
  CommitNavigationParams();
  CommitNavigationParams(const PageState& page_state,
                         bool is_overriding_user_agent,
                         base::TimeTicks navigation_start);
  ~CommitNavigationParams();

  // Opaque history state (received by ViewHostMsg_UpdateState).
  PageState page_state;

  // Whether or not the user agent override string should be used.
  bool is_overriding_user_agent;

  // The navigationStart time to expose through the Navigation Timing API to JS.
  base::TimeTicks browser_navigation_start;

  // TODO(clamy): Move the redirect chain here.
};

}  // namespace content

#endif  // CONTENT_COMMON_NAVIGATION_PARAMS_H_
