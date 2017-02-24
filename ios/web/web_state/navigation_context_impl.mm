// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/web_state/navigation_context_impl.h"

#include "base/memory/ptr_util.h"

namespace web {

// static
std::unique_ptr<NavigationContext>
NavigationContextImpl::CreateNavigationContext(WebState* web_state,
                                               const GURL& url) {
  std::unique_ptr<NavigationContext> resut(new NavigationContextImpl(
      web_state, url, false /* is_same_page */, false /* is_error_page */));
  return resut;
}

// static
std::unique_ptr<NavigationContext>
NavigationContextImpl::CreateSamePageNavigationContext(WebState* web_state,
                                                       const GURL& url) {
  std::unique_ptr<NavigationContext> result(new NavigationContextImpl(
      web_state, url, true /* is_same_page */, false /* is_error_page */));
  return result;
}

// static
std::unique_ptr<NavigationContext>
NavigationContextImpl::CreateErrorPageNavigationContext(WebState* web_state,
                                                        const GURL& url) {
  std::unique_ptr<NavigationContext> result(new NavigationContextImpl(
      web_state, url, false /* is_same_page */, true /* is_error_page */));
  return result;
}

WebState* NavigationContextImpl::GetWebState() {
  return web_state_;
}

const GURL& NavigationContextImpl::GetUrl() const {
  return url_;
}

bool NavigationContextImpl::IsSamePage() const {
  return is_same_page_;
}

bool NavigationContextImpl::IsErrorPage() const {
  return is_error_page_;
}

NavigationContextImpl::NavigationContextImpl(WebState* web_state,
                                             const GURL& url,
                                             bool is_same_page,
                                             bool is_error_page)
    : web_state_(web_state),
      url_(url),
      is_same_page_(is_same_page),
      is_error_page_(is_error_page) {}

NavigationContextImpl::~NavigationContextImpl() = default;

}  // namespace web
