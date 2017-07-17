// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_SERVICE_WORKER_DATA_H_
#define EXTENSIONS_RENDERER_SERVICE_WORKER_DATA_H_

#include <memory>

#include "base/macros.h"
#include "extensions/renderer/v8_schema_registry.h"

namespace extensions {
class ExtensionBindingsSystem;
class IPCMessageSender;
class ScriptContext;

// Per ServiceWorker data in worker thread.
// TODO(lazyboy): Also put worker ScriptContexts in this.
class ServiceWorkerData {
 public:
  ServiceWorkerData(int64_t service_worker_version_id,
                    ScriptContext* context,
                    std::unique_ptr<ExtensionBindingsSystem> bindings_system,
                    std::unique_ptr<IPCMessageSender> ipc_message_sender);
  ~ServiceWorkerData();

  V8SchemaRegistry* v8_schema_registry() { return v8_schema_registry_.get(); }
  ExtensionBindingsSystem* bindings_system() { return bindings_system_.get(); }
  int64_t service_worker_version_id() const {
    return service_worker_version_id_;
  }
  ScriptContext* context() const { return context_; }
  IPCMessageSender* ipc_message_sender() { return ipc_message_sender_.get(); }

 private:
  const int64_t service_worker_version_id_;
  ScriptContext* const context_;

  std::unique_ptr<V8SchemaRegistry> v8_schema_registry_;
  std::unique_ptr<IPCMessageSender> ipc_message_sender_;
  std::unique_ptr<ExtensionBindingsSystem> bindings_system_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerData);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_SERVICE_WORKER_DATA_H_
