// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/document_state.h"

#include "content/public/renderer/navigation_state.h"
#include "webkit/forms/password_form.h"
#include "webkit/glue/alt_error_page_resource_fetcher.h"

namespace content {

DocumentState::DocumentState()
    : load_histograms_recorded_(false),
      web_timing_histograms_recorded_(false),
      http_status_code_(0),
      was_fetched_via_spdy_(false),
      was_npn_negotiated_(false),
      was_alternate_protocol_available_(false),
      was_fetched_via_proxy_(false),
      use_error_page_(false),
      is_overriding_user_agent_(false),
      must_reset_scroll_and_scale_state_(false),
      was_prefetcher_(false),
      was_referred_by_prefetcher_(false),
      load_type_(UNDEFINED_LOAD),
      cache_policy_override_set_(false),
      cache_policy_override_(WebKit::WebURLRequest::UseProtocolCachePolicy),
      referrer_policy_set_(false),
      referrer_policy_(WebKit::WebReferrerPolicyDefault) {
}

DocumentState::~DocumentState() {}

void DocumentState::set_password_form_data(
    scoped_ptr<webkit::forms::PasswordForm> data) {
  password_form_data_.reset(data.release());
}

void DocumentState::set_alt_error_page_fetcher(
    webkit_glue::AltErrorPageResourceFetcher* f) {
  alt_error_page_fetcher_.reset(f);
}

void DocumentState::set_navigation_state(NavigationState* navigation_state) {
  navigation_state_.reset(navigation_state);
}

}  // namespace content
