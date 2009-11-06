// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines AutomationExtensionFunction.

#ifndef CHROME_BROWSER_AUTOMATION_AUTOMATION_EXTENSION_FUNCTION_H_
#define CHROME_BROWSER_AUTOMATION_AUTOMATION_EXTENSION_FUNCTION_H_

#include <string>

#include "chrome/browser/extensions/extension_function.h"

class RenderViewHost;

// An extension function that pipes the extension API call through the
// automation interface, so that extensions can be tested using UITests.
class AutomationExtensionFunction : public ExtensionFunction {
 public:
  AutomationExtensionFunction() { }

  // ExtensionFunction implementation.
  virtual void SetArgs(const Value* args);
  virtual const std::string GetResult();
  virtual const std::string GetError();
  virtual void Run();

  static ExtensionFunction* Factory();

  // If the list of enabled functions is non-empty, we enable according to the
  // list ("*" means enable all, otherwise we enable individual named
  // functions). If empty, we restore the initial functions.
  //
  // Note that all calls to this function, except a call with the empty list,
  // are additive.  Functions previously enabled will remain enabled until
  // you clear all function forwarding by specifying the empty list.
  static void SetEnabled(const std::vector<std::string>& functions_enabled);

  // Intercepts messages sent from the external host to check if they
  // are actually responses to extension API calls.  If they are, redirects
  // the message to view_host->SendExtensionResponse and returns true,
  // otherwise returns false to indicate the message was not intercepted.
  static bool InterceptMessageFromExternalHost(RenderViewHost* view_host,
                                               const std::string& message,
                                               const std::string& origin,
                                               const std::string& target);

 private:
  static bool enabled_;
  std::string args_;
  DISALLOW_COPY_AND_ASSIGN(AutomationExtensionFunction);
};

#endif  // CHROME_BROWSER_AUTOMATION_AUTOMATION_EXTENSION_FUNCTION_H_
