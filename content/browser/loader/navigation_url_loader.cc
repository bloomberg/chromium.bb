// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/navigation_url_loader.h"

#include <utility>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "content/browser/frame_host/navigation_request_info.h"
#include "content/browser/loader/navigation_url_loader_factory.h"
#include "content/browser/loader/navigation_url_loader_impl.h"
#include "content/browser/loader/navigation_url_loader_network_service.h"
#include "content/browser/loader/url_loader_request_handler.h"
#include "content/public/browser/navigation_ui_data.h"
#include "content/public/common/content_features.h"

namespace content {

static NavigationURLLoaderFactory* g_loader_factory = nullptr;

std::unique_ptr<NavigationURLLoader> NavigationURLLoader::Create(
    ResourceContext* resource_context,
    StoragePartition* storage_partition,
    std::unique_ptr<NavigationRequestInfo> request_info,
    std::unique_ptr<NavigationUIData> navigation_ui_data,
    ServiceWorkerNavigationHandle* service_worker_handle,
    AppCacheNavigationHandle* appcache_handle,
    NavigationURLLoaderDelegate* delegate) {
  if (g_loader_factory) {
    return g_loader_factory->CreateLoader(
        resource_context, storage_partition, std::move(request_info),
        std::move(navigation_ui_data), service_worker_handle, delegate);
  }
  if (base::FeatureList::IsEnabled(features::kNetworkService)) {
    return base::MakeUnique<NavigationURLLoaderNetworkService>(
        resource_context, storage_partition, std::move(request_info),
        std::move(navigation_ui_data), service_worker_handle, appcache_handle,
        delegate, std::vector<std::unique_ptr<URLLoaderRequestHandler>>());
  } else {
    return base::MakeUnique<NavigationURLLoaderImpl>(
        resource_context, storage_partition, std::move(request_info),
        std::move(navigation_ui_data), service_worker_handle, appcache_handle,
        delegate);
  }
}

void NavigationURLLoader::SetFactoryForTesting(
    NavigationURLLoaderFactory* factory) {
  DCHECK(g_loader_factory == nullptr || factory == nullptr);
  g_loader_factory = factory;
}

}  // namespace content
