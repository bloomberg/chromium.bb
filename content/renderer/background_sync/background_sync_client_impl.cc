// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/background_sync/background_sync_client_impl.h"

#include "content/child/background_sync/background_sync_type_converters.h"
#include "content/renderer/service_worker/service_worker_context_client.h"
#include "third_party/WebKit/public/platform/modules/background_sync/WebSyncRegistration.h"

namespace content {

// static
void BackgroundSyncClientImpl::Create(
    mojo::InterfaceRequest<BackgroundSyncServiceClient> request) {
  new BackgroundSyncClientImpl(request.Pass());
}

BackgroundSyncClientImpl::~BackgroundSyncClientImpl() {}

BackgroundSyncClientImpl::BackgroundSyncClientImpl(
    mojo::InterfaceRequest<BackgroundSyncServiceClient> request)
    : binding_(this, request.Pass()) {}

void BackgroundSyncClientImpl::Sync(content::SyncRegistrationPtr registration,
                                    const SyncCallback& callback) {
  ServiceWorkerContextClient* client =
      ServiceWorkerContextClient::ThreadSpecificInstance();
  if (!client) {
    callback.Run(SERVICE_WORKER_EVENT_STATUS_ABORTED);
    return;
  }
  scoped_ptr<blink::WebSyncRegistration> reg =
      mojo::ConvertTo<scoped_ptr<blink::WebSyncRegistration>>(registration);
  client->DispatchSyncEvent(*reg, callback);
}

}  // namespace content
