// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/shell_resource_context.h"

#include "content/browser/chrome_blob_storage_context.h"
#include "content/shell/shell_url_request_context_getter.h"

namespace content {

ShellResourceContext::ShellResourceContext(
    ShellURLRequestContextGetter* getter,
    ChromeBlobStorageContext* blob_storage_context)
    : getter_(getter),
      blob_storage_context_(blob_storage_context) {
}

ShellResourceContext::~ShellResourceContext() {
}

net::HostResolver* ShellResourceContext::GetHostResolver() {
  return getter_->host_resolver();
}

net::URLRequestContext* ShellResourceContext::GetRequestContext() {
  return getter_->GetURLRequestContext();
}

ChromeAppCacheService* ShellResourceContext::GetAppCacheService() {
  return NULL;
}

webkit_database::DatabaseTracker* ShellResourceContext::GetDatabaseTracker() {
  return NULL;
}

fileapi::FileSystemContext* ShellResourceContext::GetFileSystemContext() {
  return NULL;
}

ChromeBlobStorageContext* ShellResourceContext::GetBlobStorageContext() {
  return blob_storage_context_;
}

quota::QuotaManager* ShellResourceContext::GetQuotaManager() {
  return NULL;
}

HostZoomMap* ShellResourceContext::GetHostZoomMap() {
  return NULL;
}

MediaObserver* ShellResourceContext::GetMediaObserver() {
  return NULL;
}

media_stream::MediaStreamManager*
    ShellResourceContext::GetMediaStreamManager() {
  return NULL;
}

AudioManager* ShellResourceContext::GetAudioManager() {
  return NULL;
}

WebKitContext* ShellResourceContext::GetWebKitContext() {
  return NULL;
}

}  // namespace content
