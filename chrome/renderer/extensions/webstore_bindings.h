// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_WEBSTORE_BINDINGS_H_
#define CHROME_RENDERER_EXTENSIONS_WEBSTORE_BINDINGS_H_

#include "base/compiler_specific.h"
#include "chrome/renderer/extensions/chrome_v8_extension_handler.h"
#include "extensions/renderer/object_backed_native_handler.h"
#include "third_party/WebKit/public/web/WebFrame.h"

namespace extensions {

// A V8 extension that creates an object at window.chrome.webstore. This object
// allows JavaScript to initiate inline installs of apps that are listed in the
// Chrome Web Store (CWS).
class WebstoreBindings : public ObjectBackedNativeHandler,
                         public ChromeV8ExtensionHandler {
 public:
  explicit WebstoreBindings(ScriptContext* context);

  // IPC::Listener
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  void Install(const v8::FunctionCallbackInfo<v8::Value>& args);

  void OnInlineWebstoreInstallResponse(
      int install_id, bool success, const std::string& error);

  void OnInlineInstallStageChanged(int stage);

  void OnInlineInstallDownloadProgress(int percent_downloaded);

  // Extracts a Web Store item ID from a <link rel="chrome-webstore-item"
  // href="https://chrome.google.com/webstore/detail/id"> node found in the
  // frame. On success, true will be returned and the |webstore_item_id|
  // parameter will be populated with the ID. On failure, false will be returned
  // and |error| will be populated with the error.
  static bool GetWebstoreItemIdFromFrame(
      blink::WebFrame* frame, const std::string& preferred_store_link_url,
      std::string* webstore_item_id, std::string* error);

  DISALLOW_COPY_AND_ASSIGN(WebstoreBindings);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_WEBSTORE_BINDINGS_H_
