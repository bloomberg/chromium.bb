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
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("browserAction.setIcon")

 protected:
  virtual ~BrowserActionSetIconFunction() {}
};

class BrowserActionSetTitleFunction : public ExtensionActionSetTitleFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("browserAction.setTitle")

 protected:
  virtual ~BrowserActionSetTitleFunction() {}
};

class BrowserActionSetPopupFunction : public ExtensionActionSetPopupFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("browserAction.setPopup")

 protected:
  virtual ~BrowserActionSetPopupFunction() {}
};

class BrowserActionGetTitleFunction : public ExtensionActionGetTitleFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("browserAction.getTitle")

 protected:
  virtual ~BrowserActionGetTitleFunction() {}
};

class BrowserActionGetPopupFunction : public ExtensionActionGetPopupFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("browserAction.getPopup")

 protected:
  virtual ~BrowserActionGetPopupFunction() {}
};

class BrowserActionSetBadgeTextFunction
    : public ExtensionActionSetBadgeTextFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("browserAction.setBadgeText")

 protected:
  virtual ~BrowserActionSetBadgeTextFunction() {}
};

class BrowserActionSetBadgeBackgroundColorFunction
    : public ExtensionActionSetBadgeBackgroundColorFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("browserAction.setBadgeBackgroundColor")

 protected:
  virtual ~BrowserActionSetBadgeBackgroundColorFunction() {}
};

class BrowserActionGetBadgeTextFunction
    : public ExtensionActionGetBadgeTextFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("browserAction.getBadgeText")

 protected:
  virtual ~BrowserActionGetBadgeTextFunction() {}
};

class BrowserActionGetBadgeBackgroundColorFunction
    : public ExtensionActionGetBadgeBackgroundColorFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("browserAction.getBadgeBackgroundColor")

 protected:
  virtual ~BrowserActionGetBadgeBackgroundColorFunction() {}
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_EXTENSION_BROWSER_ACTIONS_API_H_
