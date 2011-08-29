// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/resource_context.h"

#include "base/logging.h"
#include "content/browser/browser_thread.h"
#include "webkit/database/database_tracker.h"

namespace content {

ResourceContext::ResourceContext()
    : host_resolver_(NULL),
      request_context_(NULL),
      appcache_service_(NULL),
      database_tracker_(NULL),
      file_system_context_(NULL),
      blob_storage_context_(NULL),
      quota_manager_(NULL),
      host_zoom_map_(NULL),
      media_observer_(NULL) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

ResourceContext::~ResourceContext() {}

void* ResourceContext::GetUserData(const void* key) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  EnsureInitialized();
  UserDataMap::const_iterator found = user_data_.find(key);
  if (found != user_data_.end())
    return found->second;
  return NULL;
}

void ResourceContext::SetUserData(const void* key, void* data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  user_data_[key] = data;
}

net::HostResolver* ResourceContext::host_resolver() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  EnsureInitialized();
  return host_resolver_;
}

void ResourceContext::set_host_resolver(
    net::HostResolver* host_resolver) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  host_resolver_ = host_resolver;
}

net::URLRequestContext* ResourceContext::request_context() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  EnsureInitialized();
  return request_context_;
}

void ResourceContext::set_request_context(
    net::URLRequestContext* request_context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  request_context_ = request_context;
}

ChromeAppCacheService* ResourceContext::appcache_service() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  EnsureInitialized();
  return appcache_service_;
}

void ResourceContext::set_appcache_service(
    ChromeAppCacheService* appcache_service) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  appcache_service_ = appcache_service;
}

webkit_database::DatabaseTracker* ResourceContext::database_tracker() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  EnsureInitialized();
  return database_tracker_;
}

void ResourceContext::set_database_tracker(
    webkit_database::DatabaseTracker* database_tracker) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  database_tracker_ = database_tracker;
}

fileapi::FileSystemContext* ResourceContext::file_system_context() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  EnsureInitialized();
  return file_system_context_;
}

void ResourceContext::set_file_system_context(
    fileapi::FileSystemContext* context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  file_system_context_ = context;
}

ChromeBlobStorageContext* ResourceContext::blob_storage_context() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  EnsureInitialized();
  return blob_storage_context_;
}

void ResourceContext::set_blob_storage_context(
    ChromeBlobStorageContext* context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  blob_storage_context_ = context;
}

quota::QuotaManager* ResourceContext::quota_manager() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  EnsureInitialized();
  return quota_manager_;
}

void ResourceContext::set_quota_manager(
    quota::QuotaManager* quota_manager) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  quota_manager_ = quota_manager;
}

HostZoomMap* ResourceContext::host_zoom_map() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  EnsureInitialized();
  return host_zoom_map_;
}

void ResourceContext::set_host_zoom_map(HostZoomMap* host_zoom_map) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  host_zoom_map_ = host_zoom_map;
}

MediaObserver* ResourceContext::media_observer() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  EnsureInitialized();
  return media_observer_;
}

void ResourceContext::set_media_observer(MediaObserver* media_observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  media_observer_ = media_observer;
}

const DownloadManager::GetNextIdThunkType&
ResourceContext::next_download_id_thunk() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  EnsureInitialized();
  return next_download_id_thunk_;
}
void ResourceContext::set_next_download_id_thunk(
    const DownloadManager::GetNextIdThunkType& thunk) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  next_download_id_thunk_ = thunk;
}

const base::Callback<prerender::PrerenderManager*(void)>&
ResourceContext::prerender_manager_getter() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  EnsureInitialized();
  return prerender_manager_getter_;
}

void ResourceContext::set_prerender_manager_getter(
      const base::Callback<prerender::PrerenderManager*(void)>&
          prerender_manager_getter) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  prerender_manager_getter_ = prerender_manager_getter;
}

}  // namespace content
