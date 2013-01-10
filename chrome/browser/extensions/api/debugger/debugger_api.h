// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the Chrome Extensions Debugger API functions for attaching debugger
// to the page.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DEBUGGER_DEBUGGER_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_DEBUGGER_DEBUGGER_API_H_

#include <string>

#include "chrome/browser/extensions/extension_function.h"

// Base debugger function.

class ExtensionDevToolsClientHost;

namespace base {
class DictionaryValue;
}

namespace content {
class DevToolsClientHost;
class WebContents;
}

class DebuggerFunction : public AsyncExtensionFunction {
 protected:
  DebuggerFunction();
  virtual ~DebuggerFunction() {}

  bool InitWebContents();
  bool InitClientHost();

  content::WebContents* contents_;
  int tab_id_;
  ExtensionDevToolsClientHost* client_host_;
};

// Implements the debugger.attach() extension function.
class AttachDebuggerFunction : public DebuggerFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("debugger.attach")

  AttachDebuggerFunction();

 protected:
  virtual ~AttachDebuggerFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

// Implements the debugger.detach() extension function.
class DetachDebuggerFunction : public DebuggerFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("debugger.detach")

  DetachDebuggerFunction();

 protected:
  virtual ~DetachDebuggerFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

// Implements the debugger.sendCommand() extension function.
class SendCommandDebuggerFunction : public DebuggerFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("debugger.sendCommand")

  SendCommandDebuggerFunction();
  void SendResponseBody(base::DictionaryValue* result);

 protected:
  virtual ~SendCommandDebuggerFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_DEBUGGER_DEBUGGER_API_H_
