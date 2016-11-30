// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_navigation_url_loader.h"

#include <utility>

#include "content/browser/loader/navigation_url_loader_delegate.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/navigation_data.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/stream_handle.h"
#include "content/public/common/resource_response.h"
#include "net/url_request/redirect_info.h"

namespace content {

TestNavigationURLLoader::TestNavigationURLLoader(
    std::unique_ptr<NavigationRequestInfo> request_info,
    NavigationURLLoaderDelegate* delegate)
    : request_info_(std::move(request_info)),
      delegate_(delegate),
      redirect_count_(0),
      response_proceeded_(false) {}

void TestNavigationURLLoader::FollowRedirect() {
  redirect_count_++;
}

void TestNavigationURLLoader::ProceedWithResponse() {
  response_proceeded_ = true;
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

void TestNavigationURLLoader::SimulateError(int error_code) {
  delegate_->OnRequestFailed(false, error_code);
}

void TestNavigationURLLoader::CallOnRequestRedirected(
    const net::RedirectInfo& redirect_info,
    const scoped_refptr<ResourceResponse>& response) {
  delegate_->OnRequestRedirected(redirect_info, response);
}

void TestNavigationURLLoader::CallOnResponseStarted(
    const scoped_refptr<ResourceResponse>& response,
    std::unique_ptr<StreamHandle> body,
    std::unique_ptr<NavigationData> navigation_data) {
  delegate_->OnResponseStarted(response, std::move(body), SSLStatus(),
                               std::move(navigation_data), GlobalRequestID(),
                               false, false);
}

TestNavigationURLLoader::~TestNavigationURLLoader() {}

}  // namespace content
