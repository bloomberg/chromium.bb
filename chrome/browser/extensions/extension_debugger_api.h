// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the Chrome Extensions Debugger API functions for attaching debugger
// to the page.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_DEBUGGER_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_DEBUGGER_API_H_
#pragma once

#include <string>

#include "chrome/browser/extensions/extension_function.h"

// Base debugger function.

class ExtensionDevToolsClientHost;
class TabContents;

namespace base {
class DictionaryValue;
}

class DebuggerFunction : public AsyncExtensionFunction {
 protected:
  DebuggerFunction();
  virtual ~DebuggerFunction() {}

  bool InitTabContents();
  bool InitClientHost();

  TabContents* contents_;
  int tab_id_;
  ExtensionDevToolsClientHost* client_host_;
};

// Implements the debugger.attach() extension function.
class AttachDebuggerFunction : public DebuggerFunction {
 public:
  AttachDebuggerFunction();
  virtual ~AttachDebuggerFunction();
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.debugger.attach")
};

// Implements the debugger.detach() extension function.
class DetachDebuggerFunction : public DebuggerFunction {
 public:
  DetachDebuggerFunction();
  virtual ~DetachDebuggerFunction();
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.debugger.detach")
};

// Implements the debugger.sendCommand() extension function.
class SendCommandDebuggerFunction : public DebuggerFunction {
 public:
  SendCommandDebuggerFunction();
  virtual ~SendCommandDebuggerFunction();
  virtual bool RunImpl() OVERRIDE;

  void SendResponseBody(base::DictionaryValue* dictionary);
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.debugger.sendCommand")
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_DEBUGGER_API_H_
