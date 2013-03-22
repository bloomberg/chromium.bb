// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/resource_message_filter.h"

#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/fileapi/chrome_blob_storage_context.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/public/browser/resource_context.h"
#include "webkit/fileapi/file_system_context.h"

namespace content {

ResourceMessageFilter::ResourceMessageFilter(
    int child_id,
    int process_type,
    ResourceContext* resource_context,
    ChromeAppCacheService* appcache_service,
    ChromeBlobStorageContext* blob_storage_context,
    fileapi::FileSystemContext* file_system_context,
    URLRequestContextSelector* url_request_context_selector)
    : child_id_(child_id),
      process_type_(process_type),
      resource_context_(resource_context),
      appcache_service_(appcache_service),
      blob_storage_context_(blob_storage_context),
      file_system_context_(file_system_context),
      url_request_context_selector_(url_request_context_selector) {
  DCHECK(resource_context);
  DCHECK(url_request_context_selector);
  // |appcache_service| and |blob_storage_context| may be NULL in unittests.
}

ResourceMessageFilter::~ResourceMessageFilter() {
}

void ResourceMessageFilter::OnChannelClosing() {
  BrowserMessageFilter::OnChannelClosing();

  // Unhook us from all pending network requests so they don't get sent to a
  // deleted object.
  ResourceDispatcherHostImpl::Get()->CancelRequestsForProcess(child_id_);
}

bool ResourceMessageFilter::OnMessageReceived(const IPC::Message& message,
                                              bool* message_was_ok) {
  return ResourceDispatcherHostImpl::Get()->OnMessageReceived(
      message, this, message_was_ok);
}

net::URLRequestContext* ResourceMessageFilter::GetURLRequestContext(
    ResourceType::Type type) {
  return url_request_context_selector_->GetRequestContext(type);
}

}  // namespace content
