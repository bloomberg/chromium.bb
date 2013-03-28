// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the Chrome Extensions Debugger API functions for attaching debugger
// to the page.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DEBUGGER_DEBUGGER_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_DEBUGGER_DEBUGGER_API_H_

#include <string>

#include "chrome/browser/extensions/extension_function.h"
#include "chrome/common/extensions/api/debugger.h"

using extensions::api::debugger::Debuggee;

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

  void FormatErrorMessage(const std::string& format);

  bool InitWebContents();
  bool InitClientHost();

  content::WebContents* contents_;
  Debuggee debuggee_;
  ExtensionDevToolsClientHost* client_host_;
};

// Implements the debugger.attach() extension function.
class DebuggerAttachFunction : public DebuggerFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("debugger.attach", DEBUGGER_ATTACH)

  DebuggerAttachFunction();

 protected:
  virtual ~DebuggerAttachFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

// Implements the debugger.detach() extension function.
class DebuggerDetachFunction : public DebuggerFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("debugger.detach", DEBUGGER_DETACH)

  DebuggerDetachFunction();

 protected:
  virtual ~DebuggerDetachFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

// Implements the debugger.sendCommand() extension function.
class DebuggerSendCommandFunction : public DebuggerFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("debugger.sendCommand", DEBUGGER_SENDCOMMAND)

  DebuggerSendCommandFunction();
  void SendResponseBody(base::DictionaryValue* result);

 protected:
  virtual ~DebuggerSendCommandFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

// Implements the debugger.getTargets() extension function.
class DebuggerGetTargetsFunction : public DebuggerFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("debugger.getTargets", DEBUGGER_ATTACH)

  DebuggerGetTargetsFunction();

 protected:
  virtual ~DebuggerGetTargetsFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_DEBUGGER_DEBUGGER_API_H_
