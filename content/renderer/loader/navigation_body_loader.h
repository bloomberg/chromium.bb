// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_LOADER_NAVIGATION_BODY_LOADER_H_
#define CONTENT_RENDERER_LOADER_NAVIGATION_BODY_LOADER_H_

#include <stddef.h>
#include <stdint.h>

#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "content/common/content_export.h"
#include "content/common/navigation_params.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"
#include "third_party/blink/public/platform/web_navigation_body_loader.h"

namespace blink {
struct WebNavigationParams;
}  // namespace blink

namespace network {
struct URLLoaderCompletionStatus;
}  // namespace network

namespace content {

class CodeCacheLoaderImpl;

// Navigation request is started in the browser process, and all redirects
// and final response are received there. Then we pass URLLoader and
// URLLoaderClient bindings to the renderer process, and create an instance
// of this class. It receives the response body, completion status and cached
// metadata, and dispatches them to Blink. It also ensures that completion
// status comes to Blink after the whole body was read and cached code metadata
// was received.
class CONTENT_EXPORT NavigationBodyLoader
    : public blink::WebNavigationBodyLoader,
      public network::mojom::URLLoaderClient {
 public:
  // This method fills navigation params related to the navigation request,
  // redirects and response, and also creates a body loader if needed.
  static void FillNavigationParamsResponseAndBodyLoader(
      mojom::CommonNavigationParamsPtr common_params,
      mojom::CommitNavigationParamsPtr commit_params,
      int request_id,
      network::mojom::URLResponseHeadPtr response_head,
      mojo::ScopedDataPipeConsumerHandle response_body,
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      int render_frame_id,
      bool is_main_frame,
      blink::WebNavigationParams* navigation_params);
  ~NavigationBodyLoader() override;

 private:
  // The loading flow is outlined below. NavigationBodyLoader can be safely
  // deleted at any moment, and it will record cancelation stats, but will not
  // notify the client.
  //
  // StartLoadingBody
  //   request code cache
  // CodeCacheReceived
  //   notify client about cache
  // BindURLLoaderAndContinue
  // OnStartLoadingResponseBody
  //   start reading from the pipe
  // OnReadable (zero or more times)
  //   notify client about data
  // OnComplete (this might come before the whole body data is read,
  //             for example due to different mojo pipes being used
  //             without a relative order guarantee)
  //   save status for later use
  // OnReadable (zero or more times)
  //   notify client about data
  // NotifyCompletionIfAppropriate
  //   notify client about completion

  // The maximal number of bytes consumed in a task. When there are more bytes
  // in the data pipe, they will be consumed in following tasks. Setting a too
  // small number will generate ton of tasks but setting a too large number will
  // lead to thread janks. Also, some clients cannot handle too large chunks
  // (512k for example).
  static constexpr uint32_t kMaxNumConsumedBytesInTask = 64 * 1024;

  NavigationBodyLoader(
      network::mojom::URLResponseHeadPtr response_head,
      mojo::ScopedDataPipeConsumerHandle response_body,
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      int render_frame_id,
      blink::mojom::ResourceLoadInfoPtr resource_load_info);

  // blink::WebNavigationBodyLoader
  void SetDefersLoading(bool defers) override;
  void StartLoadingBody(WebNavigationBodyLoader::Client* client,
                        bool use_isolated_code_cache) override;

  // network::mojom::URLLoaderClient
  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr response_head) override;
  void OnReceiveRedirect(
      const net::RedirectInfo& redirect_info,
      network::mojom::URLResponseHeadPtr response_head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback callback) override;
  void OnReceiveCachedMetadata(mojo_base::BigBuffer data) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle handle) override;
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

  void CodeCacheReceived(base::Time response_head_response_time,
                         base::Time response_time,
                         mojo_base::BigBuffer data);
  void BindURLLoaderAndContinue();
  void OnConnectionClosed();
  void OnReadable(MojoResult unused);
  // This method reads data from the pipe in a cycle and dispatches
  // BodyDataReceived synchronously.
  void ReadFromDataPipe();
  void NotifyCompletionIfAppropriate();
  void BindURLLoaderAndStartLoadingResponseBodyIfPossible();

  // Navigation parameters.
  const int render_frame_id_;
  network::mojom::URLResponseHeadPtr response_head_;
  mojo::ScopedDataPipeConsumerHandle response_body_;
  network::mojom::URLLoaderClientEndpointsPtr endpoints_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // This struct holds stats to notify browser process.
  blink::mojom::ResourceLoadInfoPtr resource_load_info_;

  // These bindings are live while loading the response.
  mojo::Remote<network::mojom::URLLoader> url_loader_;
  mojo::Receiver<network::mojom::URLLoaderClient> url_loader_client_receiver_{
      this};
  WebNavigationBodyLoader::Client* client_ = nullptr;

  // The handle and watcher are live while loading the body.
  mojo::ScopedDataPipeConsumerHandle handle_;
  mojo::SimpleWatcher handle_watcher_;

  // This loader is live while retrieving the code cache.
  std::unique_ptr<CodeCacheLoaderImpl> code_cache_loader_;

  // The final status received from network or cancelation status if aborted.
  network::URLLoaderCompletionStatus status_;

  // Whether we got the body handle to read data from.
  bool has_received_body_handle_ = false;
  // Whether we got the final status.
  bool has_received_completion_ = false;
  // Whether we got all the body data.
  bool has_seen_end_of_data_ = false;

  // Deferred body loader does not send any notifications to the client
  // and tries not to read from the body pipe.
  bool is_deferred_ = false;

  // This protects against reentrancy into OnReadable,
  // which can happen due to nested message loop triggered
  // from iniside BodyDataReceived client notification.
  bool is_in_on_readable_ = false;

  base::WeakPtrFactory<NavigationBodyLoader> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(NavigationBodyLoader);
};

}  // namespace content

#endif  // CONTENT_RENDERER_LOADER_NAVIGATION_BODY_LOADER_H_
