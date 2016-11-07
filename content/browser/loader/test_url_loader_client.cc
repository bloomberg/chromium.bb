// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/test_url_loader_client.h"

#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace content {

namespace {

class TestURLLoaderFactory final : public mojom::URLLoaderFactory {
 public:
  class Waiter final : public base::RefCounted<Waiter> {
   public:
    Waiter() = default;
    void Signal(mojom::URLLoaderClientAssociatedPtrInfo client_ptr) {
      client_ptr_ = std::move(client_ptr);
      run_loop_.Quit();
    }
    mojom::URLLoaderClientAssociatedPtrInfo Wait() {
      run_loop_.Run();
      return std::move(client_ptr_);
    }

   private:
    friend class base::RefCounted<Waiter>;
    ~Waiter() {}

    base::RunLoop run_loop_;
    mojom::URLLoaderClientAssociatedPtrInfo client_ptr_;

    DISALLOW_COPY_AND_ASSIGN(Waiter);
  };

  explicit TestURLLoaderFactory(scoped_refptr<Waiter> waiter)
      : waiter_(waiter) {}
  ~TestURLLoaderFactory() override {}

  void CreateLoaderAndStart(
      mojom::URLLoaderAssociatedRequest request,
      int32_t routing_id,
      int32_t request_id,
      const ResourceRequest& url_request,
      mojom::URLLoaderClientAssociatedPtrInfo client_ptr_info) override {
    waiter_->Signal(std::move(client_ptr_info));
  }

  void SyncLoad(int32_t routing_id,
                int32_t request_id,
                const ResourceRequest& url_request,
                const SyncLoadCallback& callback) override {
    NOTREACHED();
  }

 private:
  scoped_refptr<Waiter> waiter_;

  DISALLOW_COPY_AND_ASSIGN(TestURLLoaderFactory);
};

void CreateURLLoaderFactory(scoped_refptr<TestURLLoaderFactory::Waiter> waiter,
                            mojom::URLLoaderFactoryRequest request) {
  mojo::MakeStrongBinding(
      base::MakeUnique<TestURLLoaderFactory>(std::move(waiter)),
      std::move(request));
}

}  // namespace

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

mojom::URLLoaderClientAssociatedPtrInfo
TestURLLoaderClient::CreateLocalAssociatedPtrInfo() {
  scoped_refptr<TestURLLoaderFactory::Waiter> waiter(
      new TestURLLoaderFactory::Waiter());
  CreateURLLoaderFactory(waiter, mojo::GetProxy(&url_loader_factory_));
  ResourceRequest request;
  mojom::URLLoaderAssociatedPtr loader_proxy;

  mojom::URLLoaderClientAssociatedPtrInfo info;
  binding_.Bind(&info, url_loader_factory_.associated_group());
  url_loader_factory_->CreateLoaderAndStart(
      mojo::GetProxy(&loader_proxy, url_loader_factory_.associated_group()), 0,
      0, request, std::move(info));

  return waiter->Wait();
}

mojom::URLLoaderClientAssociatedPtrInfo
TestURLLoaderClient::CreateRemoteAssociatedPtrInfo(
    mojo::AssociatedGroup* associated_group) {
  mojom::URLLoaderClientAssociatedPtrInfo client_ptr_info;
  binding_.Bind(&client_ptr_info, associated_group);
  return client_ptr_info;
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
