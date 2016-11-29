// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/resource_requester_info.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/fileapi/file_system_context.h"

namespace content {

ResourceRequesterInfo::ResourceRequesterInfo(
    RequesterType type,
    int child_id,
    ChromeAppCacheService* appcache_service,
    ChromeBlobStorageContext* blob_storage_context,
    storage::FileSystemContext* file_system_context,
    ServiceWorkerContextWrapper* service_worker_context,
    const GetContextsCallback& get_contexts_callback)
    : type_(type),
      child_id_(child_id),
      appcache_service_(appcache_service),
      blob_storage_context_(blob_storage_context),
      file_system_context_(file_system_context),
      service_worker_context_(service_worker_context),
      get_contexts_callback_(get_contexts_callback) {}

ResourceRequesterInfo::~ResourceRequesterInfo() {}

void ResourceRequesterInfo::set_filter(
    base::WeakPtr<ResourceMessageFilter> filter) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(RequesterType::RENDERER, type_);
  filter_ = filter;
}

void ResourceRequesterInfo::GetContexts(
    ResourceType resource_type,
    ResourceContext** resource_context,
    net::URLRequestContext** request_context) const {
  get_contexts_callback_.Run(resource_type, resource_context, request_context);
}

scoped_refptr<ResourceRequesterInfo> ResourceRequesterInfo::CreateForRenderer(
    int child_id,
    ChromeAppCacheService* appcache_service,
    ChromeBlobStorageContext* blob_storage_context,
    storage::FileSystemContext* file_system_context,
    ServiceWorkerContextWrapper* service_worker_context,
    const GetContextsCallback& get_contexts_callback) {
  return scoped_refptr<ResourceRequesterInfo>(new ResourceRequesterInfo(
      RequesterType::RENDERER, child_id, appcache_service, blob_storage_context,
      file_system_context, service_worker_context, get_contexts_callback));
}

scoped_refptr<ResourceRequesterInfo>
ResourceRequesterInfo::CreateForRendererTesting(int child_id) {
  return scoped_refptr<ResourceRequesterInfo>(new ResourceRequesterInfo(
      RequesterType::RENDERER, child_id, nullptr /* appcache_service */,
      nullptr /* blob_storage_context */, nullptr /* file_system_context */,
      nullptr /*service_worker_context */, GetContextsCallback()));
}

scoped_refptr<ResourceRequesterInfo>
ResourceRequesterInfo::CreateForBrowserSideNavigation(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context) {
  return scoped_refptr<ResourceRequesterInfo>(new ResourceRequesterInfo(
      RequesterType::BROWSER_SIDE_NAVIGATION, -1,
      nullptr /* appcache_service */, nullptr /* blob_storage_context */,
      nullptr /* file_system_context */, service_worker_context.get(),
      GetContextsCallback()));
}

scoped_refptr<ResourceRequesterInfo>
ResourceRequesterInfo::CreateForDownloadOrPageSave(int child_id) {
  return scoped_refptr<ResourceRequesterInfo>(new ResourceRequesterInfo(
      RequesterType::DOWNLOAD_OR_PAGE_SAVE, child_id,
      nullptr /* appcache_service */, nullptr /* blob_storage_context */,
      nullptr /* file_system_context */, nullptr /*service_worker_context */,
      GetContextsCallback()));
}

}  // namespace content
