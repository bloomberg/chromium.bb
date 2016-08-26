// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/test_url_loader_client.h"

#include "base/run_loop.h"

namespace content {

TestURLLoaderClient::TestURLLoaderClient() : binding_(this) {}
TestURLLoaderClient::~TestURLLoaderClient() {}

void TestURLLoaderClient::OnReceiveResponse(
    const ResourceResponseHead& response_head) {
  has_received_response_ = true;
  response_head_ = response_head;
  if (quit_closure_for_on_received_response_)
    quit_closure_for_on_received_response_.Run();
}

void TestURLLoaderClient::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  response_body_ = std::move(body);
  if (quit_closure_for_on_start_loading_response_body_)
    quit_closure_for_on_start_loading_response_body_.Run();
}

void TestURLLoaderClient::OnComplete(
    const ResourceRequestCompletionStatus& status) {
  has_received_completion_ = true;
  completion_status_ = status;
  if (quit_closure_for_on_complete_)
    quit_closure_for_on_complete_.Run();
}

mojom::URLLoaderClientPtr TestURLLoaderClient::CreateInterfacePtrAndBind() {
  return binding_.CreateInterfacePtrAndBind();
}

void TestURLLoaderClient::Unbind() {
  binding_.Unbind();
  response_body_.reset();
}

void TestURLLoaderClient::RunUntilResponseReceived() {
  base::RunLoop run_loop;
  quit_closure_for_on_received_response_ = run_loop.QuitClosure();
  run_loop.Run();
  quit_closure_for_on_received_response_.Reset();
}

void TestURLLoaderClient::RunUntilResponseBodyArrived() {
  if (response_body_.is_valid())
    return;
  base::RunLoop run_loop;
  quit_closure_for_on_start_loading_response_body_ = run_loop.QuitClosure();
  run_loop.Run();
  quit_closure_for_on_start_loading_response_body_.Reset();
}

void TestURLLoaderClient::RunUntilComplete() {
  if (has_received_completion_)
    return;
  base::RunLoop run_loop;
  quit_closure_for_on_complete_ = run_loop.QuitClosure();
  run_loop.Run();
  quit_closure_for_on_complete_.Reset();
}

}  // namespace content
