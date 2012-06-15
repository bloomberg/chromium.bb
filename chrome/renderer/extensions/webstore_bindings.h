// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_WEBSTORE_BINDINGS_H_
#define CHROME_RENDERER_EXTENSIONS_WEBSTORE_BINDINGS_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/renderer/extensions/chrome_v8_extension.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"

class ChromeV8Context;

// A V8 extension that creates an object at window.chrome.webstore. This object
// allows JavaScript to initiate inline installs of apps that are listed in the
// Chrome Web Store (CWS).
class WebstoreBindings : public ChromeV8Extension,
                         public ChromeV8ExtensionHandler {
 public:
  explicit WebstoreBindings(ExtensionDispatcher* dispatcher,
                            ChromeV8Context* context);

  // IPC::Listener
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  v8::Handle<v8::Value> Install(const v8::Arguments& args);

  void OnInlineWebstoreInstallResponse(
      int install_id, bool success, const std::string& error);

  // Extracts a Web Store item ID from a <link rel="chrome-webstore-item"
  // href="https://chrome.google.com/webstore/detail/id"> node found in the
  // frame. On success, true will be returned and the |webstore_item_id|
  // parameter will be populated with the ID. On failure, false will be returned
  // and |error| will be populated with the error.
  static bool GetWebstoreItemIdFromFrame(
      WebKit::WebFrame* frame, const std::string& preferred_store_link_url,
      std::string* webstore_item_id, std::string* error);

  DISALLOW_COPY_AND_ASSIGN(WebstoreBindings);
};

#endif  // CHROME_RENDERER_EXTENSIONS_WEBSTORE_BINDINGS_H_
