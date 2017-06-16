// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_THROTTLING_URL_LOADER_H_
#define CONTENT_COMMON_THROTTLING_URL_LOADER_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/common/content_export.h"
#include "content/common/possibly_associated_interface_ptr.h"
#include "content/common/url_loader.mojom.h"
#include "content/common/url_loader_factory.mojom.h"
#include "content/public/common/url_loader_throttle.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace content {

namespace mojom {
class URLLoaderFactory;
}

// ThrottlingURLLoader is a wrapper around the mojom::URLLoader[Factory]
// interfaces. It applies a list of URLLoaderThrottle instances which could
// defer, resume or cancel the URL loading.
class CONTENT_EXPORT ThrottlingURLLoader : public mojom::URLLoaderClient,
                                           public URLLoaderThrottle::Delegate {
 public:
  // |factory| and |client| must stay alive during the lifetime of the returned
  // object.
  // Please note that the request may not start immediately since it could be
  // deferred by throttles.
  static std::unique_ptr<ThrottlingURLLoader> CreateLoaderAndStart(
      mojom::URLLoaderFactory* factory,
      std::vector<std::unique_ptr<URLLoaderThrottle>> throttles,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const ResourceRequest& url_request,
      mojom::URLLoaderClient* client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner =
          base::ThreadTaskRunnerHandle::Get());

  using StartLoaderCallback =
      base::OnceCallback<void(mojom::URLLoaderRequest request,
                              mojom::URLLoaderClientPtr client)>;

  // Similar to the method above, but uses a |start_loader_callback| instead of
  // a mojom::URLLoaderFactory to start the loader. The callback must be safe
  // to call during the lifetime of the returned object.
  static std::unique_ptr<ThrottlingURLLoader> CreateLoaderAndStart(
      StartLoaderCallback start_loader_callback,
      std::vector<std::unique_ptr<URLLoaderThrottle>> throttles,
      const ResourceRequest& url_request,
      mojom::URLLoaderClient* client,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner =
          base::ThreadTaskRunnerHandle::Get());

  ~ThrottlingURLLoader() override;

  void FollowRedirect();
  void SetPriority(net::RequestPriority priority, int32_t intra_priority_value);

 private:
  ThrottlingURLLoader(
      std::vector<std::unique_ptr<URLLoaderThrottle>> throttles,
      mojom::URLLoaderClient* client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation);

  // Either of the two sets of arguments below is valid but not both:
  // - |factory|, |routing_id|, |request_id| and |options|;
  // - |start_loader_callback|.
  void Start(mojom::URLLoaderFactory* factory,
             int32_t routing_id,
             int32_t request_id,
             uint32_t options,
             StartLoaderCallback start_loader_callback,
             const ResourceRequest& url_request,
             scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  void StartNow(mojom::URLLoaderFactory* factory,
                int32_t routing_id,
                int32_t request_id,
                uint32_t options,
                StartLoaderCallback start_loader_callback,
                const ResourceRequest& url_request,
                scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // mojom::URLLoaderClient implementation:
  void OnReceiveResponse(const ResourceResponseHead& response_head,
                         const base::Optional<net::SSLInfo>& ssl_info,
                         mojom::DownloadedTempFilePtr downloaded_file) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         const ResourceResponseHead& response_head) override;
  void OnDataDownloaded(int64_t data_len, int64_t encoded_data_len) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override;
  void OnReceiveCachedMetadata(const std::vector<uint8_t>& data) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override;
  void OnComplete(const ResourceRequestCompletionStatus& status) override;

  // URLLoaderThrottle::Delegate:
  void CancelWithError(int error_code) override;
  void Resume() override;

  enum DeferredStage {
    DEFERRED_NONE,
    DEFERRED_START,
    DEFERRED_REDIRECT,
    DEFERRED_RESPONSE
  };
  DeferredStage deferred_stage_ = DEFERRED_NONE;
  bool cancelled_by_throttle_ = false;

  std::unique_ptr<URLLoaderThrottle> throttle_;

  mojom::URLLoaderClient* forwarding_client_;
  mojo::Binding<mojom::URLLoaderClient> client_binding_;

  PossiblyAssociatedInterfacePtr<mojom::URLLoader> url_loader_;

  struct StartInfo {
    StartInfo(mojom::URLLoaderFactory* in_url_loader_factory,
              int32_t in_routing_id,
              int32_t in_request_id,
              uint32_t in_options,
              StartLoaderCallback in_start_loader_callback,
              const ResourceRequest& in_url_request,
              scoped_refptr<base::SingleThreadTaskRunner> in_task_runner);
    ~StartInfo();

    mojom::URLLoaderFactory* url_loader_factory;
    int32_t routing_id;
    int32_t request_id;
    uint32_t options;

    StartLoaderCallback start_loader_callback;

    ResourceRequest url_request;
    // |task_runner_| is used to set up |client_binding_|.
    scoped_refptr<base::SingleThreadTaskRunner> task_runner;
  };
  // Set if start is deferred.
  std::unique_ptr<StartInfo> start_info_;

  struct ResponseInfo {
    ResponseInfo(const ResourceResponseHead& in_response_head,
                 const base::Optional<net::SSLInfo>& in_ssl_info,
                 mojom::DownloadedTempFilePtr in_downloaded_file);
    ~ResponseInfo();

    ResourceResponseHead response_head;
    base::Optional<net::SSLInfo> ssl_info;
    mojom::DownloadedTempFilePtr downloaded_file;
  };
  // Set if response is deferred.
  std::unique_ptr<ResponseInfo> response_info_;

  struct RedirectInfo {
    RedirectInfo(const net::RedirectInfo& in_redirect_info,
                 const ResourceResponseHead& in_response_head);
    ~RedirectInfo();

    net::RedirectInfo redirect_info;
    ResourceResponseHead response_head;
  };
  // Set if redirect is deferred.
  std::unique_ptr<RedirectInfo> redirect_info_;

  struct PriorityInfo {
    PriorityInfo(net::RequestPriority in_priority,
                 int32_t in_intra_priority_value);
    ~PriorityInfo();

    net::RequestPriority priority;
    int32_t intra_priority_value;
  };
  // Set if request is deferred and SetPriority() is called.
  std::unique_ptr<PriorityInfo> priority_info_;

  const net::MutableNetworkTrafficAnnotationTag traffic_annotation_;

  DISALLOW_COPY_AND_ASSIGN(ThrottlingURLLoader);
};

}  // namespace content

#endif  // CONTENT_COMMON_THROTTLING_URL_LOADER_H_
