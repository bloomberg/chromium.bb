// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/navigation_url_loader.h"

#include <utility>

#include "content/browser/frame_host/navigation_request_info.h"
#include "content/browser/loader/navigation_url_loader_factory.h"
#include "content/browser/loader/navigation_url_loader_impl.h"
#include "content/public/browser/navigation_ui_data.h"

namespace content {

static NavigationURLLoaderFactory* g_factory = nullptr;

std::unique_ptr<NavigationURLLoader> NavigationURLLoader::Create(
    ResourceContext* resource_context,
    StoragePartition* storage_partition,
    std::unique_ptr<NavigationRequestInfo> request_info,
    std::unique_ptr<NavigationUIData> navigation_ui_data,
    ServiceWorkerNavigationHandle* service_worker_handle,
    AppCacheNavigationHandle* appcache_handle,
    NavigationURLLoaderDelegate* delegate) {
  if (g_factory) {
    return g_factory->CreateLoader(
        resource_context, storage_partition, std::move(request_info),
        std::move(navigation_ui_data), service_worker_handle, delegate);
  }
  return std::unique_ptr<NavigationURLLoader>(new NavigationURLLoaderImpl(
      resource_context, storage_partition, std::move(request_info),
      std::move(navigation_ui_data), service_worker_handle, appcache_handle,
      delegate));
}

void NavigationURLLoader::SetFactoryForTesting(
    NavigationURLLoaderFactory* factory) {
  DCHECK(g_factory == nullptr || factory == nullptr);
  g_factory = factory;
}

}  // namespace content
