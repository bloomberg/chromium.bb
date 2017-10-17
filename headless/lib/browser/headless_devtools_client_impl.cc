// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/public/internal/headless_devtools_client_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"

namespace headless {

// static
std::unique_ptr<HeadlessDevToolsClient> HeadlessDevToolsClient::Create() {
  return base::WrapUnique(new HeadlessDevToolsClientImpl());
}

// static
HeadlessDevToolsClientImpl* HeadlessDevToolsClientImpl::From(
    HeadlessDevToolsClient* client) {
  // This downcast is safe because there is only one implementation of
  // HeadlessDevToolsClient.
  return static_cast<HeadlessDevToolsClientImpl*>(client);
}

HeadlessDevToolsClientImpl::HeadlessDevToolsClientImpl()
    : agent_host_(nullptr),
      raw_protocol_listener_(nullptr),
      next_message_id_(0),
      next_raw_message_id_(1),
      renderer_crashed_(false),
      accessibility_domain_(this),
      animation_domain_(this),
      application_cache_domain_(this),
      browser_domain_(this),
      cache_storage_domain_(this),
      console_domain_(this),
      css_domain_(this),
      database_domain_(this),
      debugger_domain_(this),
      device_orientation_domain_(this),
      dom_domain_(this),
      dom_debugger_domain_(this),
      dom_snapshot_domain_(this),
      dom_storage_domain_(this),
      emulation_domain_(this),
      headless_experimental_domain_(this),
      heap_profiler_domain_(this),
      indexeddb_domain_(this),
      input_domain_(this),
      inspector_domain_(this),
      io_domain_(this),
      layer_tree_domain_(this),
      log_domain_(this),
      memory_domain_(this),
      network_domain_(this),
      page_domain_(this),
      performance_domain_(this),
      profiler_domain_(this),
      runtime_domain_(this),
      security_domain_(this),
      service_worker_domain_(this),
      target_domain_(this),
      tracing_domain_(this),
      browser_main_thread_(content::BrowserThread::GetTaskRunnerForThread(
          content::BrowserThread::UI)),
      weak_ptr_factory_(this) {}

HeadlessDevToolsClientImpl::~HeadlessDevToolsClientImpl() {}

bool HeadlessDevToolsClientImpl::AttachToHost(
    content::DevToolsAgentHost* agent_host) {
  DCHECK(!agent_host_);
  if (agent_host->IsAttached())
    return false;
  agent_host->AttachClient(this);
  agent_host_ = agent_host;
  return true;
}

void HeadlessDevToolsClientImpl::ForceAttachToHost(
    content::DevToolsAgentHost* agent_host) {
  DCHECK(!agent_host_);
  agent_host_ = agent_host;
  agent_host_->ForceAttachClient(this);
}

void HeadlessDevToolsClientImpl::DetachFromHost(
    content::DevToolsAgentHost* agent_host) {
  DCHECK_EQ(agent_host_, agent_host);
  if (!renderer_crashed_)
    agent_host_->DetachClient(this);
  agent_host_ = nullptr;
  pending_messages_.clear();
}

void HeadlessDevToolsClientImpl::SetRawProtocolListener(
    RawProtocolListener* raw_protocol_listener) {
  raw_protocol_listener_ = raw_protocol_listener;
}

int HeadlessDevToolsClientImpl::GetNextRawDevToolsMessageId() {
  int id = next_raw_message_id_;
  next_raw_message_id_ += 2;
  return id;
}

void HeadlessDevToolsClientImpl::SendRawDevToolsMessage(
    const std::string& json_message) {
#ifndef NDEBUG
  std::unique_ptr<base::Value> message =
      base::JSONReader::Read(json_message, base::JSON_PARSE_RFC);
  const base::DictionaryValue* message_dict;
  int id = 0;
  if (!message || !message->GetAsDictionary(&message_dict) ||
      !message_dict->GetInteger("id", &id)) {
    NOTREACHED() << "Badly formed message";
    return;
  }
  DCHECK_EQ((id % 2), 1) << "Raw devtools messages must have an odd ID.";
#endif

  agent_host_->DispatchProtocolMessage(this, json_message);
}

void HeadlessDevToolsClientImpl::SendRawDevToolsMessage(
    const base::DictionaryValue& message) {
  std::string json_message;
  base::JSONWriter::Write(message, &json_message);
  SendRawDevToolsMessage(json_message);
}

void HeadlessDevToolsClientImpl::DispatchProtocolMessage(
    content::DevToolsAgentHost* agent_host,
    const std::string& json_message) {
  DCHECK_EQ(agent_host_, agent_host);

  std::unique_ptr<base::Value> message =
      base::JSONReader::Read(json_message, base::JSON_PARSE_RFC);
  const base::DictionaryValue* message_dict;
  if (!message || !message->GetAsDictionary(&message_dict)) {
    NOTREACHED() << "Badly formed reply";
    return;
  }

  if (raw_protocol_listener_ &&
      raw_protocol_listener_->OnProtocolMessage(agent_host->GetId(),
                                                json_message, *message_dict)) {
    return;
  }

  if (!DispatchMessageReply(*message_dict) &&
      !DispatchEvent(std::move(message), *message_dict)) {
    DLOG(ERROR) << "Unhandled protocol message: " << json_message;
  }
}

bool HeadlessDevToolsClientImpl::DispatchMessageReply(
    const base::DictionaryValue& message_dict) {
  int id = 0;
  if (!message_dict.GetInteger("id", &id))
    return false;
  auto it = pending_messages_.find(id);
  if (it == pending_messages_.end()) {
    NOTREACHED() << "Unexpected reply";
    return false;
  }
  Callback callback = std::move(it->second);
  pending_messages_.erase(it);
  if (!callback.callback_with_result.is_null()) {
    const base::DictionaryValue* result_dict;
    if (message_dict.GetDictionary("result", &result_dict)) {
      callback.callback_with_result.Run(*result_dict);
    } else if (message_dict.GetDictionary("error", &result_dict)) {
      auto null_value = base::MakeUnique<base::Value>();
      DLOG(ERROR) << "Error in method call result: " << *result_dict;
      callback.callback_with_result.Run(*null_value);
    } else {
      NOTREACHED() << "Reply has neither result nor error";
      return false;
    }
  } else if (!callback.callback.is_null()) {
    callback.callback.Run();
  }
  return true;
}

bool HeadlessDevToolsClientImpl::DispatchEvent(
    std::unique_ptr<base::Value> owning_message,
    const base::DictionaryValue& message_dict) {
  std::string method;
  if (!message_dict.GetString("method", &method))
    return false;
  if (method == "Inspector.targetCrashed")
    renderer_crashed_ = true;
  EventHandlerMap::const_iterator it = event_handlers_.find(method);
  if (it == event_handlers_.end()) {
    NOTREACHED() << "Unknown event: " << method;
    return false;
  }
  if (!it->second.is_null()) {
    const base::DictionaryValue* result_dict;
    if (!message_dict.GetDictionary("params", &result_dict)) {
      NOTREACHED() << "Badly formed event parameters";
      return false;
    }
    // DevTools assumes event handling is async so we must post a task here or
    // we risk breaking things.
    browser_main_thread_->PostTask(
        FROM_HERE, base::Bind(&HeadlessDevToolsClientImpl::DispatchEventTask,
                              weak_ptr_factory_.GetWeakPtr(),
                              base::Passed(std::move(owning_message)),
                              &it->second, result_dict));
  }
  return true;
}

void HeadlessDevToolsClientImpl::DispatchEventTask(
    std::unique_ptr<base::Value> owning_message,
    const EventHandler* event_handler,
    const base::DictionaryValue* result_dict) {
  event_handler->Run(*result_dict);
}

void HeadlessDevToolsClientImpl::AgentHostClosed(
    content::DevToolsAgentHost* agent_host,
    bool replaced_with_another_client) {
  DCHECK_EQ(agent_host_, agent_host);
  agent_host = nullptr;
  pending_messages_.clear();
}

accessibility::Domain* HeadlessDevToolsClientImpl::GetAccessibility() {
  return &accessibility_domain_;
}

animation::Domain* HeadlessDevToolsClientImpl::GetAnimation() {
  return &animation_domain_;
}

application_cache::Domain* HeadlessDevToolsClientImpl::GetApplicationCache() {
  return &application_cache_domain_;
}

browser::Domain* HeadlessDevToolsClientImpl::GetBrowser() {
  return &browser_domain_;
}

cache_storage::Domain* HeadlessDevToolsClientImpl::GetCacheStorage() {
  return &cache_storage_domain_;
}

console::Domain* HeadlessDevToolsClientImpl::GetConsole() {
  return &console_domain_;
}

css::Domain* HeadlessDevToolsClientImpl::GetCSS() {
  return &css_domain_;
}

database::Domain* HeadlessDevToolsClientImpl::GetDatabase() {
  return &database_domain_;
}

debugger::Domain* HeadlessDevToolsClientImpl::GetDebugger() {
  return &debugger_domain_;
}

device_orientation::Domain* HeadlessDevToolsClientImpl::GetDeviceOrientation() {
  return &device_orientation_domain_;
}

dom::Domain* HeadlessDevToolsClientImpl::GetDOM() {
  return &dom_domain_;
}

dom_debugger::Domain* HeadlessDevToolsClientImpl::GetDOMDebugger() {
  return &dom_debugger_domain_;
}

dom_snapshot::Domain* HeadlessDevToolsClientImpl::GetDOMSnapshot() {
  return &dom_snapshot_domain_;
}

dom_storage::Domain* HeadlessDevToolsClientImpl::GetDOMStorage() {
  return &dom_storage_domain_;
}

emulation::Domain* HeadlessDevToolsClientImpl::GetEmulation() {
  return &emulation_domain_;
}

headless_experimental::Domain*
HeadlessDevToolsClientImpl::GetHeadlessExperimental() {
  return &headless_experimental_domain_;
}

heap_profiler::Domain* HeadlessDevToolsClientImpl::GetHeapProfiler() {
  return &heap_profiler_domain_;
}

indexeddb::Domain* HeadlessDevToolsClientImpl::GetIndexedDB() {
  return &indexeddb_domain_;
}

input::Domain* HeadlessDevToolsClientImpl::GetInput() {
  return &input_domain_;
}

inspector::Domain* HeadlessDevToolsClientImpl::GetInspector() {
  return &inspector_domain_;
}

io::Domain* HeadlessDevToolsClientImpl::GetIO() {
  return &io_domain_;
}

layer_tree::Domain* HeadlessDevToolsClientImpl::GetLayerTree() {
  return &layer_tree_domain_;
}

log::Domain* HeadlessDevToolsClientImpl::GetLog() {
  return &log_domain_;
}

memory::Domain* HeadlessDevToolsClientImpl::GetMemory() {
  return &memory_domain_;
}

network::Domain* HeadlessDevToolsClientImpl::GetNetwork() {
  return &network_domain_;
}

page::Domain* HeadlessDevToolsClientImpl::GetPage() {
  return &page_domain_;
}

performance::Domain* HeadlessDevToolsClientImpl::GetPerformance() {
  return &performance_domain_;
}

profiler::Domain* HeadlessDevToolsClientImpl::GetProfiler() {
  return &profiler_domain_;
}

runtime::Domain* HeadlessDevToolsClientImpl::GetRuntime() {
  return &runtime_domain_;
}

security::Domain* HeadlessDevToolsClientImpl::GetSecurity() {
  return &security_domain_;
}

service_worker::Domain* HeadlessDevToolsClientImpl::GetServiceWorker() {
  return &service_worker_domain_;
}

target::Domain* HeadlessDevToolsClientImpl::GetTarget() {
  return &target_domain_;
}

tracing::Domain* HeadlessDevToolsClientImpl::GetTracing() {
  return &tracing_domain_;
}

template <typename CallbackType>
void HeadlessDevToolsClientImpl::FinalizeAndSendMessage(
    base::DictionaryValue* message,
    CallbackType callback) {
  if (renderer_crashed_)
    return;
  DCHECK(agent_host_);
  int id = next_message_id_;
  next_message_id_ += 2;  // We only send even numbered messages.
  message->SetInteger("id", id);
  std::string json_message;
  base::JSONWriter::Write(*message, &json_message);
  pending_messages_[id] = Callback(callback);
  agent_host_->DispatchProtocolMessage(this, json_message);
}

template <typename CallbackType>
void HeadlessDevToolsClientImpl::SendMessageWithParams(
    const char* method,
    std::unique_ptr<base::Value> params,
    CallbackType callback) {
  base::DictionaryValue message;
  message.SetString("method", method);
  message.Set("params", std::move(params));
  FinalizeAndSendMessage(&message, std::move(callback));
}

void HeadlessDevToolsClientImpl::SendMessage(
    const char* method,
    std::unique_ptr<base::Value> params,
    base::Callback<void(const base::Value&)> callback) {
  SendMessageWithParams(method, std::move(params), std::move(callback));
}

void HeadlessDevToolsClientImpl::SendMessage(
    const char* method,
    std::unique_ptr<base::Value> params,
    base::Closure callback) {
  SendMessageWithParams(method, std::move(params), std::move(callback));
}

void HeadlessDevToolsClientImpl::RegisterEventHandler(
    const char* method,
    base::Callback<void(const base::Value&)> callback) {
  DCHECK(event_handlers_.find(method) == event_handlers_.end());
  event_handlers_[method] = callback;
}

HeadlessDevToolsClientImpl::Callback::Callback() {}

HeadlessDevToolsClientImpl::Callback::Callback(Callback&& other) = default;

HeadlessDevToolsClientImpl::Callback::Callback(base::Closure callback)
    : callback(callback) {}

HeadlessDevToolsClientImpl::Callback::Callback(
    base::Callback<void(const base::Value&)> callback)
    : callback_with_result(callback) {}

HeadlessDevToolsClientImpl::Callback::~Callback() {}

HeadlessDevToolsClientImpl::Callback& HeadlessDevToolsClientImpl::Callback::
operator=(Callback&& other) = default;

}  // namespace headless
