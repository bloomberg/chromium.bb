// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/background_sync/background_sync_client_impl.h"

#include <utility>

#include "content/child/background_sync/background_sync_provider.h"
#include "content/child/background_sync/background_sync_type_converters.h"
#include "content/renderer/service_worker/service_worker_context_client.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "third_party/WebKit/public/platform/WebThread.h"
#include "third_party/WebKit/public/platform/modules/background_sync/WebSyncProvider.h"
#include "third_party/WebKit/public/platform/modules/background_sync/WebSyncRegistration.h"
#include "third_party/WebKit/public/web/modules/serviceworker/WebServiceWorkerContextProxy.h"

namespace content {

// static
void BackgroundSyncClientImpl::Create(
    mojo::InterfaceRequest<BackgroundSyncServiceClient> request) {
  new BackgroundSyncClientImpl(std::move(request));
}

BackgroundSyncClientImpl::~BackgroundSyncClientImpl() {}

BackgroundSyncClientImpl::BackgroundSyncClientImpl(
    mojo::InterfaceRequest<BackgroundSyncServiceClient> request)
    : binding_(this, std::move(request)), callback_seq_num_(0) {}

void BackgroundSyncClientImpl::Sync(
    int64_t handle_id,
    content::BackgroundSyncEventLastChance last_chance,
    const SyncCallback& callback) {
  DCHECK(!blink::Platform::current()->mainThread()->isCurrentThread());
  // Get a registration for the given handle_id from the provider. This way
  // the provider knows about the handle and can delete it once Blink releases
  // it.
  // TODO(jkarlin): Change the WebSyncPlatform to support
  // DuplicateRegistrationHandle and then this cast can go.
  BackgroundSyncProvider* provider = static_cast<BackgroundSyncProvider*>(
      blink::Platform::current()->backgroundSyncProvider());

  if (provider == nullptr) {
    // This can happen if the renderer is shutting down when sync fires. See
    // crbug.com/543898. No need to fire the callback as the browser will
    // automatically fail all pending sync events when the mojo connection
    // closes.
    return;
  }

  // TODO(jkarlin): Find a way to claim the handle safely without requiring a
  // round-trip IPC.
  int64_t id = ++callback_seq_num_;
  sync_callbacks_[id] = callback;
  provider->DuplicateRegistrationHandle(
      handle_id, base::Bind(&BackgroundSyncClientImpl::SyncDidGetRegistration,
                            base::Unretained(this), id, last_chance));
}

void BackgroundSyncClientImpl::SyncDidGetRegistration(
    int64_t callback_id,
    content::BackgroundSyncEventLastChance last_chance,
    BackgroundSyncError error,
    SyncRegistrationPtr registration) {
  SyncCallback callback;
  auto it = sync_callbacks_.find(callback_id);
  DCHECK(it != sync_callbacks_.end());
  callback = it->second;
  sync_callbacks_.erase(it);

  if (error != BACKGROUND_SYNC_ERROR_NONE) {
    callback.Run(SERVICE_WORKER_EVENT_STATUS_ABORTED);
    return;
  }

  ServiceWorkerContextClient* client =
      ServiceWorkerContextClient::ThreadSpecificInstance();
  if (!client) {
    callback.Run(SERVICE_WORKER_EVENT_STATUS_ABORTED);
    return;
  }

  scoped_ptr<blink::WebSyncRegistration> web_registration =
      mojo::ConvertTo<scoped_ptr<blink::WebSyncRegistration>>(registration);

  blink::WebServiceWorkerContextProxy::LastChanceOption web_last_chance =
      mojo::ConvertTo<blink::WebServiceWorkerContextProxy::LastChanceOption>(
          last_chance);

  client->DispatchSyncEvent(*web_registration, web_last_chance, callback);
}

}  // namespace content
