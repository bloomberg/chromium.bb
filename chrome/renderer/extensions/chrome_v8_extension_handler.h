// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_CHROME_V8_EXTENSION_HANDLER_H_
#define CHROME_RENDERER_EXTENSIONS_CHROME_V8_EXTENSION_HANDLER_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "ipc/ipc_channel.h"
#include "v8/include/v8.h"

class ChromeV8Context;

// Base class for context-scoped handlers used with ChromeV8Extension.
class ChromeV8ExtensionHandler : public IPC::Channel::Listener {
 public:
  virtual v8::Handle<v8::Value> HandleNativeFunction(
      const std::string& name,
      const v8::Arguments& arguments) = 0;
  virtual ~ChromeV8ExtensionHandler();

  // IPC::Channel::Listener
  virtual bool OnMessageReceived(const IPC::Message& message) = 0;

 protected:
  explicit ChromeV8ExtensionHandler(ChromeV8Context* context);
  int GetRoutingId();
  void Send(IPC::Message* message);
  ChromeV8Context* context_;

 private:
  int routing_id_;
  DISALLOW_COPY_AND_ASSIGN(ChromeV8ExtensionHandler);
};

#endif  // CHROME_RENDERER_EXTENSIONS_CHROME_V8_EXTENSION_HANDLER_H_
