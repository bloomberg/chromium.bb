// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_NATIVE_EXTENSION_BINDINGS_SYSTEM_H_
#define EXTENSIONS_RENDERER_NATIVE_EXTENSION_BINDINGS_SYSTEM_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "extensions/renderer/api_binding_types.h"
#include "extensions/renderer/api_bindings_system.h"
#include "extensions/renderer/event_emitter.h"
#include "extensions/renderer/extension_bindings_system.h"
#include "v8/include/v8.h"

struct ExtensionHostMsg_Request_Params;

namespace extensions {
class ScriptContext;

// The implementation of the Bindings System for extensions code with native
// implementations (rather than JS hooks). Handles permission/availability
// checking and creates all bindings available for a given context. Sending the
// IPC is still abstracted out for testing purposes, but otherwise this should
// be a plug-and-play version for use in the Extensions system.
// Designed to be used in a single thread, but for all contexts on that thread.
class NativeExtensionBindingsSystem : public ExtensionBindingsSystem {
 public:
  using SendRequestIPCMethod =
      base::Callback<void(ScriptContext*,
                          const ExtensionHostMsg_Request_Params&,
                          binding::RequestThread)>;
  using SendEventListenerIPCMethod =
      base::Callback<void(binding::EventListenersChanged,
                          ScriptContext*,
                          const std::string& event_name,
                          const base::DictionaryValue* filter,
                          bool was_manual)>;

  NativeExtensionBindingsSystem(
      const SendRequestIPCMethod& send_request_ipc,
      const SendEventListenerIPCMethod& send_event_listener_ipc);
  ~NativeExtensionBindingsSystem() override;

  // ExtensionBindingsSystem:
  void DidCreateScriptContext(ScriptContext* context) override;
  void WillReleaseScriptContext(ScriptContext* context) override;
  void UpdateBindingsForContext(ScriptContext* context) override;
  void DispatchEventInContext(const std::string& event_name,
                              const base::ListValue* event_args,
                              const EventFilteringInfo* filtering_info,
                              ScriptContext* context) override;
  bool HasEventListenerInContext(const std::string& event_name,
                                 ScriptContext* context) override;
  void HandleResponse(int request_id,
                      bool success,
                      const base::ListValue& response,
                      const std::string& error) override;
  RequestSender* GetRequestSender() override;

  APIBindingsSystem* api_system() { return &api_system_; }

 private:
  // Handles sending a given |request|, forwarding it on to the send_ipc_ after
  // adding additional info.
  void SendRequest(std::unique_ptr<APIRequestHandler::Request> request,
                   v8::Local<v8::Context> context);

  // Called when listeners for a given event have changed, and forwards it along
  // to |send_event_listener_ipc_|.
  void OnEventListenerChanged(const std::string& event_name,
                              binding::EventListenersChanged change,
                              const base::DictionaryValue* filter,
                              bool was_manual,
                              v8::Local<v8::Context> context);

  // Getter callback for an extension API, since APIs are constructed lazily.
  static void BindingAccessor(v8::Local<v8::Name> name,
                              const v8::PropertyCallbackInfo<v8::Value>& info);

  // Creates and returns the API binding for the given |name|.
  static v8::Local<v8::Object> GetAPIHelper(v8::Local<v8::Context> context,
                                            v8::Local<v8::String> name);

  // Gets the chrome.runtime API binding.
  static v8::Local<v8::Object> GetRuntime(v8::Local<v8::Context> context);

  // Callback to get an API binding for an internal API.
  static void GetInternalAPI(const v8::FunctionCallbackInfo<v8::Value>& info);

  // Helper method to get a APIBindingJSUtil object for the current context,
  // and populate |binding_util_out|. We use an out parameter instead of
  // returning it in order to let us use weak ptrs, which can't be used on a
  // method with a return value.
  void GetJSBindingUtil(v8::Local<v8::Context> context,
                        v8::Local<v8::Value>* binding_util_out);

  // Handler to send request IPCs. Abstracted out for testing purposes.
  SendRequestIPCMethod send_request_ipc_;

  // Handler to notify the browser of event registrations. Abstracted out for
  // testing purposes.
  SendEventListenerIPCMethod send_event_listener_ipc_;

  // The APIBindingsSystem associated with this class.
  APIBindingsSystem api_system_;

  // A function to acquire an internal API.
  v8::Eternal<v8::FunctionTemplate> get_internal_api_;

  base::WeakPtrFactory<NativeExtensionBindingsSystem> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(NativeExtensionBindingsSystem);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_NATIVE_EXTENSION_BINDINGS_SYSTEM_H_
