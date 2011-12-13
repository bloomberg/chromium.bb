// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TERMINAL_TERMINAL_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_TERMINAL_TERMINAL_PRIVATE_API_H_
#pragma once

#include <string>

#include "chrome/browser/extensions/extension_function.h"

// Opens new terminal process. Returns the new process id.
class OpenTerminalProcessFunction : public AsyncExtensionFunction {
 public:
  OpenTerminalProcessFunction();
  virtual ~OpenTerminalProcessFunction();

  virtual bool RunImpl() OVERRIDE;

 private:
  void OpenOnFileThread();
  void RespondOnUIThread(pid_t pid);

  const char* command_;

  DECLARE_EXTENSION_FUNCTION_NAME("terminalPrivate.openTerminalProcess")
};

// Send input to the terminal process specified by the pid sent as an argument.
class SendInputToTerminalProcessFunction : public AsyncExtensionFunction {
 public:
  virtual bool RunImpl() OVERRIDE;

 private:
  void SendInputOnFileThread(pid_t pid, const std::string& input);
  void RespondOnUIThread(bool success);

  DECLARE_EXTENSION_FUNCTION_NAME("terminalPrivate.sendInput")
};

// Closes terminal process with given pid.
class CloseTerminalProcessFunction : public AsyncExtensionFunction {
 public:
  virtual bool RunImpl() OVERRIDE;

 private:
  void CloseOnFileThread(pid_t pid);
  void RespondOnUIThread(bool success);

  DECLARE_EXTENSION_FUNCTION_NAME("terminalPrivate.closeTerminalProcess")
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_TERMINAL_TERMINAL_PRIVATE_API_H_
