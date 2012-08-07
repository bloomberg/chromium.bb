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

#include "base/compiler_specific.h"
#include "chrome/renderer/extensions/chrome_v8_extension.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"

namespace extensions {
class ChromeV8Context;

// Implements the chrome.app JavaScript object.
//
// TODO(aa): Add unit testing for this class.
class AppBindings : public ChromeV8Extension,
                    public ChromeV8ExtensionHandler {
 public:
  explicit AppBindings(Dispatcher* dispatcher,
                       ChromeV8Context* context);

 private:
  // IPC::Listener
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  v8::Handle<v8::Value> GetIsInstalled(const v8::Arguments& args);
  v8::Handle<v8::Value> Install(const v8::Arguments& args);
  v8::Handle<v8::Value> GetDetails(const v8::Arguments& args);
  v8::Handle<v8::Value> GetDetailsForFrame(const v8::Arguments& args);
  v8::Handle<v8::Value> GetAppNotifyChannel(const v8::Arguments& args);
  v8::Handle<v8::Value> GetInstallState(const v8::Arguments& args);
  v8::Handle<v8::Value> GetRunningState(const v8::Arguments& args);

  v8::Handle<v8::Value> GetDetailsForFrameImpl(WebKit::WebFrame* frame);

  void OnGetAppNotifyChannelResponse(const std::string& channel_id,
                                     const std::string& error,
                                     int callback_id);
  void OnAppInstallStateResponse(const std::string& state, int callback_id);

  DISALLOW_COPY_AND_ASSIGN(AppBindings);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_APP_BINDINGS_H_
