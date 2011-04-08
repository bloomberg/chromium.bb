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

class DictionaryValue;
class ExtensionDevToolsClientHost;

class DebuggerFunction : public AsyncExtensionFunction {
 protected:
  DebuggerFunction();
  virtual ~DebuggerFunction() {}

  bool InitTabContents(int tab_id);
  bool InitClientHost(int tab_id);

  TabContents* contents_;
  ExtensionDevToolsClientHost* client_host_;
};

// Implements the debugger.attach() extension function.
class AttachDebuggerFunction : public DebuggerFunction {
 public:
  AttachDebuggerFunction();
  ~AttachDebuggerFunction();
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.debugger.attach")
};

// Implements the debugger.detach() extension function.
class DetachDebuggerFunction : public DebuggerFunction {
 public:
  DetachDebuggerFunction();
  ~DetachDebuggerFunction();
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.debugger.detach")
};

// Implements the debugger.sendRequest() extension function.
class SendRequestDebuggerFunction : public DebuggerFunction {
 public:
  SendRequestDebuggerFunction();
  ~SendRequestDebuggerFunction();
  virtual bool RunImpl();

  void SendResponseBody(DictionaryValue* dictionary);
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.debugger.sendRequest")
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_DEBUGGER_API_H_
