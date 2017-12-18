// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_NETWORK_URL_LOADER_H_
#define CONTENT_NETWORK_URL_LOADER_H_

#include <stdint.h>

#include <memory>

#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/network/upload_progress_tracker.h"
#include "content/public/common/url_loader.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/http/http_raw_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request.h"

namespace net {
class HttpResponseHeaders;
}

namespace network {
class NetToMojoPendingBuffer;
}

namespace content {

class NetworkContext;
struct ResourceResponse;

class CONTENT_EXPORT URLLoader : public mojom::URLLoader,
                                 public net::URLRequest::Delegate {
 public:
  URLLoader(NetworkContext* context,
            mojom::URLLoaderRequest url_loader_request,
            int32_t options,
            const ResourceRequest& request,
            bool report_raw_headers,
            mojom::URLLoaderClientPtr url_loader_client,
            const net::NetworkTrafficAnnotationTag& traffic_annotation,
            uint32_t process_id);
  ~URLLoader() override;

  // Called when the associated NetworkContext is going away.
  void Cleanup();

  // mojom::URLLoader implementation:
  void FollowRedirect() override;
  void ProceedWithResponse() override;
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

  // net::URLRequest::Delegate methods:
  void OnReceivedRedirect(net::URLRequest* url_request,
                          const net::RedirectInfo& redirect_info,
                          bool* defer_redirect) override;
  void OnAuthRequired(net::URLRequest* request,
                      net::AuthChallengeInfo* info) override;
  void OnCertificateRequested(net::URLRequest* request,
                              net::SSLCertRequestInfo* info) override;
  void OnSSLCertificateError(net::URLRequest* request,
                             const net::SSLInfo& info,
                             bool fatal) override;
  void OnResponseStarted(net::URLRequest* url_request, int net_error) override;
  void OnReadCompleted(net::URLRequest* url_request, int bytes_read) override;

  // Returns a WeakPtr so tests can validate that the object was destroyed.
  base::WeakPtr<URLLoader> GetWeakPtrForTests();

 private:
  void ReadMore();
  void DidRead(int num_bytes, bool completed_synchronously);
  void NotifyCompleted(int error_code);
  void OnConnectionError();
  void OnResponseBodyStreamConsumerClosed(MojoResult result);
  void OnResponseBodyStreamReady(MojoResult result);
  void CloseResponseBodyStreamProducer();
  void DeleteIfNeeded();
  void SendResponseToClient();
  void CompletePendingWrite();
  void SetRawResponseHeaders(scoped_refptr<const net::HttpResponseHeaders>);
  void UpdateBodyReadBeforePaused();
  void SendUploadProgress(const net::UploadProgress& progress);
  void OnUploadProgressACK();
  void OnSSLCertificateErrorResponse(const net::SSLInfo& ssl_info,
                                     int net_error);

  NetworkContext* context_;
  int32_t options_;
  ResourceType resource_type_;
  bool is_load_timing_enabled_;
  uint32_t process_id_;
  uint32_t render_frame_id_;
  bool connected_;
  std::unique_ptr<net::URLRequest> url_request_;
  mojo::Binding<mojom::URLLoader> binding_;
  mojom::URLLoaderClientPtr url_loader_client_;
  int64_t total_written_bytes_ = 0;

  mojo::ScopedDataPipeProducerHandle response_body_stream_;
  scoped_refptr<network::NetToMojoPendingBuffer> pending_write_;
  uint32_t pending_write_buffer_size_ = 0;
  uint32_t pending_write_buffer_offset_ = 0;
  mojo::SimpleWatcher writable_handle_watcher_;
  mojo::SimpleWatcher peer_closed_handle_watcher_;

  // Used when deferring sending the data to the client until mime sniffing is
  // finished.
  scoped_refptr<ResourceResponse> response_;
  mojo::ScopedDataPipeConsumerHandle consumer_handle_;

  bool report_raw_headers_;
  net::HttpRawRequestHeaders raw_request_headers_;
  scoped_refptr<const net::HttpResponseHeaders> raw_response_headers_;

  std::unique_ptr<UploadProgressTracker> upload_progress_tracker_;

  bool should_pause_reading_body_ = false;
  // The response body stream is open, but transferring data is paused.
  bool paused_reading_body_ = false;

  // Set to true if the response body may be read from cache.
  bool body_may_be_from_cache_ = false;
  // Whether to update |body_read_before_paused_| after the pending read is
  // completed (or when the response body stream is closed).
  bool update_body_read_before_paused_ = false;
  // The number of bytes obtained by the reads initiated before the last
  // PauseReadingBodyFromNet() call. -1 means the request hasn't been paused.
  // The body may be read from cache or network. So even if this value is not
  // -1, we still need to check whether it is from network before reporting it
  // as BodyReadFromNetBeforePaused.
  int64_t body_read_before_paused_ = -1;

  base::WeakPtrFactory<URLLoader> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(URLLoader);
};

}  // namespace content

#endif  // CONTENT_NETWORK_URL_LOADER_H_
