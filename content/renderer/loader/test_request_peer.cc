// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/loader/test_request_peer.h"

#include "content/renderer/loader/resource_dispatcher.h"
#include "net/url_request/redirect_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TestRequestPeer::TestRequestPeer(ResourceDispatcher* dispatcher,
                                 Context* context)
    : dispatcher_(dispatcher), context_(context) {}

TestRequestPeer::~TestRequestPeer() = default;

void TestRequestPeer::OnUploadProgress(uint64_t position, uint64_t size) {
  EXPECT_FALSE(context_->complete);
}

bool TestRequestPeer::OnReceivedRedirect(const net::RedirectInfo& redirect_info,
                                         const ResourceResponseInfo& info) {
  EXPECT_FALSE(context_->cancelled);
  EXPECT_FALSE(context_->complete);
  ++context_->seen_redirects;
  if (context_->defer_on_redirect)
    dispatcher_->SetDefersLoading(context_->request_id, true);
  return context_->follow_redirects;
}

void TestRequestPeer::OnReceivedResponse(const ResourceResponseInfo& info) {
  EXPECT_FALSE(context_->cancelled);
  EXPECT_FALSE(context_->received_response);
  EXPECT_FALSE(context_->complete);
  context_->received_response = true;
  if (context_->cancel_on_receive_response) {
    dispatcher_->Cancel(context_->request_id);
    context_->cancelled = true;
  }
}

void TestRequestPeer::OnDownloadedData(int len, int encoded_data_length) {
  EXPECT_TRUE(context_->received_response);
  EXPECT_FALSE(context_->cancelled);
  EXPECT_FALSE(context_->complete);
  context_->total_downloaded_data_length += len;
  context_->total_encoded_data_length += encoded_data_length;
}

void TestRequestPeer::OnReceivedData(std::unique_ptr<ReceivedData> data) {
  if (context_->cancelled)
    return;
  EXPECT_TRUE(context_->received_response);
  EXPECT_FALSE(context_->complete);
  context_->data.append(data->payload(), data->length());

  if (context_->cancel_on_receive_data) {
    dispatcher_->Cancel(context_->request_id);
    context_->cancelled = true;
  }
}

void TestRequestPeer::OnTransferSizeUpdated(int transfer_size_diff) {
  EXPECT_TRUE(context_->received_response);
  EXPECT_FALSE(context_->complete);
  if (context_->cancelled)
    return;
  context_->total_encoded_data_length += transfer_size_diff;
  if (context_->defer_on_transfer_size_updated)
    dispatcher_->SetDefersLoading(context_->request_id, true);
}

void TestRequestPeer::OnReceivedCachedMetadata(const char* data, int len) {
  EXPECT_TRUE(context_->received_response);
  EXPECT_FALSE(context_->complete);
  if (context_->cancelled)
    return;
  context_->cached_metadata = std::vector<char>(data, data + len);
}

void TestRequestPeer::OnCompletedRequest(int error_code,
                                         bool stale_copy_in_cache,
                                         const base::TimeTicks& completion_time,
                                         int64_t total_transfer_size,
                                         int64_t encoded_body_size,
                                         int64_t decoded_body_size) {
  if (context_->cancelled)
    return;
  EXPECT_TRUE(context_->received_response);
  EXPECT_FALSE(context_->complete);
  context_->complete = true;
}

TestRequestPeer::Context::Context() = default;
TestRequestPeer::Context::~Context() = default;

}  // namespace content
