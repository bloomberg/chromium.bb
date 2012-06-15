// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/url_fetcher.h"

#include "base/bind.h"
#include "content/common/net/url_request_user_data.h"
#include "net/url_request/url_fetcher_factory.h"
#include "net/url_request/url_fetcher_impl.h"

// static
net::URLFetcher* content::URLFetcher::Create(
    const GURL& url,
    net::URLFetcher::RequestType request_type,
    net::URLFetcherDelegate* d) {
  return new net::URLFetcherImpl(url, request_type, d);
}

// static
net::URLFetcher* content::URLFetcher::Create(
    int id,
    const GURL& url,
    net::URLFetcher::RequestType request_type,
    net::URLFetcherDelegate* d) {
  net::URLFetcherFactory* factory = net::URLFetcherImpl::factory();
  return factory ? factory->CreateURLFetcher(id, url, request_type, d) :
                   new net::URLFetcherImpl(url, request_type, d);
}

// static
void content::URLFetcher::CancelAll() {
  net::URLFetcherImpl::CancelAll();
}

// static
void content::URLFetcher::SetEnableInterceptionForTests(bool enabled) {
  net::URLFetcherImpl::SetEnableInterceptionForTests(enabled);
}

namespace {

base::SupportsUserData::Data* CreateURLRequestUserData(
    int render_process_id,
    int render_view_id) {
  return new URLRequestUserData(render_process_id, render_view_id);
}

}  // namespace

namespace content {

void AssociateURLFetcherWithRenderView(net::URLFetcher* url_fetcher,
                                       const GURL& first_party_for_cookies,
                                       int render_process_id,
                                       int render_view_id) {
  url_fetcher->SetFirstPartyForCookies(first_party_for_cookies);
  url_fetcher->SetURLRequestUserData(
      URLRequestUserData::kUserDataKey,
      base::Bind(&CreateURLRequestUserData,
                 render_process_id, render_view_id));
}

}  // namespace content
