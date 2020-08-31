// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Brought to you by number 42.

#include "net/cookies/cookie_options.h"

#include "net/cookies/cookie_util.h"

namespace net {

CookieOptions::SameSiteCookieContext
CookieOptions::SameSiteCookieContext::MakeInclusive() {
  return SameSiteCookieContext(ContextType::SAME_SITE_STRICT,
                               ContextType::SAME_SITE_STRICT);
}

CookieOptions::SameSiteCookieContext
CookieOptions::SameSiteCookieContext::MakeInclusiveForSet() {
  return SameSiteCookieContext(ContextType::SAME_SITE_LAX,
                               ContextType::SAME_SITE_LAX);
}

CookieOptions::SameSiteCookieContext::ContextType
CookieOptions::SameSiteCookieContext::GetContextForCookieInclusion() const {
  DCHECK_LE(schemeful_context_, context_);

  if (cookie_util::IsSchemefulSameSiteEnabled())
    return schemeful_context_;

  return context_;
}

bool operator==(const CookieOptions::SameSiteCookieContext& lhs,
                const CookieOptions::SameSiteCookieContext& rhs) {
  return std::tie(lhs.context_, lhs.schemeful_context_) ==
         std::tie(rhs.context_, rhs.schemeful_context_);
}

bool operator!=(const CookieOptions::SameSiteCookieContext& lhs,
                const CookieOptions::SameSiteCookieContext& rhs) {
  return !(lhs == rhs);
}

// Keep default values in sync with content/public/common/cookie_manager.mojom.
CookieOptions::CookieOptions()
    : exclude_httponly_(true),
      same_site_cookie_context_(SameSiteCookieContext(
          SameSiteCookieContext::ContextType::CROSS_SITE)),
      update_access_time_(true),
      return_excluded_cookies_(false) {}

// static
CookieOptions CookieOptions::MakeAllInclusive() {
  CookieOptions options;
  options.set_include_httponly();
  options.set_same_site_cookie_context(SameSiteCookieContext::MakeInclusive());
  options.set_do_not_update_access_time();
  return options;
}

}  // namespace net
