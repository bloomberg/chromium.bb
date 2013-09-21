// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Message definition file, included multiple times, hence no include guard.

#include "base/strings/string16.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"
#include "url/gurl.h"

#define IPC_MESSAGE_START ServiceWorkerMsgStart

// Messages sent from the child process to the browser.

IPC_MESSAGE_CONTROL3(ServiceWorkerHostMsg_RegisterServiceWorker,
                     int32 /* callback_id */,
                     string16 /* scope */,
                     GURL /* script_url */)

IPC_MESSAGE_CONTROL2(ServiceWorkerHostMsg_UnregisterServiceWorker,
                     int32 /* callback_id */,
                     string16 /* scope (url pattern) */)

// Messages sent from the browser to the child process.

// Response to ServiceWorkerMsg_RegisterServiceWorker
IPC_MESSAGE_CONTROL1(ServiceWorkerMsg_ServiceWorkerRegistered,
                     int32 /* callback_id */)

// Response to ServiceWorkerMsg_UnregisterServiceWorker
IPC_MESSAGE_CONTROL1(ServiceWorkerMsg_ServiceWorkerUnregistered,
                     int32 /* callback_id */)
