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
#include "content/public/common/resource_load_info.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "third_party/blink/public/platform/web_navigation_body_loader.h"

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
  NavigationBodyLoader(
      const CommonNavigationParams& common_params,
      const CommitNavigationParams& commit_params,
      int request_id,
      const network::ResourceResponseHead& head,
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      int render_frame_id,
      bool is_main_frame);
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

  // blink::WebNavigationBodyLoader
  void SetDefersLoading(bool defers) override;
  void StartLoadingBody(WebNavigationBodyLoader::Client* client,
                        bool use_isolated_code_cache) override;

  // network::mojom::URLLoaderClient
  void OnReceiveResponse(const network::ResourceResponseHead& head) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         const network::ResourceResponseHead& head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback callback) override;
  void OnReceiveCachedMetadata(const std::vector<uint8_t>& data) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle handle) override;
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

  void CodeCacheReceived(const base::Time& response_time,
                         const std::vector<uint8_t>& data);
  void BindURLLoaderAndContinue();
  void OnConnectionClosed();
  void OnReadable(MojoResult unused);
  // This method reads data from the pipe in a cycle and dispatches
  // BodyDataReceived synchronously.
  void ReadFromDataPipe();
  void NotifyCompletionIfAppropriate();

  // Navigation parameters.
  const int render_frame_id_;
  const network::ResourceResponseHead head_;
  network::mojom::URLLoaderClientEndpointsPtr endpoints_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // This struct holds stats to notify browser process.
  mojom::ResourceLoadInfoPtr resource_load_info_;

  // These bindings are live while loading the response.
  network::mojom::URLLoaderPtr url_loader_;
  mojo::Binding<network::mojom::URLLoaderClient> url_loader_client_binding_;
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

  base::WeakPtrFactory<NavigationBodyLoader> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(NavigationBodyLoader);
};

}  // namespace content

#endif  // CONTENT_RENDERER_LOADER_NAVIGATION_BODY_LOADER_H_
