// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/fakes/fake_navigation_context.h"

#include "net/http/http_response_headers.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

FakeNavigationContext::FakeNavigationContext() = default;
FakeNavigationContext::~FakeNavigationContext() = default;

WebState* FakeNavigationContext::GetWebState() {
  return nullptr;
}
const GURL& FakeNavigationContext::GetUrl() const {
  return GURL::EmptyGURL();
}

ui::PageTransition FakeNavigationContext::GetPageTransition() const {
  return ui::PAGE_TRANSITION_LINK;
}

bool FakeNavigationContext::IsSameDocument() const {
  return false;
}

bool FakeNavigationContext::IsPost() const {
  return false;
}

NSError* FakeNavigationContext::GetError() const {
  return nil;
}

net::HttpResponseHeaders* FakeNavigationContext::GetResponseHeaders() const {
  return response_headers_.get();
}

void FakeNavigationContext::SetResponseHeaders(
    const scoped_refptr<net::HttpResponseHeaders>& response_headers) {
  response_headers_ = response_headers;
}

}  // namespace web
