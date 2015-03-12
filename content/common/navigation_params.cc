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
    : is_overriding_user_agent(false),
      browser_navigation_start(base::TimeTicks::Now()),
      can_load_local_resources(false),
      request_time(base::Time::Now()) {
}

CommitNavigationParams::CommitNavigationParams(
    bool is_overriding_user_agent,
    base::TimeTicks navigation_start,
    const std::vector<GURL>& redirects,
    bool can_load_local_resources,
    const std::string& frame_to_navigate,
    base::Time request_time)
    : is_overriding_user_agent(is_overriding_user_agent),
      browser_navigation_start(navigation_start),
      redirects(redirects),
      can_load_local_resources(can_load_local_resources),
      frame_to_navigate(frame_to_navigate),
      request_time(request_time) {
}

CommitNavigationParams::~CommitNavigationParams() {}

HistoryNavigationParams::HistoryNavigationParams()
    : page_id(-1),
      pending_history_list_offset(-1),
      current_history_list_offset(-1),
      current_history_list_length(0),
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

StartNavigationParams::StartNavigationParams()
    : is_post(false),
      should_replace_current_entry(false),
      transferred_request_child_id(-1),
      transferred_request_request_id(-1) {
}

StartNavigationParams::StartNavigationParams(
    bool is_post,
    const std::string& extra_headers,
    const std::vector<unsigned char>& browser_initiated_post_data,
    bool should_replace_current_entry,
    int transferred_request_child_id,
    int transferred_request_request_id)
    : is_post(is_post),
      extra_headers(extra_headers),
      browser_initiated_post_data(browser_initiated_post_data),
      should_replace_current_entry(should_replace_current_entry),
      transferred_request_child_id(transferred_request_child_id),
      transferred_request_request_id(transferred_request_request_id) {
}

StartNavigationParams::~StartNavigationParams() {
}

NavigationParams::NavigationParams(
    const CommonNavigationParams& common_params,
    const StartNavigationParams& start_params,
    const CommitNavigationParams& commit_params,
    const HistoryNavigationParams& history_params)
    : common_params(common_params),
      start_params(start_params),
      commit_params(commit_params),
      history_params(history_params) {
}

NavigationParams::~NavigationParams() {
}

}  // namespace content
