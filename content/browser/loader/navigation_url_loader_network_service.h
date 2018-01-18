// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_NETWORK_SERVICE_H_
#define CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_NETWORK_SERVICE_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "content/browser/loader/navigation_url_loader.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/ssl_status.h"
#include "services/network/public/interfaces/url_loader.mojom.h"
#include "services/network/public/interfaces/url_loader_factory.mojom.h"

namespace net {
struct RedirectInfo;
}

namespace content {

class NavigationData;
class NavigationPostDataHandler;
class ResourceContext;
class StoragePartition;
class URLLoaderRequestHandler;
struct GlobalRequestID;

// This is an implementation of NavigationURLLoader used when
// --enable-network-service is used.
class CONTENT_EXPORT NavigationURLLoaderNetworkService
    : public NavigationURLLoader {
 public:
  // The caller is responsible for ensuring that |delegate| outlives the loader.
  // Note |initial_handlers| is there for test purposes only.
  NavigationURLLoaderNetworkService(
      ResourceContext* resource_context,
      StoragePartition* storage_partition,
      std::unique_ptr<NavigationRequestInfo> request_info,
      std::unique_ptr<NavigationUIData> navigation_ui_data,
      ServiceWorkerNavigationHandle* service_worker_handle,
      AppCacheNavigationHandle* appcache_handle,
      NavigationURLLoaderDelegate* delegate,
      std::vector<std::unique_ptr<URLLoaderRequestHandler>> initial_handlers);
  ~NavigationURLLoaderNetworkService() override;

  // NavigationURLLoader implementation:
  void FollowRedirect() override;
  void ProceedWithResponse() override;

  void OnReceiveResponse(
      scoped_refptr<network::ResourceResponse> response,
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
      const base::Optional<net::SSLInfo>& maybe_ssl_info,
      std::unique_ptr<NavigationData> navigation_data,
      const GlobalRequestID& global_request_id,
      bool is_download,
      bool is_stream,
      network::mojom::DownloadedTempFilePtr downloaded_file);
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         scoped_refptr<network::ResourceResponse> response);
  void OnComplete(const network::URLLoaderCompletionStatus& status);

 private:
  class URLLoaderRequestController;
  void OnRequestStarted(base::TimeTicks timestamp);

  void BindNonNetworkURLLoaderFactoryRequest(
      const GURL& url,
      network::mojom::URLLoaderFactoryRequest factory);

  NavigationURLLoaderDelegate* delegate_;

  // Lives on the IO thread.
  std::unique_ptr<URLLoaderRequestController> request_controller_;

  bool allow_download_;

  // Factories to handle navigation requests for non-network resources.
  ContentBrowserClient::NonNetworkURLLoaderFactoryMap
      non_network_url_loader_factories_;

  base::WeakPtrFactory<NavigationURLLoaderNetworkService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(NavigationURLLoaderNetworkService);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_NETWORK_SERVICE_H_
