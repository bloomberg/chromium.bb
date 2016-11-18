// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_NATIVE_EXTENSION_BINDINGS_SYSTEM_H_
#define EXTENSIONS_RENDERER_NATIVE_EXTENSION_BINDINGS_SYSTEM_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "extensions/common/extension_messages.h"
#include "extensions/renderer/api_bindings_system.h"
#include "v8/include/v8.h"

namespace extensions {
class ScriptContext;

// The implementation of the Bindings System for extensions code with native
// implementations (rather than JS hooks). Handles permission/availability
// checking and creates all bindings available for a given context. Sending the
// IPC is still abstracted out for testing purposes, but otherwise this should
// be a plug-and-play version for use in the Extensions system.
// Designed to be used in a single thread, but for all contexts on that thread.
class NativeExtensionBindingsSystem {
 public:
  using SendIPCMethod =
      base::Callback<void(const ExtensionHostMsg_Request_Params&)>;

  explicit NativeExtensionBindingsSystem(const SendIPCMethod& send_ipc);
  ~NativeExtensionBindingsSystem();

  // Creates the "chrome" object (if it doesn't already exist) and instantiates
  // the individual APIs on top of it. Only exposes those APIs available to the
  // given context.
  void CreateAPIsInContext(ScriptContext* context);

  // Handles responding to a given request. This signature matches the arguments
  // given to the response IPC.
  void OnResponse(int request_id,
                  bool success,
                  const base::ListValue& response,
                  const std::string& error);

 private:
  // Handles sending a given |request|, forwarding it on to the send_ipc_ after
  // adding additional info.
  void SendRequest(std::unique_ptr<APIBindingsSystem::Request> request,
                   v8::Local<v8::Context> context);

  // Handler to send IPCs. Abstracted out for testing purposes.
  SendIPCMethod send_ipc_;

  // The APIBindingsSystem associated with this class.
  APIBindingsSystem api_system_;

  DISALLOW_COPY_AND_ASSIGN(NativeExtensionBindingsSystem);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_NATIVE_EXTENSION_BINDINGS_SYSTEM_H_
