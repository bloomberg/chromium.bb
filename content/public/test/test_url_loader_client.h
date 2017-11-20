// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_URL_LOADER_CLIENT_H_
#define CONTENT_PUBLIC_TEST_TEST_URL_LOADER_CLIENT_H_

#include <stdint.h>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "content/public/common/resource_response.h"
#include "content/public/common/url_loader.mojom.h"
#include "content/public/common/url_loader_factory.mojom.h"
#include "mojo/public/c/system/data_pipe.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/url_loader_completion_status.h"

namespace content {

// TestURLLoaderClient records URLLoaderClient function calls.
//
// Example usage:
//   TestURLLoaderClient client;
//   factory_->CreateLoaderAndStart(..., client.CreateInterfacePtr(), ...);
//   client.RunUntilComplete();
//   EXPECT_EQ(net::OK, client.status().error_code);
//   ...
class TestURLLoaderClient final : public mojom::URLLoaderClient {
 public:
  TestURLLoaderClient();
  ~TestURLLoaderClient() override;

  void OnReceiveResponse(const ResourceResponseHead& response_head,
                         const base::Optional<net::SSLInfo>& ssl_info,
                         mojom::DownloadedTempFilePtr downloaded_file) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         const ResourceResponseHead& response_head) override;
  void OnDataDownloaded(int64_t data_length, int64_t encoded_length) override;
  void OnReceiveCachedMetadata(const std::vector<uint8_t>& data) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override;
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

  bool has_received_response() const { return has_received_response_; }
  bool has_received_redirect() const { return has_received_redirect_; }
  bool has_data_downloaded() const { return has_data_downloaded_; }
  bool has_received_upload_progress() const {
    return has_received_upload_progress_;
  }
  bool has_received_cached_metadata() const {
    return has_received_cached_metadata_;
  }
  bool has_received_completion() const { return has_received_completion_; }
  const ResourceResponseHead& response_head() const { return response_head_; }
  const base::Optional<net::SSLInfo>& ssl_info() const { return ssl_info_; }
  const net::RedirectInfo& redirect_info() const { return redirect_info_; }
  const std::string& cached_metadata() const { return cached_metadata_; }
  mojo::DataPipeConsumerHandle response_body() { return response_body_.get(); }
  mojo::ScopedDataPipeConsumerHandle response_body_release() {
    return std::move(response_body_);
  }
  const network::URLLoaderCompletionStatus& completion_status() const {
    return completion_status_;
  }
  int64_t download_data_length() const { return download_data_length_; }
  int64_t encoded_download_data_length() const {
    return encoded_download_data_length_;
  }
  int64_t body_transfer_size() const { return body_transfer_size_; }
  int64_t current_upload_position() const { return current_upload_position_; }
  int64_t total_upload_size() const { return total_upload_size_; }

  void reset_has_received_upload_progress() {
    has_received_upload_progress_ = false;
  }

  void ClearHasReceivedRedirect();
  // Creates an InterfacePtr, binds it to |*this| and returns it.
  mojom::URLLoaderClientPtr CreateInterfacePtr();

  void Unbind();

  void RunUntilResponseReceived();
  void RunUntilRedirectReceived();
  void RunUntilDataDownloaded();
  void RunUntilCachedMetadataReceived();
  void RunUntilResponseBodyArrived();
  void RunUntilComplete();
  void RunUntilConnectionError();

 private:
  void OnConnectionError();

  mojo::Binding<mojom::URLLoaderClient> binding_;
  ResourceResponseHead response_head_;
  base::Optional<net::SSLInfo> ssl_info_;
  net::RedirectInfo redirect_info_;
  std::string cached_metadata_;
  mojo::ScopedDataPipeConsumerHandle response_body_;
  network::URLLoaderCompletionStatus completion_status_;
  bool has_received_response_ = false;
  bool has_received_redirect_ = false;
  bool has_data_downloaded_ = false;
  bool has_received_upload_progress_ = false;
  bool has_received_cached_metadata_ = false;
  bool has_received_completion_ = false;
  bool has_received_connection_error_ = false;

  base::Closure quit_closure_for_on_receive_response_;
  base::Closure quit_closure_for_on_receive_redirect_;
  base::Closure quit_closure_for_on_data_downloaded_;
  base::Closure quit_closure_for_on_receive_cached_metadata_;
  base::Closure quit_closure_for_on_start_loading_response_body_;
  base::Closure quit_closure_for_on_complete_;
  base::Closure quit_closure_for_on_connection_error_;

  mojom::URLLoaderFactoryPtr url_loader_factory_;
  int64_t download_data_length_ = 0;
  int64_t encoded_download_data_length_ = 0;
  int64_t body_transfer_size_ = 0;
  int64_t current_upload_position_ = 0;
  int64_t total_upload_size_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestURLLoaderClient);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_URL_LOADER_CLIENT_H_
