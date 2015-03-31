// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_dispatcher_host.h"

#include "base/logging.h"
#include "content/browser/cache_storage/cache_storage_context_impl.h"
#include "content/browser/cache_storage/cache_storage_listener.h"
#include "content/common/cache_storage/cache_storage_messages.h"
#include "content/public/browser/content_browser_client.h"

namespace content {

namespace {

const uint32 kFilteredMessageClasses[] = {CacheStorageMsgStart};

}  // namespace

CacheStorageDispatcherHost::CacheStorageDispatcherHost()
    : BrowserMessageFilter(kFilteredMessageClasses,
                           arraysize(kFilteredMessageClasses)) {
}

CacheStorageDispatcherHost::~CacheStorageDispatcherHost() {
}

void CacheStorageDispatcherHost::Init(CacheStorageContextImpl* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&CacheStorageDispatcherHost::CreateCacheListener, this,
                 make_scoped_refptr(context)));
}

void CacheStorageDispatcherHost::OnDestruct() const {
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

bool CacheStorageDispatcherHost::OnMessageReceived(
    const IPC::Message& message) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(cache_listener_);
  bool handled = cache_listener_->OnMessageReceived(message);
  if (!handled)
    BadMessageReceived();
  return handled;
}

void CacheStorageDispatcherHost::CreateCacheListener(
    CacheStorageContextImpl* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  cache_listener_.reset(new CacheStorageListener(this, context));
}

}  // namespace content
