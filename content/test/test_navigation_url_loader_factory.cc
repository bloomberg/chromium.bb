// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_navigation_url_loader_factory.h"

#include <utility>

#include "content/browser/loader/navigation_url_loader.h"
#include "content/test/test_navigation_url_loader.h"

namespace content {

TestNavigationURLLoaderFactory::TestNavigationURLLoaderFactory() {
  NavigationURLLoader::SetFactoryForTesting(this);
}

TestNavigationURLLoaderFactory::~TestNavigationURLLoaderFactory() {
  NavigationURLLoader::SetFactoryForTesting(NULL);
}

std::unique_ptr<NavigationURLLoader>
TestNavigationURLLoaderFactory::CreateLoader(
    BrowserContext* browser_context,
    std::unique_ptr<NavigationRequestInfo> request_info,
    ServiceWorkerContextWrapper* service_worker_context_wrapper,
    NavigationURLLoaderDelegate* delegate) {
  return std::unique_ptr<NavigationURLLoader>(
      new TestNavigationURLLoader(std::move(request_info), delegate));
}

}  // namespace content
