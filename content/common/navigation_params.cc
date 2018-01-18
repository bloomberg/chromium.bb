// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/navigation_params.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/common/appcache_info.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/service_worker_modes.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"
#include "url/url_constants.h"
#include "url/url_util.h"

namespace content {

SourceLocation::SourceLocation() : line_number(0), column_number(0) {}

SourceLocation::SourceLocation(const std::string& url,
                               unsigned int line_number,
                               unsigned int column_number)
    : url(url), line_number(line_number), column_number(column_number) {}

SourceLocation::~SourceLocation() {}

CommonNavigationParams::CommonNavigationParams()
    : transition(ui::PAGE_TRANSITION_LINK),
      navigation_type(FrameMsg_Navigate_Type::DIFFERENT_DOCUMENT),
      allow_download(true),
      should_replace_current_entry(false),
      report_type(FrameMsg_UILoadMetricsReportType::NO_REPORT),
      previews_state(PREVIEWS_UNSPECIFIED),
      navigation_start(base::TimeTicks::Now()),
      method("GET"),
      should_check_main_world_csp(CSPDisposition::CHECK),
      started_from_context_menu(false),
      has_user_gesture(false) {}

CommonNavigationParams::CommonNavigationParams(
    const GURL& url,
    const Referrer& referrer,
    ui::PageTransition transition,
    FrameMsg_Navigate_Type::Value navigation_type,
    bool allow_download,
    bool should_replace_current_entry,
    base::TimeTicks ui_timestamp,
    FrameMsg_UILoadMetricsReportType::Value report_type,
    const GURL& base_url_for_data_url,
    const GURL& history_url_for_data_url,
    PreviewsState previews_state,
    const base::TimeTicks& navigation_start,
    std::string method,
    const scoped_refptr<network::ResourceRequestBody>& post_data,
    base::Optional<SourceLocation> source_location,
    CSPDisposition should_check_main_world_csp,
    bool started_from_context_menu,
    bool has_user_gesture)
    : url(url),
      referrer(referrer),
      transition(transition),
      navigation_type(navigation_type),
      allow_download(allow_download),
      should_replace_current_entry(should_replace_current_entry),
      ui_timestamp(ui_timestamp),
      report_type(report_type),
      base_url_for_data_url(base_url_for_data_url),
      history_url_for_data_url(history_url_for_data_url),
      previews_state(previews_state),
      navigation_start(navigation_start),
      method(method),
      post_data(post_data),
      source_location(source_location),
      should_check_main_world_csp(should_check_main_world_csp),
      started_from_context_menu(started_from_context_menu),
      has_user_gesture(has_user_gesture) {
  // |method != "POST"| should imply absence of |post_data|.
  if (method != "POST" && post_data) {
    NOTREACHED();
    this->post_data = nullptr;
  }
}

CommonNavigationParams::CommonNavigationParams(
    const CommonNavigationParams& other) = default;

CommonNavigationParams::~CommonNavigationParams() {
}

RequestNavigationParams::RequestNavigationParams()
    : is_overriding_user_agent(false),
      can_load_local_resources(false),
      nav_entry_id(0),
      is_history_navigation_in_new_child(false),
      intended_as_new_entry(false),
      pending_history_list_offset(-1),
      current_history_list_offset(-1),
      current_history_list_length(0),
      is_view_source(false),
      should_clear_history_list(false),
      should_create_service_worker(false),
      service_worker_provider_id(kInvalidServiceWorkerProviderId),
      appcache_host_id(kAppCacheNoHostId) {}

RequestNavigationParams::RequestNavigationParams(
    bool is_overriding_user_agent,
    const std::vector<GURL>& redirects,
    const GURL& original_url,
    const std::string& original_method,
    bool can_load_local_resources,
    const PageState& page_state,
    int nav_entry_id,
    bool is_history_navigation_in_new_child,
    std::map<std::string, bool> subframe_unique_names,
    bool intended_as_new_entry,
    int pending_history_list_offset,
    int current_history_list_offset,
    int current_history_list_length,
    bool is_view_source,
    bool should_clear_history_list)
    : is_overriding_user_agent(is_overriding_user_agent),
      redirects(redirects),
      original_url(original_url),
      original_method(original_method),
      can_load_local_resources(can_load_local_resources),
      page_state(page_state),
      nav_entry_id(nav_entry_id),
      is_history_navigation_in_new_child(is_history_navigation_in_new_child),
      subframe_unique_names(subframe_unique_names),
      intended_as_new_entry(intended_as_new_entry),
      pending_history_list_offset(pending_history_list_offset),
      current_history_list_offset(current_history_list_offset),
      current_history_list_length(current_history_list_length),
      is_view_source(is_view_source),
      should_clear_history_list(should_clear_history_list),
      should_create_service_worker(false),
      service_worker_provider_id(kInvalidServiceWorkerProviderId),
      appcache_host_id(kAppCacheNoHostId) {}

RequestNavigationParams::RequestNavigationParams(
    const RequestNavigationParams& other) = default;

RequestNavigationParams::~RequestNavigationParams() {
}

NavigationParams::NavigationParams(
    const CommonNavigationParams& common_params,
    const RequestNavigationParams& request_params)
    : common_params(common_params),
      request_params(request_params) {
}

NavigationParams::~NavigationParams() {
}

}  // namespace content
