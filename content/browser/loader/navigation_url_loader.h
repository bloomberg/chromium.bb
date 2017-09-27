// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_H_
#define CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "content/common/content_export.h"
#include "content/public/common/resource_request_completion_status.h"

namespace content {

class AppCacheNavigationHandle;
class NavigationUIData;
class NavigationURLLoaderDelegate;
class NavigationURLLoaderFactory;
class ResourceContext;
class ServiceWorkerNavigationHandle;
class StoragePartition;
class ThrottlingURLLoader;
struct NavigationRequestInfo;
struct ResourceRequest;

// PlzNavigate: The navigation logic's UI thread entry point into the resource
// loading stack. It exposes an interface to control the request prior to
// receiving the response. If the NavigationURLLoader is destroyed before
// OnResponseStarted is called, the request is aborted.
class CONTENT_EXPORT NavigationURLLoader {
 public:
  // Creates a NavigationURLLoader. The caller is responsible for ensuring that
  // |delegate| outlives the loader. |request_body| must not be accessed on the
  // UI thread after this point.
  //
  // TODO(davidben): When navigation is disentangled from the loader, the
  // request parameters should not come in as a navigation-specific
  // structure. Information like has_user_gesture and
  // should_replace_current_entry shouldn't be needed at this layer.
  static std::unique_ptr<NavigationURLLoader> Create(
      ResourceContext* resource_context,
      StoragePartition* storage_partition,
      std::unique_ptr<NavigationRequestInfo> request_info,
      std::unique_ptr<NavigationUIData> navigation_ui_data,
      ServiceWorkerNavigationHandle* service_worker_handle,
      AppCacheNavigationHandle* appcache_handle,
      NavigationURLLoaderDelegate* delegate);

  // For testing purposes; sets the factory for use in testing.
  static void SetFactoryForTesting(NavigationURLLoaderFactory* factory);

  virtual ~NavigationURLLoader() {}

  // Called in response to OnRequestRedirected to continue processing the
  // request.
  virtual void FollowRedirect() = 0;

  // Called in response to OnResponseStarted to process the response.
  virtual void ProceedWithResponse() = 0;

  // Callback to intercept the response from the URLLoader. Only used when
  // network service is enabled. Args: the initial resource request,
  // the URLLoader for sending the request, optional completion status if
  // it has already been received.
  using NavigationInterceptionCB =
      base::OnceCallback<void(std::unique_ptr<ResourceRequest>,
                              std::unique_ptr<ThrottlingURLLoader>,
                              base::Optional<ResourceRequestCompletionStatus>)>;

  // This method is called to intercept the url response. Caller is responsible
  // for handling the URLLoader later on. The callback should be called on the
  // same thread that URLLoader is constructed.
  virtual void InterceptNavigation(NavigationInterceptionCB callback) = 0;

 protected:
  NavigationURLLoader() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NavigationURLLoader);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_H_
