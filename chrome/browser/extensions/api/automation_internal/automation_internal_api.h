// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_AUTOMATION_INTERNAL_AUTOMATION_INTERNAL_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_AUTOMATION_INTERNAL_AUTOMATION_INTERNAL_API_H_

#include "chrome/browser/extensions/chrome_extension_function.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
struct AXEventNotificationDetails;
}  // namespace content

namespace ui {
struct AXNodeData;
}

namespace extensions {

// Implementation of the chrome.automation API.
class AutomationInternalEnableCurrentTabFunction
    : public ChromeUIThreadExtensionFunction {
  DECLARE_EXTENSION_FUNCTION("automationInternal.enableCurrentTab",
                             AUTOMATIONINTERNAL_ENABLECURRENTTAB)
 protected:
  virtual ~AutomationInternalEnableCurrentTabFunction() {}

  virtual ExtensionFunction::ResponseAction Run() OVERRIDE;
};

class AutomationInternalPerformActionFunction
    : public UIThreadExtensionFunction {
  DECLARE_EXTENSION_FUNCTION("automationInternal.performAction",
                             AUTOMATIONINTERNAL_PERFORMACTION)
 protected:
  virtual ~AutomationInternalPerformActionFunction() {}

  virtual ExtensionFunction::ResponseAction Run() OVERRIDE;
};

class AutomationInternalEnableDesktopFunction
    : public UIThreadExtensionFunction {
  DECLARE_EXTENSION_FUNCTION("automationInternal.enableDesktop",
                             AUTOMATIONINTERNAL_ENABLEDESKTOP)
 protected:
  virtual ~AutomationInternalEnableDesktopFunction() {}

  virtual ResponseAction Run() OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_AUTOMATION_INTERNAL_AUTOMATION_INTERNAL_API_H_
