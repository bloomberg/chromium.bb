// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/network/url_request_context_owner.h"

#include "components/prefs/pref_service.h"
#include "net/url_request/url_request_context.h"

namespace content {

URLRequestContextOwner::URLRequestContextOwner() = default;

URLRequestContextOwner::~URLRequestContextOwner() = default;

URLRequestContextOwner::URLRequestContextOwner(URLRequestContextOwner&& other)
    : pref_service(std::move(other.pref_service)),
      url_request_context(std::move(other.url_request_context)) {}

URLRequestContextOwner& URLRequestContextOwner::operator=(
    URLRequestContextOwner&& other) {
  pref_service = std::move(other.pref_service);
  url_request_context = std::move(other.url_request_context);
  return *this;
}

}  // namespace content
