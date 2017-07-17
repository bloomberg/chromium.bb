// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/service_worker_data.h"

#include "extensions/renderer/extension_bindings_system.h"
#include "extensions/renderer/ipc_message_sender.h"

namespace extensions {

ServiceWorkerData::ServiceWorkerData(
    int64_t service_worker_version_id,
    ScriptContext* context,
    std::unique_ptr<ExtensionBindingsSystem> bindings_system,
    std::unique_ptr<IPCMessageSender> ipc_message_sender)
    : service_worker_version_id_(service_worker_version_id),
      context_(context),
      v8_schema_registry_(new V8SchemaRegistry),
      ipc_message_sender_(std::move(ipc_message_sender)),
      bindings_system_(std::move(bindings_system)) {}

ServiceWorkerData::~ServiceWorkerData() {}

}  // namespace extensions
