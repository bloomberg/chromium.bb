// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_IMPL_CORE_H_
#define CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_IMPL_CORE_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/loader/navigation_url_loader_impl.h"

namespace base {
class Value;
}

namespace net {
class URLRequestContextGetter;
struct RedirectInfo;
}

namespace storage {
class FileSystemContext;
}

namespace content {

class AppCacheNavigationHandleCore;
class NavigationResourceHandler;
class ResourceContext;
class ServiceWorkerNavigationHandleCore;
class StreamHandle;
struct GlobalRequestID;
struct ResourceResponse;

// The IO-thread counterpart to the NavigationURLLoaderImpl. It lives on the IO
// thread and is owned by the UI-thread NavigationURLLoaderImpl and the
// IO-thread NavigationResourceHandler.
// NavigationURLLoaderImplCore interacts with the ResourceDispatcherHost stack
// and forwards signals back to the loader on the UI thread.
class NavigationURLLoaderImplCore
    : public base::RefCountedThreadSafe<NavigationURLLoaderImplCore> {
 public:
  // Creates a new NavigationURLLoaderImplCore that forwards signals back to
  // |loader| on the UI thread.
  explicit NavigationURLLoaderImplCore(
      const base::WeakPtr<NavigationURLLoaderImpl>& loader);

  // Starts the request.
  void Start(ResourceContext* resource_context,
             net::URLRequestContextGetter* url_request_context_getter,
             storage::FileSystemContext* upload_file_system_context,
             ServiceWorkerNavigationHandleCore* service_worker_handle_core,
             AppCacheNavigationHandleCore* appcache_handle_core,
             std::unique_ptr<NavigationRequestInfo> request_info,
             std::unique_ptr<NavigationUIData> navigation_ui_data);

  // Follows the current pending redirect.
  void FollowRedirect();

  // Proceeds with processing the response.
  void ProceedWithResponse();

  // Cancels the request on the IO thread if this NavigationURLLoaderImplCore is
  // still attached to the NavigationResourceHandler.
  void CancelRequestIfNeeded();

  void set_resource_handler(NavigationResourceHandler* resource_handler) {
    resource_handler_ = resource_handler;
  }

  // Notifies |loader_| on the UI thread that the request was redirected.
  void NotifyRequestRedirected(const net::RedirectInfo& redirect_info,
                               ResourceResponse* response);

  // Notifies |loader_| on the UI thread that the response started.
  void NotifyResponseStarted(ResourceResponse* response,
                             std::unique_ptr<StreamHandle> body,
                             const net::SSLInfo& ssl_info,
                             base::Value navigation_data,
                             const GlobalRequestID& request_id,
                             bool is_download,
                             bool is_stream);

  // Notifies |loader_| on the UI thread that the request failed.
  void NotifyRequestFailed(bool in_cache,
                           int net_error,
                           const base::Optional<net::SSLInfo>& ssl_info);

 private:
  friend class base::RefCountedThreadSafe<NavigationURLLoaderImplCore>;
  virtual ~NavigationURLLoaderImplCore();

  base::WeakPtr<NavigationURLLoaderImpl> loader_;
  NavigationResourceHandler* resource_handler_;

  base::WeakPtrFactory<NavigationURLLoaderImplCore> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(NavigationURLLoaderImplCore);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_IMPL_CORE_H_
