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
  CommitNavigationParams(bool is_overriding_user_agent,
                         base::TimeTicks navigation_start,
                         const std::vector<GURL>& redirects,
                         bool can_load_local_resources,
                         const std::string& frame_to_navigate,
                         base::Time request_time);
  ~CommitNavigationParams();

  // Whether or not the user agent override string should be used.
  bool is_overriding_user_agent;

  // The navigationStart time to expose through the Navigation Timing API to JS.
  base::TimeTicks browser_navigation_start;

  // Any redirect URLs that occurred before |url|. Useful for cross-process
  // navigations; defaults to empty.
  std::vector<GURL> redirects;

  // Whether or not this url should be allowed to access local file://
  // resources.
  bool can_load_local_resources;

  // If not empty, which frame to navigate.
  std::string frame_to_navigate;

  // The time the request was created. This is used by the old performance
  // infrastructure to set up DocumentState associated with the RenderView.
  // TODO(ppi): make it go away.
  base::Time request_time;
};

// Used by FrameMsg_Navigate.
// PlzNavigate: sent to the renderer when the navigation is ready to commit.
struct CONTENT_EXPORT HistoryNavigationParams {
  HistoryNavigationParams();
  HistoryNavigationParams(const PageState& page_state,
                          int32 page_id,
                          int pending_history_list_offset,
                          int current_history_list_offset,
                          int current_history_list_length,
                          bool should_clear_history_list);
  ~HistoryNavigationParams();

  // Opaque history state (received by ViewHostMsg_UpdateState).
  PageState page_state;

  // The page_id for this navigation, or -1 if it is a new navigation.  Back,
  // Forward, and Reload navigations should have a valid page_id.  If the load
  // succeeds, then this page_id will be reflected in the resultant
  // FrameHostMsg_DidCommitProvisionalLoad message.
  int32 page_id;

  // For history navigations, this is the offset in the history list of the
  // pending load. For non-history navigations, this will be ignored.
  int pending_history_list_offset;

  // Where its current page contents reside in session history and the total
  // size of the session history list.
  int current_history_list_offset;
  int current_history_list_length;

  // Whether session history should be cleared. In that case, the RenderView
  // needs to notify the browser that the clearing was succesful when the
  // navigation commits.
  bool should_clear_history_list;
};

// Parameters needed at the start of a navigation. Used by FrameMsg_Navigate.
// PlzNavigate: these parameters are not used in navigation.
struct CONTENT_EXPORT StartNavigationParams {
  StartNavigationParams();
  StartNavigationParams(
      bool is_post,
      const std::string& extra_headers,
      const std::vector<unsigned char>& browser_initiated_post_data,
      bool should_replace_current_entry,
      int transferred_request_child_id,
      int transferred_request_request_id);
  ~StartNavigationParams();

  // Whether the navigation is a POST request (as opposed to a GET).
  bool is_post;

  // Extra headers (separated by \n) to send during the request.
  std::string extra_headers;

  // If is_post is true, holds the post_data information from browser. Empty
  // otherwise.
  std::vector<unsigned char> browser_initiated_post_data;

  // Informs the RenderView the pending navigation should replace the current
  // history entry when it commits. This is used for cross-process redirects so
  // the transferred navigation can recover the navigation state.
  bool should_replace_current_entry;

  // The following two members identify a previous request that has been
  // created before this navigation is being transferred to a new render view.
  // This serves the purpose of recycling the old request.
  // Unless this refers to a transferred navigation, these values are -1 and -1.
  int transferred_request_child_id;
  int transferred_request_request_id;
};

struct NavigationParams {
  NavigationParams(const CommonNavigationParams& common_params,
                   const StartNavigationParams& start_params,
                   const CommitNavigationParams& commit_params,
                   const HistoryNavigationParams& history_params);
  ~NavigationParams();

  CommonNavigationParams common_params;
  StartNavigationParams start_params;
  CommitNavigationParams commit_params;
  HistoryNavigationParams history_params;
};
}  // namespace content

#endif  // CONTENT_COMMON_NAVIGATION_PARAMS_H_
