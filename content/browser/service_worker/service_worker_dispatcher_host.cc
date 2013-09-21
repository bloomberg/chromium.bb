// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_dispatcher_host.h"

#include "content/browser/service_worker/service_worker_context.h"
#include "content/common/service_worker_messages.h"
#include "ipc/ipc_message_macros.h"
#include "url/gurl.h"

namespace content {

ServiceWorkerDispatcherHost::ServiceWorkerDispatcherHost(
    ServiceWorkerContext* context)
    : context_(context) {}

ServiceWorkerDispatcherHost::~ServiceWorkerDispatcherHost() {}

bool ServiceWorkerDispatcherHost::OnMessageReceived(const IPC::Message& message,
                                                    bool* message_was_ok) {

  if (IPC_MESSAGE_CLASS(message) != ServiceWorkerMsgStart)
    return false;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(
    ServiceWorkerDispatcherHost, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_RegisterServiceWorker,
                        OnRegisterServiceWorker)
    IPC_MESSAGE_HANDLER(ServiceWorkerHostMsg_UnregisterServiceWorker,
                        OnUnregisterServiceWorker)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void ServiceWorkerDispatcherHost::OnRegisterServiceWorker(int32 registry_id,
                                                       const string16& scope,
                                                       const GURL& script_url) {
  // TODO(alecflett): Enforce that script_url must have the same
  // origin as the registering document.
}

void ServiceWorkerDispatcherHost::OnUnregisterServiceWorker(
    int32 registry_id,
    const string16& scope) {}

}  // namespace content
