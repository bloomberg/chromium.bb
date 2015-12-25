// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_H_
#define CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"

namespace content {

class BrowserContext;
class NavigationURLLoaderDelegate;
class NavigationURLLoaderFactory;
class ServiceWorkerNavigationHandle;
struct CommonNavigationParams;
struct NavigationRequestInfo;

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
  static scoped_ptr<NavigationURLLoader> Create(
      BrowserContext* browser_context,
      scoped_ptr<NavigationRequestInfo> request_info,
      ServiceWorkerNavigationHandle* service_worker_handle,
      NavigationURLLoaderDelegate* delegate);

  // For testing purposes; sets the factory for use in testing.
  static void SetFactoryForTesting(NavigationURLLoaderFactory* factory);

  virtual ~NavigationURLLoader() {}

  // Called in response to OnRequestRedirected to continue processing the
  // request.
  virtual void FollowRedirect() = 0;

 protected:
  NavigationURLLoader() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NavigationURLLoader);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_H_
