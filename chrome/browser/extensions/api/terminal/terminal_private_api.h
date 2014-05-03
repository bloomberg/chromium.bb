// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TERMINAL_TERMINAL_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_TERMINAL_TERMINAL_PRIVATE_API_H_

#include <string>

#include "chrome/browser/extensions/chrome_extension_function.h"

namespace extensions {
// Base class for all terminalPrivate function classes. Main purpose is to run
// permission check before calling actual function implementation.
class TerminalPrivateFunction : public ChromeAsyncExtensionFunction {
 public:
  TerminalPrivateFunction();

 protected:
  virtual ~TerminalPrivateFunction();

  // ExtensionFunction:
  virtual bool RunAsync() OVERRIDE;

  // Override with actual extension function implementation.
  virtual bool RunTerminalFunction() = 0;
};

// Opens new terminal process. Returns the new process id.
class TerminalPrivateOpenTerminalProcessFunction
    : public TerminalPrivateFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("terminalPrivate.openTerminalProcess",
                             TERMINALPRIVATE_OPENTERMINALPROCESS)

  TerminalPrivateOpenTerminalProcessFunction();

 protected:
  virtual ~TerminalPrivateOpenTerminalProcessFunction();

  // TerminalPrivateFunction:
  virtual bool RunTerminalFunction() OVERRIDE;

 private:
  void OpenOnFileThread();
  void RespondOnUIThread(pid_t pid);

  const char* command_;
};

// Send input to the terminal process specified by the pid sent as an argument.
class TerminalPrivateSendInputFunction : public TerminalPrivateFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("terminalPrivate.sendInput",
                             TERMINALPRIVATE_SENDINPUT)

 protected:
  virtual ~TerminalPrivateSendInputFunction();

  // TerminalPrivateFunction:
  virtual bool RunTerminalFunction() OVERRIDE;

 private:
  void SendInputOnFileThread(pid_t pid, const std::string& input);
  void RespondOnUIThread(bool success);
};

// Closes terminal process with given pid.
class TerminalPrivateCloseTerminalProcessFunction
    : public TerminalPrivateFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("terminalPrivate.closeTerminalProcess",
                             TERMINALPRIVATE_CLOSETERMINALPROCESS)

 protected:
  virtual ~TerminalPrivateCloseTerminalProcessFunction();

  virtual bool RunTerminalFunction() OVERRIDE;

 private:
  void CloseOnFileThread(pid_t pid);
  void RespondOnUIThread(bool success);
};

// Called by extension when terminal size changes.
class TerminalPrivateOnTerminalResizeFunction : public TerminalPrivateFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("terminalPrivate.onTerminalResize",
                             TERMINALPRIVATE_ONTERMINALRESIZE)

 protected:
  virtual ~TerminalPrivateOnTerminalResizeFunction();

  virtual bool RunTerminalFunction() OVERRIDE;

 private:
  void OnResizeOnFileThread(pid_t pid, int width, int height);
  void RespondOnUIThread(bool success);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TERMINAL_TERMINAL_PRIVATE_API_H_
