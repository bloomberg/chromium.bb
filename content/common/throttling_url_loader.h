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
#include "content/public/common/url_loader.mojom.h"
#include "content/public/common/url_loader_factory.mojom.h"
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
// defer, resume or cancel the URL loading. If the Mojo connection fails during
// the request it is canceled with net::ERR_FAILED.
class CONTENT_EXPORT ThrottlingURLLoader : public mojom::URLLoaderClient {
 public:
  // |factory| and |client| must stay alive during the lifetime of the returned
  // object. Please note that the request may not start immediately since it
  // could be deferred by throttles.
  static std::unique_ptr<ThrottlingURLLoader> CreateLoaderAndStart(
      mojom::URLLoaderFactory* factory,
      std::vector<std::unique_ptr<URLLoaderThrottle>> throttles,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const ResourceRequest& url_request,
      mojom::URLLoaderClient* client,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
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
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner =
          base::ThreadTaskRunnerHandle::Get());

  ~ThrottlingURLLoader() override;

  void FollowRedirect();
  void SetPriority(net::RequestPriority priority, int32_t intra_priority_value);

  // Disconnects the client connection and releases the URLLoader.
  void DisconnectClient();

  // Sets the forwarding client to receive all subsequent notifications.
  void set_forwarding_client(mojom::URLLoaderClient* client) {
    forwarding_client_ = client;
  }

 private:
  class ForwardingThrottleDelegate;

  ThrottlingURLLoader(
      std::vector<std::unique_ptr<URLLoaderThrottle>> throttles,
      mojom::URLLoaderClient* client,
      const net::NetworkTrafficAnnotationTag& traffic_annotation);

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

  // Processes the result of a URLLoaderThrottle call, adding the throttle to
  // the blocking set if it deferred and updating |*should_defer| accordingly.
  // Returns |true| if the request should continue to be processed (regardless
  // of whether it's been deferred) or |false| if it's been cancelled.
  bool HandleThrottleResult(URLLoaderThrottle* throttle,
                            bool throttle_deferred,
                            bool* should_defer);

  // Stops a given throttle from deferring the request. If this was not the last
  // deferring throttle, the request remains deferred. Otherwise it resumes
  // progress.
  void StopDeferringForThrottle(URLLoaderThrottle* throttle);

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

  void OnClientConnectionError();

  void CancelWithError(int error_code);
  void Resume();

  void PauseReadingBodyFromNet(URLLoaderThrottle* throttle);
  void ResumeReadingBodyFromNet(URLLoaderThrottle* throttle);

  enum DeferredStage {
    DEFERRED_NONE,
    DEFERRED_START,
    DEFERRED_REDIRECT,
    DEFERRED_RESPONSE
  };
  DeferredStage deferred_stage_ = DEFERRED_NONE;
  bool loader_cancelled_ = false;
  bool is_synchronous_ = false;

  struct ThrottleEntry {
    ThrottleEntry(ThrottlingURLLoader* loader,
                  std::unique_ptr<URLLoaderThrottle> the_throttle);
    ThrottleEntry(ThrottleEntry&& other);
    ~ThrottleEntry();

    ThrottleEntry& operator=(ThrottleEntry&& other);

    std::unique_ptr<ForwardingThrottleDelegate> delegate;
    std::unique_ptr<URLLoaderThrottle> throttle;

   private:
    DISALLOW_COPY_AND_ASSIGN(ThrottleEntry);
  };

  std::vector<ThrottleEntry> throttles_;
  std::set<URLLoaderThrottle*> deferring_throttles_;
  std::set<URLLoaderThrottle*> pausing_reading_body_from_net_throttles_;

  // NOTE: This may point to a native implementation (instead of a Mojo proxy
  // object). And it is possible that the implementation of |forwarding_client_|
  // destroys this object synchronously when this object is calling into it.
  mojom::URLLoaderClient* forwarding_client_;
  mojo::Binding<mojom::URLLoaderClient> client_binding_;

  mojom::URLLoaderPtr url_loader_;

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

  const net::NetworkTrafficAnnotationTag traffic_annotation_;

  uint32_t inside_delegate_calls_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ThrottlingURLLoader);
};

}  // namespace content

#endif  // CONTENT_COMMON_THROTTLING_URL_LOADER_H_
