// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_sync/background_sync_context_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/stl_util.h"
#include "content/browser/background_sync/background_sync_manager.h"
#include "content/browser/background_sync/background_sync_service_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_thread.h"

namespace content {

BackgroundSyncContextImpl::BackgroundSyncContextImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

BackgroundSyncContextImpl::~BackgroundSyncContextImpl() {
  DCHECK(!background_sync_manager_);
  DCHECK(services_.empty());
}

void BackgroundSyncContextImpl::Init(
    const scoped_refptr<ServiceWorkerContextWrapper>& context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&BackgroundSyncContextImpl::CreateBackgroundSyncManager, this,
                 context));
}

void BackgroundSyncContextImpl::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&BackgroundSyncContextImpl::ShutdownOnIO, this));
}

void BackgroundSyncContextImpl::CreateService(
    mojo::InterfaceRequest<BackgroundSyncService> request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&BackgroundSyncContextImpl::CreateServiceOnIOThread, this,
                 base::Passed(&request)));
}

void BackgroundSyncContextImpl::ServiceHadConnectionError(
    BackgroundSyncServiceImpl* service) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(ContainsValue(services_, service));

  services_.erase(service);
  delete service;
}

BackgroundSyncManager* BackgroundSyncContextImpl::background_sync_manager()
    const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  return background_sync_manager_.get();
}

void BackgroundSyncContextImpl::CreateBackgroundSyncManager(
    const scoped_refptr<ServiceWorkerContextWrapper>& context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!background_sync_manager_);

  background_sync_manager_ = BackgroundSyncManager::Create(context);
}

void BackgroundSyncContextImpl::CreateServiceOnIOThread(
    mojo::InterfaceRequest<BackgroundSyncService> request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(background_sync_manager_);
  services_.insert(new BackgroundSyncServiceImpl(this, std::move(request)));
}

void BackgroundSyncContextImpl::ShutdownOnIO() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  STLDeleteElements(&services_);
  background_sync_manager_.reset();
}

}  // namespace content
