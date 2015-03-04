// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_navigation_url_loader.h"

#include "content/browser/loader/navigation_url_loader_delegate.h"
#include "content/public/browser/stream_handle.h"
#include "content/public/common/resource_response.h"
#include "net/url_request/redirect_info.h"

namespace content {

TestNavigationURLLoader::TestNavigationURLLoader(
    scoped_ptr<NavigationRequestInfo> request_info,
    NavigationURLLoaderDelegate* delegate)
    : request_info_(request_info.Pass()),
      delegate_(delegate),
      redirect_count_(0) {
}

void TestNavigationURLLoader::FollowRedirect() {
  redirect_count_++;
}

void TestNavigationURLLoader::SimulateServerRedirect(const GURL& redirect_url) {
  net::RedirectInfo redirect_info;
  redirect_info.status_code = 302;
  redirect_info.new_method = "GET";
  redirect_info.new_url = redirect_url;
  redirect_info.new_first_party_for_cookies = redirect_url;
  scoped_refptr<ResourceResponse> response(new ResourceResponse);
  CallOnRequestRedirected(redirect_info, response);
}

void TestNavigationURLLoader::CallOnRequestRedirected(
    const net::RedirectInfo& redirect_info,
    const scoped_refptr<ResourceResponse>& response) {
  delegate_->OnRequestRedirected(redirect_info, response);
}

void TestNavigationURLLoader::CallOnResponseStarted(
    const scoped_refptr<ResourceResponse>& response,
    scoped_ptr<StreamHandle> body) {
  delegate_->OnResponseStarted(response, body.Pass());
}

TestNavigationURLLoader::~TestNavigationURLLoader() {}

}  // namespace content
