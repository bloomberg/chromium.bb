// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines AutomationExtensionFunction.

#ifndef CHROME_BROWSER_AUTOMATION_AUTOMATION_EXTENSION_FUNCTION_H_
#define CHROME_BROWSER_AUTOMATION_AUTOMATION_EXTENSION_FUNCTION_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "chrome/browser/extensions/extension_function.h"

class RenderViewHost;
class TabContents;

// An extension function that pipes the extension API call through the
// automation interface, so that extensions can be tested using UITests.
class AutomationExtensionFunction : public AsyncExtensionFunction {
 public:
  AutomationExtensionFunction();

  // ExtensionFunction implementation.
  virtual void SetArgs(const ListValue* args);
  virtual const std::string GetResult();
  virtual bool RunImpl();

  static ExtensionFunction* Factory();

  // Enable API automation of selected APIs.  Overridden extension API messages
  // will be routed to the automation client attached to |api_handler_tab|.
  //
  // If the list of enabled functions is non-empty, we enable according to the
  // list ("*" means enable all, otherwise we enable individual named
  // functions).  An empty list makes this function a no-op.
  //
  // Note that all calls to this function are additive.  Functions previously
  // enabled will remain enabled until you call Disable().
  //
  // Calling this function after enabling one or more functions with a
  // tab other than the one previously used is an error.
  static void Enable(TabContents* api_handler_tab,
                     const std::vector<std::string>& functions_enabled);

  // Restore the default API function implementations and reset the stored
  // API handler.
  static void Disable();

  // Intercepts messages sent from the external host to check if they
  // are actually responses to extension API calls.  If they are, redirects
  // the message to respond to the pending asynchronous API call and returns
  // true, otherwise returns false to indicate the message was not intercepted.
  static bool InterceptMessageFromExternalHost(RenderViewHost* view_host,
                                               const std::string& message,
                                               const std::string& origin,
                                               const std::string& target);

 private:
  ~AutomationExtensionFunction();

  // Weak reference, lifetime managed by the ExternalTabContainer instance
  // owning the TabContents in question.
  static TabContents* api_handler_tab_;

  typedef std::map<int, scoped_refptr<AutomationExtensionFunction> >
      PendingFunctionsMap;
  static PendingFunctionsMap pending_functions_;

  std::string args_;
  std::string json_result_;

  DISALLOW_COPY_AND_ASSIGN(AutomationExtensionFunction);
};

#endif  // CHROME_BROWSER_AUTOMATION_AUTOMATION_EXTENSION_FUNCTION_H_
