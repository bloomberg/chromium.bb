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
  return web_state_.get();
}
const GURL& FakeNavigationContext::GetUrl() const {
  return url_;
}

ui::PageTransition FakeNavigationContext::GetPageTransition() const {
  return page_transition_;
}

bool FakeNavigationContext::IsSameDocument() const {
  return same_document_;
}

bool FakeNavigationContext::IsPost() const {
  return is_post_;
}

NSError* FakeNavigationContext::GetError() const {
  return error_;
}

net::HttpResponseHeaders* FakeNavigationContext::GetResponseHeaders() const {
  return response_headers_.get();
}

bool FakeNavigationContext::IsRendererInitiated() const {
  return renderer_initiated_;
}

void FakeNavigationContext::SetWebState(std::unique_ptr<WebState> web_state) {
  web_state_ = std::move(web_state);
}

void FakeNavigationContext::SetUrl(const GURL& url) {
  url_ = url;
}

void FakeNavigationContext::SetPageTransition(ui::PageTransition transition) {
  page_transition_ = transition;
}

void FakeNavigationContext::SetIsSameDocument(bool same_document) {
  same_document_ = same_document;
}

void FakeNavigationContext::SetIsPost(bool is_post) {
  is_post_ = is_post;
}

void FakeNavigationContext::SetError(NSError* error) {
  error_ = error;
}

void FakeNavigationContext::SetResponseHeaders(
    const scoped_refptr<net::HttpResponseHeaders>& response_headers) {
  response_headers_ = response_headers;
}

void FakeNavigationContext::SetIsRendererInitiated(bool renderer_initiated) {
  renderer_initiated_ = renderer_initiated;
}

}  // namespace web
