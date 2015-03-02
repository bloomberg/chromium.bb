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
                         bool allow_download,
                         base::TimeTicks ui_timestamp,
                         FrameMsg_UILoadMetricsReportType::Value report_type,
                         const GURL& base_url_for_data_url,
                         const GURL& history_url_for_data_url);
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

  // Timestamp of the user input event that triggered this navigation. Empty if
  // the navigation was not triggered by clicking on a link or by receiving an
  // intent on Android.
  base::TimeTicks ui_timestamp;

  // The report type to be used when recording the metric using |ui_timestamp|.
  FrameMsg_UILoadMetricsReportType::Value report_type;

  // Base URL for use in Blink's SubstituteData.
  // Is only used with data: URLs.
  GURL base_url_for_data_url;

  // History URL for use in Blink's SubstituteData.
  // Is only used with data: URLs.
  GURL history_url_for_data_url;
};

// PlzNavigate: parameters needed to start a navigation on the IO thread.
struct CONTENT_EXPORT BeginNavigationParams {
  // TODO(clamy): See if it is possible to reuse this in
  // ResourceMsg_Request_Params.
  BeginNavigationParams();
  BeginNavigationParams(std::string method,
                        std::string headers,
                        int load_flags,
                        bool has_user_gesture);

  // The request method: GET, POST, etc.
  std::string method;

  // Additional HTTP request headers.
  std::string headers;

  // net::URLRequest load flags (net::LOAD_NORMAL) by default).
  int load_flags;

  // True if the request was user initiated.
  bool has_user_gesture;
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
