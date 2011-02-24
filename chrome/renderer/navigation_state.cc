// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/navigation_state.h"

#include "chrome/renderer/user_script_idle_scheduler.h"
#include "webkit/glue/alt_error_page_resource_fetcher.h"
#include "webkit/glue/password_form.h"

NavigationState::~NavigationState() {}

void NavigationState::set_user_script_idle_scheduler(
    UserScriptIdleScheduler* scheduler) {
  user_script_idle_scheduler_.reset(scheduler);
}

void NavigationState::swap_user_script_idle_scheduler(
    NavigationState* state) {
  user_script_idle_scheduler_.swap(state->user_script_idle_scheduler_);
}

const base::Time& NavigationState::prerendered_page_display_time() const {
  return prerendered_page_display_time_;
}

void NavigationState::set_prerendered_page_display_time(
    const base::Time& value) {
  DCHECK(prerendered_page_display_time_.is_null());
  prerendered_page_display_time_ = value;
}

void NavigationState::set_password_form_data(webkit_glue::PasswordForm* data) {
  password_form_data_.reset(data);
}

void NavigationState::set_alt_error_page_fetcher(
    webkit_glue::AltErrorPageResourceFetcher* f) {
  alt_error_page_fetcher_.reset(f);
}

bool NavigationState::is_prerendering() const {
  return is_prerendering_;
}

void NavigationState::set_is_prerendering(bool is_prerendering) {
  is_prerendering_ = is_prerendering;
}

bool NavigationState::was_started_as_prerender() const {
  return was_started_as_prerender_;
}

void NavigationState::set_was_started_as_prerender(
    bool was_started_as_prerender) {
  DCHECK(!was_started_as_prerender_);
  was_started_as_prerender_ = was_started_as_prerender;
}

NavigationState::NavigationState(PageTransition::Type transition_type,
                                 const base::Time& request_time,
                                 bool is_content_initiated,
                                 int32 pending_page_id,
                                 int pending_history_list_offset)
    : transition_type_(transition_type),
      load_type_(UNDEFINED_LOAD),
      request_time_(request_time),
      load_histograms_recorded_(false),
      web_timing_histograms_recorded_(false),
      request_committed_(false),
      is_content_initiated_(is_content_initiated),
      pending_page_id_(pending_page_id),
      pending_history_list_offset_(pending_history_list_offset),
      use_error_page_(false),
      is_prerendering_(false),
      was_started_as_prerender_(false),
      cache_policy_override_set_(false),
      cache_policy_override_(WebKit::WebURLRequest::UseProtocolCachePolicy),
      user_script_idle_scheduler_(NULL),
      http_status_code_(0),
      was_fetched_via_spdy_(false),
      was_npn_negotiated_(false),
      was_alternate_protocol_available_(false),
      was_fetched_via_proxy_(false),
      was_translated_(false),
      was_within_same_page_(false),
      was_prefetcher_(false),
      was_referred_by_prefetcher_(false) {
}
