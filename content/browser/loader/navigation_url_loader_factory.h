// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_FACTORY_H_
#define CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_FACTORY_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/loader/navigation_url_loader.h"

namespace content {

// NavigationURLLoader::Create uses the currently registered factory to create
// the loader. This is intended for testing.
class NavigationURLLoaderFactory {
 public:
  virtual scoped_ptr<NavigationURLLoader> CreateLoader(
      BrowserContext* browser_context,
      scoped_ptr<NavigationRequestInfo> request_info,
      ServiceWorkerNavigationHandle* service_worker_handle,
      NavigationURLLoaderDelegate* delegate) = 0;

 protected:
  NavigationURLLoaderFactory() {}
  virtual ~NavigationURLLoaderFactory() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NavigationURLLoaderFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_NAVIGATION_URL_LOADER_FACTORY_H_
