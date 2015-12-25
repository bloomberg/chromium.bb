// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/resource_context_impl.h"

#include <stdint.h>

#include "base/logging.h"
#include "content/browser/fileapi/chrome_blob_storage_context.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/browser/streams/stream_context.h"
#include "content/browser/webui/url_data_manager_backend.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/keygen_handler.h"
#include "net/ssl/client_cert_store.h"

using base::UserDataAdapter;

namespace content {

namespace {

// Key names on ResourceContext.
const char kBlobStorageContextKeyName[] = "content_blob_storage_context";
const char kStreamContextKeyName[] = "content_stream_context";
const char kURLDataManagerBackendKeyName[] = "url_data_manager_backend";

// Used by the default implementation of GetMediaDeviceIDSalt, below.
std::string ReturnEmptySalt() {
  return std::string();
}

}  // namespace


ResourceContext::ResourceContext() {
  if (ResourceDispatcherHostImpl::Get())
    ResourceDispatcherHostImpl::Get()->AddResourceContext(this);
}

ResourceContext::~ResourceContext() {
  ResourceDispatcherHostImpl* rdhi = ResourceDispatcherHostImpl::Get();
  if (rdhi) {
    rdhi->CancelRequestsForContext(this);
    rdhi->RemoveResourceContext(this);
  }

  // In some tests this object is destructed on UI thread.
  DetachUserDataThread();
}

ResourceContext::SaltCallback ResourceContext::GetMediaDeviceIDSalt() {
  return base::Bind(&ReturnEmptySalt);
}

scoped_ptr<net::ClientCertStore> ResourceContext::CreateClientCertStore() {
  return scoped_ptr<net::ClientCertStore>();
}

void ResourceContext::CreateKeygenHandler(
    uint32_t key_size_in_bits,
    const std::string& challenge_string,
    const GURL& url,
    const base::Callback<void(scoped_ptr<net::KeygenHandler>)>& callback) {
  callback.Run(make_scoped_ptr(
      new net::KeygenHandler(key_size_in_bits, challenge_string, url)));
}

ChromeBlobStorageContext* GetChromeBlobStorageContextForResourceContext(
    const ResourceContext* resource_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return UserDataAdapter<ChromeBlobStorageContext>::Get(
      resource_context, kBlobStorageContextKeyName);
}

StreamContext* GetStreamContextForResourceContext(
    const ResourceContext* resource_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return UserDataAdapter<StreamContext>::Get(
      resource_context, kStreamContextKeyName);
}

URLDataManagerBackend* GetURLDataManagerForResourceContext(
    ResourceContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!context->GetUserData(kURLDataManagerBackendKeyName)) {
    context->SetUserData(kURLDataManagerBackendKeyName,
                         new URLDataManagerBackend());
  }
  return static_cast<URLDataManagerBackend*>(
      context->GetUserData(kURLDataManagerBackendKeyName));
}

void InitializeResourceContext(BrowserContext* browser_context) {
  ResourceContext* resource_context = browser_context->GetResourceContext();

  resource_context->SetUserData(
      kBlobStorageContextKeyName,
      new UserDataAdapter<ChromeBlobStorageContext>(
          ChromeBlobStorageContext::GetFor(browser_context)));

  resource_context->SetUserData(
      kStreamContextKeyName,
      new UserDataAdapter<StreamContext>(
          StreamContext::GetFor(browser_context)));

  resource_context->DetachUserDataThread();
}

}  // namespace content
