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
      blob_storage_context_(NULL) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

ResourceContext::~ResourceContext() {}

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

}  // namespace content
