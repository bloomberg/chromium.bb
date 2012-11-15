// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/navigation_controller.h"

#include "base/memory/ref_counted_memory.h"

namespace content {

NavigationController::LoadURLParams::LoadURLParams(const GURL& url)
    : url(url),
      load_type(LOAD_TYPE_DEFAULT),
      transition_type(PAGE_TRANSITION_LINK),
      is_renderer_initiated(false),
      override_user_agent(UA_OVERRIDE_INHERIT),
      browser_initiated_post_data(NULL),
      can_load_local_resources(false),
      is_cross_site_redirect(false) {
}

NavigationController::LoadURLParams::~LoadURLParams() {
}

NavigationController::LoadURLParams::LoadURLParams(
    const NavigationController::LoadURLParams& other)
    : url(other.url),
      load_type(other.load_type),
      transition_type(other.transition_type),
      referrer(other.referrer),
      extra_headers(other.extra_headers),
      is_renderer_initiated(other.is_renderer_initiated),
      override_user_agent(other.override_user_agent),
      transferred_global_request_id(other.transferred_global_request_id),
      base_url_for_data_url(other.base_url_for_data_url),
      virtual_url_for_data_url(other.virtual_url_for_data_url),
      browser_initiated_post_data(other.browser_initiated_post_data),
      is_cross_site_redirect(false) {
}

NavigationController::LoadURLParams&
NavigationController::LoadURLParams::operator=(
    const NavigationController::LoadURLParams& other) {
  url = other.url;
  load_type = other.load_type;
  transition_type = other.transition_type;
  referrer = other.referrer;
  extra_headers = other.extra_headers;
  is_renderer_initiated = other.is_renderer_initiated;
  override_user_agent = other.override_user_agent;
  transferred_global_request_id = other.transferred_global_request_id;
  base_url_for_data_url = other.base_url_for_data_url;
  virtual_url_for_data_url = other.virtual_url_for_data_url;
  browser_initiated_post_data = other.browser_initiated_post_data;
  is_cross_site_redirect = other.is_cross_site_redirect;

  return *this;
}

}  // namespace content
