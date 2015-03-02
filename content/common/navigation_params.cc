// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/navigation_params.h"

#include "base/memory/ref_counted_memory.h"

namespace content {

CommonNavigationParams::CommonNavigationParams()
    : transition(ui::PAGE_TRANSITION_LINK),
      navigation_type(FrameMsg_Navigate_Type::NORMAL),
      allow_download(true),
      report_type(FrameMsg_UILoadMetricsReportType::NO_REPORT) {
}

CommonNavigationParams::CommonNavigationParams(
    const GURL& url,
    const Referrer& referrer,
    ui::PageTransition transition,
    FrameMsg_Navigate_Type::Value navigation_type,
    bool allow_download,
    base::TimeTicks ui_timestamp,
    FrameMsg_UILoadMetricsReportType::Value report_type,
    const GURL& base_url_for_data_url,
    const GURL& history_url_for_data_url)
    : url(url),
      referrer(referrer),
      transition(transition),
      navigation_type(navigation_type),
      allow_download(allow_download),
      ui_timestamp(ui_timestamp),
      report_type(report_type),
      base_url_for_data_url(base_url_for_data_url),
      history_url_for_data_url(history_url_for_data_url) {
}

CommonNavigationParams::~CommonNavigationParams() {
}

BeginNavigationParams::BeginNavigationParams()
    : load_flags(0),
      has_user_gesture(false) {
}

BeginNavigationParams::BeginNavigationParams(std::string method,
                                             std::string headers,
                                             int load_flags,
                                             bool has_user_gesture)
    : method(method),
      headers(headers),
      load_flags(load_flags),
      has_user_gesture(has_user_gesture) {
}

CommitNavigationParams::CommitNavigationParams()
    : is_overriding_user_agent(false) {
}

CommitNavigationParams::CommitNavigationParams(const PageState& page_state,
                                               bool is_overriding_user_agent,
                                               base::TimeTicks navigation_start)
    : page_state(page_state),
      is_overriding_user_agent(is_overriding_user_agent),
      browser_navigation_start(navigation_start) {
}

CommitNavigationParams::~CommitNavigationParams() {}

}  // namespace content
