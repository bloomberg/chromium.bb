// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_SEND_REQUEST_NATIVES_H_
#define CHROME_RENDERER_EXTENSIONS_SEND_REQUEST_NATIVES_H_

#include "chrome/renderer/extensions/chrome_v8_extension.h"
#include "chrome/renderer/extensions/dispatcher.h"

#include "v8/include/v8.h"

namespace extensions {
class RequestSender;

// Native functions exposed to extensions via JS for calling API functions in
// the browser.
class SendRequestNatives : public ChromeV8Extension {
 public:
  SendRequestNatives(Dispatcher* dispatcher, RequestSender* request_sender);

 private:
  v8::Handle<v8::Value> GetNextRequestId(const v8::Arguments& args);
  // Starts an API request to the browser, with an optional callback.  The
  // callback will be dispatched to EventBindings::HandleResponse.
  v8::Handle<v8::Value> StartRequest(const v8::Arguments& args);

  RequestSender* request_sender_;

  DISALLOW_COPY_AND_ASSIGN(SendRequestNatives);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_SEND_REQUEST_NATIVES_H_
