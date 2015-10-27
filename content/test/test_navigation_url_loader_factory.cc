// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_navigation_url_loader_factory.h"

#include "content/browser/loader/navigation_url_loader.h"
#include "content/test/test_navigation_url_loader.h"

namespace content {

TestNavigationURLLoaderFactory::TestNavigationURLLoaderFactory() {
  NavigationURLLoader::SetFactoryForTesting(this);
}

TestNavigationURLLoaderFactory::~TestNavigationURLLoaderFactory() {
  NavigationURLLoader::SetFactoryForTesting(NULL);
}

scoped_ptr<NavigationURLLoader> TestNavigationURLLoaderFactory::CreateLoader(
    BrowserContext* browser_context,
    scoped_ptr<NavigationRequestInfo> request_info,
    ServiceWorkerNavigationHandle* service_worker_handle,
    NavigationURLLoaderDelegate* delegate) {
  return scoped_ptr<NavigationURLLoader>(new TestNavigationURLLoader(
      request_info.Pass(), delegate));
}

}  // namespace content
