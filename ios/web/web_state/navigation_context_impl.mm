// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/navigation_context_impl.h"

#import <Foundation/Foundation.h>

#include "base/memory/ptr_util.h"
#include "net/http/http_response_headers.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

// static
std::unique_ptr<NavigationContextImpl>
NavigationContextImpl::CreateNavigationContext(
    WebState* web_state,
    const GURL& url,
    ui::PageTransition page_transition,
    bool is_renderer_initiated) {
  std::unique_ptr<NavigationContextImpl> result(new NavigationContextImpl(
      web_state, url, page_transition, is_renderer_initiated));
  return result;
}

#ifndef NDEBUG
NSString* NavigationContextImpl::GetDescription() const {
  return [NSString stringWithFormat:
                       @"web::WebState: %ld, url: %s, "
                        "is_same_document: %@, error: %@",
                       reinterpret_cast<long>(web_state_), url_.spec().c_str(),
                       is_same_document_ ? @"true" : @"false", error_];
}
#endif  // NDEBUG

WebState* NavigationContextImpl::GetWebState() {
  return web_state_;
}

const GURL& NavigationContextImpl::GetUrl() const {
  return url_;
}

ui::PageTransition NavigationContextImpl::GetPageTransition() const {
  return page_transition_;
}

bool NavigationContextImpl::IsSameDocument() const {
  return is_same_document_;
}

bool NavigationContextImpl::IsPost() const {
  return is_post_;
}

NSError* NavigationContextImpl::GetError() const {
  return error_;
}

net::HttpResponseHeaders* NavigationContextImpl::GetResponseHeaders() const {
  return response_headers_.get();
}

bool NavigationContextImpl::IsRendererInitiated() const {
  return is_renderer_initiated_;
}

void NavigationContextImpl::SetUrl(const GURL& url) {
  url_ = url;
}

void NavigationContextImpl::SetIsSameDocument(bool is_same_document) {
  is_same_document_ = is_same_document;
}

void NavigationContextImpl::SetIsPost(bool is_post) {
  is_post_ = is_post;
}

void NavigationContextImpl::SetError(NSError* error) {
  error_ = error;
}

void NavigationContextImpl::SetResponseHeaders(
    const scoped_refptr<net::HttpResponseHeaders>& response_headers) {
  response_headers_ = response_headers;
}

void NavigationContextImpl::SetIsRendererInitiated(bool is_renderer_initiated) {
  is_renderer_initiated_ = is_renderer_initiated;
}

int NavigationContextImpl::GetNavigationItemUniqueID() const {
  return navigation_item_unique_id_;
}

void NavigationContextImpl::SetNavigationItemUniqueID(int unique_id) {
  navigation_item_unique_id_ = unique_id;
}

void NavigationContextImpl::SetWKNavigationType(
    WKNavigationType wk_navigation_type) {
  wk_navigation_type_ = wk_navigation_type;
}

WKNavigationType NavigationContextImpl::GetWKNavigationType() const {
  return wk_navigation_type_;
}

NavigationContextImpl::NavigationContextImpl(WebState* web_state,
                                             const GURL& url,
                                             ui::PageTransition page_transition,
                                             bool is_renderer_initiated)
    : web_state_(web_state),
      url_(url),
      page_transition_(page_transition),
      is_same_document_(false),
      error_(nil),
      response_headers_(nullptr),
      is_renderer_initiated_(is_renderer_initiated) {}

NavigationContextImpl::~NavigationContextImpl() = default;

}  // namespace web
