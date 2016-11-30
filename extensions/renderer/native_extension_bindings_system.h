// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_NATIVE_EXTENSION_BINDINGS_SYSTEM_H_
#define EXTENSIONS_RENDERER_NATIVE_EXTENSION_BINDINGS_SYSTEM_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "extensions/renderer/api_bindings_system.h"
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
  using SendIPCMethod =
      base::Callback<void(ScriptContext*,
                          const ExtensionHostMsg_Request_Params&)>;

  explicit NativeExtensionBindingsSystem(const SendIPCMethod& send_ipc);
  ~NativeExtensionBindingsSystem() override;

  // ExtensionBindingsSystem:
  void DidCreateScriptContext(ScriptContext* context) override;
  void WillReleaseScriptContext(ScriptContext* context) override;
  void UpdateBindingsForContext(ScriptContext* context) override;
  void DispatchEventInContext(const std::string& event_name,
                              const base::ListValue* event_args,
                              const base::DictionaryValue* filtering_info,
                              ScriptContext* context) override;
  void HandleResponse(int request_id,
                      bool success,
                      const base::ListValue& response,
                      const std::string& error) override;
  RequestSender* GetRequestSender() override;

 private:
  // Handles sending a given |request|, forwarding it on to the send_ipc_ after
  // adding additional info.
  void SendRequest(std::unique_ptr<APIBindingsSystem::Request> request,
                   v8::Local<v8::Context> context);

  // Getter callback for an extension API, since APIs are constructed lazily.
  static void GetAPIHelper(v8::Local<v8::Name> name,
                           const v8::PropertyCallbackInfo<v8::Value>& info);

  // Handler to send IPCs. Abstracted out for testing purposes.
  SendIPCMethod send_ipc_;

  // The APIBindingsSystem associated with this class.
  APIBindingsSystem api_system_;

  base::WeakPtrFactory<NativeExtensionBindingsSystem> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(NativeExtensionBindingsSystem);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_NATIVE_EXTENSION_BINDINGS_SYSTEM_H_
