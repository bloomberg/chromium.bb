// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_EXTENSION_BROWSER_ACTIONS_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_EXTENSION_BROWSER_ACTIONS_API_H_
#pragma once

#include "chrome/browser/extensions/api/extension_action/extension_actions_api.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/common/extensions/extension_action.h"

// Base class for chrome.browserAction.* APIs.
class BrowserActionFunction : public ExtensionActionFunction {
 protected:
  virtual ~BrowserActionFunction() {}
  virtual bool RunImpl() OVERRIDE;
  void FireUpdateNotification();
};

// Implement chrome.browserAction.setIcon().
class BrowserActionSetIconFunction : public BrowserActionFunction {
  virtual ~BrowserActionSetIconFunction() {}
  virtual bool RunExtensionAction() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("browserAction.setIcon")
};

// Implement chrome.browserAction.setTitle().
class BrowserActionSetTitleFunction : public BrowserActionFunction {
  virtual ~BrowserActionSetTitleFunction() {}
  virtual bool RunExtensionAction() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("browserAction.setTitle")
};

// Implement chrome.browserAction.setPopup().
class BrowserActionSetPopupFunction : public BrowserActionFunction {
  virtual ~BrowserActionSetPopupFunction() {}
  virtual bool RunExtensionAction() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("browserAction.setPopup")
};

// Implement chrome.browserAction.setBadgeText().
class BrowserActionSetBadgeTextFunction : public BrowserActionFunction {
  virtual ~BrowserActionSetBadgeTextFunction() {}
  virtual bool RunExtensionAction() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("browserAction.setBadgeText")
};

// Implement chrome.browserAction.setBadgeBackgroundColor().
class BrowserActionSetBadgeBackgroundColorFunction
    : public BrowserActionFunction {
  virtual ~BrowserActionSetBadgeBackgroundColorFunction() {}
  virtual bool RunExtensionAction() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("browserAction.setBadgeBackgroundColor")
};

// Implement chrome.browserAction.getTitle().
class BrowserActionGetTitleFunction : public BrowserActionFunction {
  virtual ~BrowserActionGetTitleFunction() {}
  virtual bool RunExtensionAction() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("browserAction.getTitle")
};

// Implement chrome.browserAction.getPopup().
class BrowserActionGetPopupFunction : public BrowserActionFunction {
  virtual ~BrowserActionGetPopupFunction() {}
  virtual bool RunExtensionAction() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("browserAction.getPopup")
};

// Implement chrome.browserAction.getBadgeText().
class BrowserActionGetBadgeTextFunction : public BrowserActionFunction {
  virtual ~BrowserActionGetBadgeTextFunction() {}
  virtual bool RunExtensionAction() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("browserAction.getBadgeText")
};

// Implement chrome.browserAction.getBadgeBackgroundColor().
class BrowserActionGetBadgeBackgroundColorFunction
    : public BrowserActionFunction {
  virtual ~BrowserActionGetBadgeBackgroundColorFunction() {}
  virtual bool RunExtensionAction() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("browserAction.getBadgeBackgroundColor")
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_EXTENSION_BROWSER_ACTIONS_API_H_
