// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MIME_SNIFFING_URL_LOADER_H_
#define CONTENT_COMMON_MIME_SNIFFING_URL_LOADER_H_

#include <tuple>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece.h"
#include "content/common/content_export.h"
#include "content/common/possibly_associated_interface_ptr.h"
#include "content/public/common/url_loader_throttle.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace content {

class MimeSniffingThrottle;

// Reads the response body and determines its mime type. This url loader buffers
// the response body until the mime type is decided. MimeSniffingURLLoader
// is expected to be created just after receiving OnReceiveResponse(), so this
// handles only OnStartLoadingResponseBody() and OnComplete() as a
// network::mojom::URLLoaderClient.
//
// This loader has five states:
// kWaitForBody: The initial state until the body is received (=
//               OnStartLoadingResponseBody() is called) or the response is
//               finished (= OnComplete() is called). When body is provided, the
//               state is changed to kSniffing. Otherwise the state goes to
//               kCompleted.
// kSniffing: Receives the body from the source loader and estimate the mime
//            type. The received body is kept in this loader until the mime type
//            is decided. When the mime type is decided or all body has been
//            received, this loader will dispatch queued messages like
//            OnStartLoadingResponseBody() to the destination
//            loader client, and then the state is changed to kSending.
// kSending: Receives the body and sends it to the destination loader client.
//           The state changes to kCompleted after all data is sent.
// kCompleted: All data has been sent to the destination loader.
// kAborted: Unexpected behavior happens. Watchers, pipes and the binding from
//           the source loader to |this| are stopped. All incoming messages from
//           the destination (through network::mojom::URLLoader) are ignored in
//           this state.
class CONTENT_EXPORT MimeSniffingURLLoader
    : public network::mojom::URLLoaderClient,
      public network::mojom::URLLoader {
 public:
  ~MimeSniffingURLLoader() override;

  // Start waiting for the body.
  void Start(
      network::mojom::URLLoaderPtr source_url_loader,
      network::mojom::URLLoaderClientRequest source_url_loader_client_request);

  // network::mojom::URLLoaderPtr controls the lifetime of the loader.
  static std::tuple<network::mojom::URLLoaderPtr,
                    network::mojom::URLLoaderClientRequest,
                    MimeSniffingURLLoader*>
  CreateLoader(base::WeakPtr<MimeSniffingThrottle> throttle,
               const GURL& response_url,
               const network::ResourceResponseHead& response_head);

 private:
  MimeSniffingURLLoader(
      base::WeakPtr<MimeSniffingThrottle> throttle,
      const GURL& response_url,
      const network::ResourceResponseHead& response_head,
      network::mojom::URLLoaderClientPtr destination_url_loader_client);

  // network::mojom::URLLoaderClient implementation (called from the source of
  // the response):
  void OnReceiveResponse(
      const network::ResourceResponseHead& response_head) override;
  void OnReceiveRedirect(
      const net::RedirectInfo& redirect_info,
      const network::ResourceResponseHead& response_head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override;
  void OnReceiveCachedMetadata(const std::vector<uint8_t>& data) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override;
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

  // network::mojom::URLLoader implementation (called from the destination of
  // the response):
  void FollowRedirect(
      const base::Optional<std::vector<std::string>>&
          to_be_removed_request_headers,
      const base::Optional<net::HttpRequestHeaders>& modified_request_headers,
      const base::Optional<GURL>& new_url) override;
  void ProceedWithResponse() override;
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

  void OnBodyReadable(MojoResult);
  void OnBodyWritable(MojoResult);
  void CompleteSniffing();
  void CompleteSending();
  void SendReceivedBodyToClient();
  void ForwardBodyToClient();

  void Abort();

  static const char kDefaultMimeType[];

  base::WeakPtr<MimeSniffingThrottle> throttle_;

  mojo::Binding<network::mojom::URLLoaderClient> source_url_client_binding_;
  network::mojom::URLLoaderPtr source_url_loader_;
  network::mojom::URLLoaderClientPtr destination_url_loader_client_;

  GURL response_url_;

  // Capture the response head to defer to send it to the destination until the
  // mime type is decided.
  network::ResourceResponseHead response_head_;

  enum class State { kWaitForBody, kSniffing, kSending, kCompleted, kAborted };
  State state_ = State::kWaitForBody;

  // Set if OnComplete() is called during sniffing.
  base::Optional<network::URLLoaderCompletionStatus> complete_status_;

  std::vector<char> buffered_body_;
  size_t bytes_remaining_in_buffer_;

  mojo::ScopedDataPipeConsumerHandle body_consumer_handle_;
  mojo::ScopedDataPipeProducerHandle body_producer_handle_;
  mojo::SimpleWatcher body_consumer_watcher_;
  mojo::SimpleWatcher body_producer_watcher_;

  DISALLOW_COPY_AND_ASSIGN(MimeSniffingURLLoader);
};

}  // namespace content

#endif  // CONTENT_COMMON_MIME_SNIFFING_URL_LOADER_H_
