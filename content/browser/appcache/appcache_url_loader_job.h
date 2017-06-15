// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_URL_LOADER_JOB_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_URL_LOADER_JOB_H_

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "content/browser/appcache/appcache_entry.h"
#include "content/browser/appcache/appcache_job.h"
#include "content/browser/appcache/appcache_response.h"
#include "content/browser/appcache/appcache_storage.h"
#include "content/browser/loader/url_loader_request_handler.h"
#include "content/common/content_export.h"
#include "content/common/resource_request.h"
#include "content/common/url_loader.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/data_pipe.h"

namespace content {

class AppCacheRequest;
class NetToMojoPendingBuffer;

// AppCacheJob wrapper for a mojom::URLLoader implementation which returns
// responses stored in the AppCache.
class CONTENT_EXPORT AppCacheURLLoaderJob : public AppCacheJob,
                                            public AppCacheStorage::Delegate,
                                            public mojom::URLLoader {
 public:
  ~AppCacheURLLoaderJob() override;

  // Sets up the bindings.
  void Start(mojom::URLLoaderRequest request, mojom::URLLoaderClientPtr client);

  // AppCacheJob overrides.
  void Kill() override;
  bool IsStarted() const override;
  void DeliverAppCachedResponse(const GURL& manifest_url,
                                int64_t cache_id,
                                const AppCacheEntry& entry,
                                bool is_fallback) override;
  void DeliverNetworkResponse() override;
  void DeliverErrorResponse() override;
  const GURL& GetURL() const override;
  AppCacheURLLoaderJob* AsURLLoaderJob() override;

  // mojom::URLLoader implementation:
  void FollowRedirect() override;
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override;

  void set_loader_callback(LoaderCallback callback) {
    loader_callback_ = std::move(callback);
  }

 protected:
  // AppCacheJob::Create() creates this instance.
  friend class AppCacheJob;

  AppCacheURLLoaderJob(const ResourceRequest& request,
                       AppCacheStorage* storage);

  // AppCacheStorage::Delegate methods
  void OnResponseInfoLoaded(AppCacheResponseInfo* response_info,
                            int64_t response_id) override;

  // AppCacheResponseReader completion callback
  void OnReadComplete(int result);

  void OnConnectionError();

  // Helper to send the AppCacheResponseInfo to the URLLoaderClient.
  void SendResponseInfo();

  // Helper function to read the data from the AppCache.
  void ReadMore();

  // Callback invoked when the data pipe can be written to.
  void OnResponseBodyStreamReady(MojoResult result);

  // Notifies the client about request completion.
  void NotifyCompleted(int error_code);

  // The current request.
  ResourceRequest request_;

  base::WeakPtr<AppCacheStorage> storage_;

  // Time when the request started.
  base::TimeTicks start_time_tick_;

  // The AppCache manifest URL.
  GURL manifest_url_;

  // The AppCache id.
  int64_t cache_id_;

  AppCacheEntry entry_;

  // Set to true if we are loading fallback content.
  bool is_fallback_;

  // The data pipe used to transfer AppCache data to the client.
  mojo::DataPipe data_pipe_;

  // Binds the URLLoaderClient with us.
  mojo::Binding<mojom::URLLoader> binding_;

  // The URLLoaderClient pointer. We call this interface with notifications
  // about the URL load
  mojom::URLLoaderClientPtr client_info_;

  // mojo data pipe entities.
  mojo::ScopedDataPipeProducerHandle response_body_stream_;

  scoped_refptr<NetToMojoPendingBuffer> pending_write_;

  mojo::SimpleWatcher writable_handle_watcher_;

  // The Callback to be invoked in the network service land to indicate if
  // the request can be serviced via the AppCache.
  LoaderCallback loader_callback_;

  DISALLOW_COPY_AND_ASSIGN(AppCacheURLLoaderJob);
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_URL_LOADER_JOB_H_
