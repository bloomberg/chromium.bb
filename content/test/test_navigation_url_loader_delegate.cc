// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_navigation_url_loader_delegate.h"

#include "base/run_loop.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/navigation_data.h"
#include "content/public/browser/stream_handle.h"
#include "content/public/common/resource_response.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TestNavigationURLLoaderDelegate::TestNavigationURLLoaderDelegate()
    : net_error_(0), on_request_handled_counter_(0) {}

TestNavigationURLLoaderDelegate::~TestNavigationURLLoaderDelegate() {}

void TestNavigationURLLoaderDelegate::WaitForRequestRedirected() {
  request_redirected_.reset(new base::RunLoop);
  request_redirected_->Run();
  request_redirected_.reset();
}

void TestNavigationURLLoaderDelegate::WaitForResponseStarted() {
  response_started_.reset(new base::RunLoop);
  response_started_->Run();
  response_started_.reset();
}

void TestNavigationURLLoaderDelegate::WaitForRequestFailed() {
  request_failed_.reset(new base::RunLoop);
  request_failed_->Run();
  request_failed_.reset();
}

void TestNavigationURLLoaderDelegate::WaitForRequestStarted() {
  request_started_.reset(new base::RunLoop);
  request_started_->Run();
  request_started_.reset();
}

void TestNavigationURLLoaderDelegate::ReleaseBody() {
  body_.reset();
  handle_.reset();
}

void TestNavigationURLLoaderDelegate::OnRequestRedirected(
    const net::RedirectInfo& redirect_info,
    const scoped_refptr<ResourceResponse>& response) {
  redirect_info_ = redirect_info;
  redirect_response_ = response;
  ASSERT_TRUE(request_redirected_);
  request_redirected_->Quit();
}

void TestNavigationURLLoaderDelegate::OnResponseStarted(
    const scoped_refptr<ResourceResponse>& response,
    std::unique_ptr<StreamHandle> body,
    mojo::ScopedDataPipeConsumerHandle consumer_handle,
    const SSLStatus& ssl_status,
    std::unique_ptr<NavigationData> navigation_data,
    const GlobalRequestID& request_id,
    bool is_download,
    bool is_stream,
    mojom::URLLoaderFactoryPtrInfo loader_factory_ptr_info) {
  response_ = response;
  body_ = std::move(body);
  handle_ = std::move(consumer_handle);
  if (response_started_)
    response_started_->Quit();
}

void TestNavigationURLLoaderDelegate::OnRequestFailed(bool in_cache,
                                                      int net_error) {
  net_error_ = net_error;
  if (request_failed_)
    request_failed_->Quit();
}

void TestNavigationURLLoaderDelegate::OnRequestStarted(
    base::TimeTicks timestamp) {
  ASSERT_FALSE(timestamp.is_null());
  ++on_request_handled_counter_;
  if (request_started_)
    request_started_->Quit();
}

}  // namespace content
