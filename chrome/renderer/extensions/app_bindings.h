// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The AppObjectExtension is a v8 extension that creates an object
// at window.chrome.app.  This object allows javascript to get details
// on the app state of the page.
// The read-only property app.isInstalled is true if the current page is
// within the extent of an installed, enabled app.

#ifndef CHROME_RENDERER_EXTENSIONS_APP_BINDINGS_H_
#define CHROME_RENDERER_EXTENSIONS_APP_BINDINGS_H_

#include "base/macros.h"
#include "chrome/renderer/extensions/chrome_v8_extension_handler.h"
#include "extensions/renderer/object_backed_native_handler.h"

namespace blink {
class WebLocalFrame;
}

namespace extensions {
class Dispatcher;

// Implements the chrome.app JavaScript object.
//
// TODO(aa): Add unit testing for this class.
class AppBindings : public ObjectBackedNativeHandler,
                    public ChromeV8ExtensionHandler {
 public:
  AppBindings(Dispatcher* dispatcher, ScriptContext* context);
  ~AppBindings() override;

 private:
  // IPC::Listener
  bool OnMessageReceived(const IPC::Message& message) override;

  void GetIsInstalled(const v8::FunctionCallbackInfo<v8::Value>& args);
  void GetDetails(const v8::FunctionCallbackInfo<v8::Value>& args);
  void GetInstallState(const v8::FunctionCallbackInfo<v8::Value>& args);
  void GetRunningState(const v8::FunctionCallbackInfo<v8::Value>& args);

  v8::Local<v8::Value> GetDetailsImpl(blink::WebLocalFrame* frame);

  void OnAppInstallStateResponse(const std::string& state, int callback_id);

  // Dispatcher handle. Not owned.
  Dispatcher* dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(AppBindings);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_APP_BINDINGS_H_
