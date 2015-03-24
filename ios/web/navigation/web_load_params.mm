// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/navigation/web_load_params.h"

namespace web {

WebLoadParams::WebLoadParams(const GURL& url)
    : url(url),
      transition_type(ui::PAGE_TRANSITION_LINK),
      is_renderer_initiated(false),
      cache_mode(net::RequestTracker::CACHE_NORMAL),
      post_data(NULL) {
}

WebLoadParams::~WebLoadParams() {}

WebLoadParams::WebLoadParams(const WebLoadParams& other)
    : url(other.url),
      referrer(other.referrer),
      transition_type(other.transition_type),
      is_renderer_initiated(other.is_renderer_initiated),
      cache_mode(other.cache_mode),
      extra_headers([other.extra_headers copy]),
      post_data([other.post_data copy]) {
}

WebLoadParams& WebLoadParams::operator=(
    const WebLoadParams& other) {
  url = other.url;
  referrer = other.referrer;
  is_renderer_initiated = other.is_renderer_initiated;
  transition_type = other.transition_type;
  cache_mode = other.cache_mode;
  extra_headers.reset([other.extra_headers copy]);
  post_data.reset([other.post_data copy]);

  return *this;
}

}  // namespace web
