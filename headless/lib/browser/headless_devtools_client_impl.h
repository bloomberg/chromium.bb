// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_DEVTOOLS_CLIENT_IMPL_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_DEVTOOLS_CLIENT_IMPL_H_

#include <unordered_map>

#include "content/public/browser/devtools_agent_host_client.h"
#include "headless/public/domains/accessibility.h"
#include "headless/public/domains/animation.h"
#include "headless/public/domains/application_cache.h"
#include "headless/public/domains/cache_storage.h"
#include "headless/public/domains/console.h"
#include "headless/public/domains/css.h"
#include "headless/public/domains/database.h"
#include "headless/public/domains/debugger.h"
#include "headless/public/domains/device_orientation.h"
#include "headless/public/domains/dom.h"
#include "headless/public/domains/dom_debugger.h"
#include "headless/public/domains/dom_storage.h"
#include "headless/public/domains/emulation.h"
#include "headless/public/domains/heap_profiler.h"
#include "headless/public/domains/indexeddb.h"
#include "headless/public/domains/input.h"
#include "headless/public/domains/inspector.h"
#include "headless/public/domains/io.h"
#include "headless/public/domains/layer_tree.h"
#include "headless/public/domains/memory.h"
#include "headless/public/domains/network.h"
#include "headless/public/domains/page.h"
#include "headless/public/domains/profiler.h"
#include "headless/public/domains/rendering.h"
#include "headless/public/domains/runtime.h"
#include "headless/public/domains/security.h"
#include "headless/public/domains/service_worker.h"
#include "headless/public/domains/tracing.h"
#include "headless/public/domains/worker.h"
#include "headless/public/headless_devtools_client.h"
#include "headless/public/internal/message_dispatcher.h"

namespace base {
class DictionaryValue;
}

namespace content {
class DevToolsAgentHost;
}

namespace headless {

class HeadlessDevToolsClientImpl : public HeadlessDevToolsClient,
                                   public content::DevToolsAgentHostClient,
                                   public internal::MessageDispatcher {
 public:
  HeadlessDevToolsClientImpl();
  ~HeadlessDevToolsClientImpl() override;

  static HeadlessDevToolsClientImpl* From(HeadlessDevToolsClient* client);

  // HeadlessDevToolsClient implementation:
  accessibility::Domain* GetAccessibility() override;
  animation::Domain* GetAnimation() override;
  application_cache::Domain* GetApplicationCache() override;
  cache_storage::Domain* GetCacheStorage() override;
  console::Domain* GetConsole() override;
  css::Domain* GetCSS() override;
  database::Domain* GetDatabase() override;
  debugger::Domain* GetDebugger() override;
  device_orientation::Domain* GetDeviceOrientation() override;
  dom::Domain* GetDOM() override;
  dom_debugger::Domain* GetDOMDebugger() override;
  dom_storage::Domain* GetDOMStorage() override;
  emulation::Domain* GetEmulation() override;
  heap_profiler::Domain* GetHeapProfiler() override;
  indexeddb::Domain* GetIndexedDB() override;
  input::Domain* GetInput() override;
  inspector::Domain* GetInspector() override;
  io::Domain* GetIO() override;
  layer_tree::Domain* GetLayerTree() override;
  memory::Domain* GetMemory() override;
  network::Domain* GetNetwork() override;
  page::Domain* GetPage() override;
  profiler::Domain* GetProfiler() override;
  rendering::Domain* GetRendering() override;
  runtime::Domain* GetRuntime() override;
  security::Domain* GetSecurity() override;
  service_worker::Domain* GetServiceWorker() override;
  tracing::Domain* GetTracing() override;
  worker::Domain* GetWorker() override;

  // content::DevToolstAgentHostClient implementation:
  void DispatchProtocolMessage(content::DevToolsAgentHost* agent_host,
                               const std::string& json_message) override;
  void AgentHostClosed(content::DevToolsAgentHost* agent_host,
                       bool replaced_with_another_client) override;

  // internal::MessageDispatcher implementation:
  void SendMessage(const char* method,
                   std::unique_ptr<base::Value> params,
                   base::Callback<void(const base::Value&)> callback) override;
  void SendMessage(const char* method,
                   std::unique_ptr<base::Value> params,
                   base::Callback<void()> callback) override;
  void SendMessage(const char* method,
                   base::Callback<void(const base::Value&)> callback) override;
  void SendMessage(const char* method,
                   base::Callback<void()> callback) override;

  void AttachToHost(content::DevToolsAgentHost* agent_host);
  void DetachFromHost(content::DevToolsAgentHost* agent_host);

 private:
  // Represents a message for which we are still waiting for a reply. Contains
  // a callback with or without a result parameter depending on the message that
  // is pending.
  struct PendingMessage {
    PendingMessage();
    PendingMessage(PendingMessage&& other);
    explicit PendingMessage(base::Callback<void()> callback);
    explicit PendingMessage(base::Callback<void(const base::Value&)> callback);
    ~PendingMessage();

    PendingMessage& operator=(PendingMessage&& other);

    // TODO(skyostil): Use a class union once allowed.
    base::Callback<void()> callback;
    base::Callback<void(const base::Value&)> callback_with_result;
  };

  template <typename CallbackType>
  void FinalizeAndSendMessage(base::DictionaryValue* message,
                              CallbackType callback);

  template <typename CallbackType>
  void SendMessageWithParams(const char* method,
                             std::unique_ptr<base::Value> params,
                             CallbackType callback);

  template <typename CallbackType>
  void SendMessageWithoutParams(const char* method, CallbackType callback);

  content::DevToolsAgentHost* agent_host_;  // Not owned.
  int next_message_id_;
  std::unordered_map<int, PendingMessage> pending_messages_;

  accessibility::Domain accessibility_domain_;
  animation::Domain animation_domain_;
  application_cache::Domain application_cache_domain_;
  cache_storage::Domain cache_storage_domain_;
  console::Domain console_domain_;
  css::Domain css_domain_;
  database::Domain database_domain_;
  debugger::Domain debugger_domain_;
  device_orientation::Domain device_orientation_domain_;
  dom_debugger::Domain dom_debugger_domain_;
  dom::Domain dom_domain_;
  dom_storage::Domain dom_storage_domain_;
  emulation::Domain emulation_domain_;
  heap_profiler::Domain heap_profiler_domain_;
  indexeddb::Domain indexeddb_domain_;
  input::Domain input_domain_;
  inspector::Domain inspector_domain_;
  io::Domain io_domain_;
  layer_tree::Domain layer_tree_domain_;
  memory::Domain memory_domain_;
  network::Domain network_domain_;
  page::Domain page_domain_;
  profiler::Domain profiler_domain_;
  rendering::Domain rendering_domain_;
  runtime::Domain runtime_domain_;
  security::Domain security_domain_;
  service_worker::Domain service_worker_domain_;
  tracing::Domain tracing_domain_;
  worker::Domain worker_domain_;

  DISALLOW_COPY_AND_ASSIGN(HeadlessDevToolsClientImpl);
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_DEVTOOLS_CLIENT_IMPL_H_
