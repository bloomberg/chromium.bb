// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_CHROME_V8_EXTENSION_HANDLER_H_
#define CHROME_RENDERER_EXTENSIONS_CHROME_V8_EXTENSION_HANDLER_H_

#include <string>

#include "base/basictypes.h"
#include "ipc/ipc_listener.h"
#include "v8/include/v8.h"

namespace extensions {
class ScriptContext;

// Base class for context-scoped handlers used with ChromeV8Extension.
// TODO(koz): Rename/refactor this somehow. Maybe just pull it into
// ChromeV8Extension.
class ChromeV8ExtensionHandler : public IPC::Listener {
 public:
  virtual ~ChromeV8ExtensionHandler();

  // IPC::Listener
  virtual bool OnMessageReceived(const IPC::Message& message) = 0;

 protected:
  explicit ChromeV8ExtensionHandler(ScriptContext* context);
  int GetRoutingID();
  void Send(IPC::Message* message);
  ScriptContext* context_;

 private:
  int routing_id_;
  DISALLOW_COPY_AND_ASSIGN(ChromeV8ExtensionHandler);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_CHROME_V8_EXTENSION_HANDLER_H_
