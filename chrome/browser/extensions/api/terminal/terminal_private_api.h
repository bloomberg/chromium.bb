// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TERMINAL_TERMINAL_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_TERMINAL_TERMINAL_PRIVATE_API_H_
#pragma once

#include <string>

#include "chrome/browser/extensions/extension_function.h"

// Base class for all terminalPrivate function classes. Main purpose is to run
// permission check before calling actual function implementation.
class TerminalPrivateFunction : public AsyncExtensionFunction {
 public:
  TerminalPrivateFunction();
  virtual ~TerminalPrivateFunction();

  virtual bool RunImpl() OVERRIDE;

  // Override with actual extension function implementation.
  virtual bool RunTerminalFunction() = 0;

};

// Opens new terminal process. Returns the new process id.
class OpenTerminalProcessFunction : public TerminalPrivateFunction {
 public:
  OpenTerminalProcessFunction();
  virtual ~OpenTerminalProcessFunction();

  virtual bool RunTerminalFunction() OVERRIDE;

 private:
  void OpenOnFileThread();
  void RespondOnUIThread(pid_t pid);

  const char* command_;

  DECLARE_EXTENSION_FUNCTION_NAME("terminalPrivate.openTerminalProcess")
};

// Send input to the terminal process specified by the pid sent as an argument.
class SendInputToTerminalProcessFunction : public TerminalPrivateFunction {
 public:
  virtual bool RunTerminalFunction() OVERRIDE;

 private:
  void SendInputOnFileThread(pid_t pid, const std::string& input);
  void RespondOnUIThread(bool success);

  DECLARE_EXTENSION_FUNCTION_NAME("terminalPrivate.sendInput")
};

// Closes terminal process with given pid.
class CloseTerminalProcessFunction : public TerminalPrivateFunction {
 public:
  virtual bool RunTerminalFunction() OVERRIDE;

 private:
  void CloseOnFileThread(pid_t pid);
  void RespondOnUIThread(bool success);

  DECLARE_EXTENSION_FUNCTION_NAME("terminalPrivate.closeTerminalProcess")
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_TERMINAL_TERMINAL_PRIVATE_API_H_
