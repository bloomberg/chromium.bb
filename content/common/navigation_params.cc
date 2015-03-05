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

CommitNavigationParams::CommitNavigationParams(bool is_overriding_user_agent,
                                               base::TimeTicks navigation_start)
    : is_overriding_user_agent(is_overriding_user_agent),
      browser_navigation_start(navigation_start) {
}

CommitNavigationParams::~CommitNavigationParams() {}

HistoryNavigationParams::HistoryNavigationParams()
    : page_id(-1),
      pending_history_list_offset(-1),
      current_history_list_offset(-1),
      current_history_list_length(-1),
      should_clear_history_list(false) {
}

HistoryNavigationParams::HistoryNavigationParams(
    const PageState& page_state,
    int32 page_id,
    int pending_history_list_offset,
    int current_history_list_offset,
    int current_history_list_length,
    bool should_clear_history_list)
    : page_state(page_state),
      page_id(page_id),
      pending_history_list_offset(pending_history_list_offset),
      current_history_list_offset(current_history_list_offset),
      current_history_list_length(current_history_list_length),
      should_clear_history_list(should_clear_history_list) {
}

HistoryNavigationParams::~HistoryNavigationParams() {
}

}  // namespace content
