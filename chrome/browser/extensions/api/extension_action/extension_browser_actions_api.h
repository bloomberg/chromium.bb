// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_EXTENSION_BROWSER_ACTIONS_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_EXTENSION_BROWSER_ACTIONS_API_H_
#pragma once

#include "chrome/browser/extensions/api/extension_action/extension_actions_api.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/common/extensions/extension_action.h"

//
// browserAction.* aliases for supported browserActions APIs.
//

class BrowserActionSetIconFunction : public ExtensionActionSetIconFunction {
  virtual ~BrowserActionSetIconFunction() {}
  DECLARE_EXTENSION_FUNCTION_NAME("browserAction.setIcon")
};

class BrowserActionSetTitleFunction : public ExtensionActionSetTitleFunction {
  virtual ~BrowserActionSetTitleFunction() {}
  DECLARE_EXTENSION_FUNCTION_NAME("browserAction.setTitle")
};

class BrowserActionSetPopupFunction : public ExtensionActionSetPopupFunction {
  virtual ~BrowserActionSetPopupFunction() {}
  DECLARE_EXTENSION_FUNCTION_NAME("browserAction.setPopup")
};

class BrowserActionGetTitleFunction : public ExtensionActionGetTitleFunction {
  virtual ~BrowserActionGetTitleFunction() {}
  DECLARE_EXTENSION_FUNCTION_NAME("browserAction.getTitle")
};

class BrowserActionGetPopupFunction : public ExtensionActionGetPopupFunction {
  virtual ~BrowserActionGetPopupFunction() {}
  DECLARE_EXTENSION_FUNCTION_NAME("browserAction.getPopup")
};

class BrowserActionSetBadgeTextFunction
    : public ExtensionActionSetBadgeTextFunction {
  virtual ~BrowserActionSetBadgeTextFunction() {}
  DECLARE_EXTENSION_FUNCTION_NAME("browserAction.setBadgeText")
};

class BrowserActionSetBadgeBackgroundColorFunction
    : public ExtensionActionSetBadgeBackgroundColorFunction {
  virtual ~BrowserActionSetBadgeBackgroundColorFunction() {}
  DECLARE_EXTENSION_FUNCTION_NAME("browserAction.setBadgeBackgroundColor")
};

class BrowserActionGetBadgeTextFunction
    : public ExtensionActionGetBadgeTextFunction {
  virtual ~BrowserActionGetBadgeTextFunction() {}
  DECLARE_EXTENSION_FUNCTION_NAME("browserAction.getBadgeText")
};

class BrowserActionGetBadgeBackgroundColorFunction
    : public ExtensionActionGetBadgeBackgroundColorFunction {
  virtual ~BrowserActionGetBadgeBackgroundColorFunction() {}
  DECLARE_EXTENSION_FUNCTION_NAME("browserAction.getBadgeBackgroundColor")
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_EXTENSION_BROWSER_ACTIONS_API_H_
