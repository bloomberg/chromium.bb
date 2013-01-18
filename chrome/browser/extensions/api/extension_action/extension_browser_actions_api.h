// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_EXTENSION_BROWSER_ACTIONS_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_EXTENSION_BROWSER_ACTIONS_API_H_

#include "chrome/browser/extensions/api/extension_action/extension_actions_api.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_function.h"

//
// browserAction.* aliases for supported browserActions APIs.
//

class BrowserActionSetIconFunction : public ExtensionActionSetIconFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.setIcon", BROWSERACTION_SETICON)

 protected:
  virtual ~BrowserActionSetIconFunction() {}
};

class BrowserActionSetTitleFunction : public ExtensionActionSetTitleFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.setTitle", BROWSERACTION_SETTITLE)

 protected:
  virtual ~BrowserActionSetTitleFunction() {}
};

class BrowserActionSetPopupFunction : public ExtensionActionSetPopupFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.setPopup", BROWSERACTION_SETPOPUP)

 protected:
  virtual ~BrowserActionSetPopupFunction() {}
};

class BrowserActionGetTitleFunction : public ExtensionActionGetTitleFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.getTitle", BROWSERACTION_GETTITLE)

 protected:
  virtual ~BrowserActionGetTitleFunction() {}
};

class BrowserActionGetPopupFunction : public ExtensionActionGetPopupFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.getPopup", BROWSERACTION_GETPOPUP)

 protected:
  virtual ~BrowserActionGetPopupFunction() {}
};

class BrowserActionSetBadgeTextFunction
    : public ExtensionActionSetBadgeTextFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.setBadgeText",
                             BROWSERACTION_SETBADGETEXT)

 protected:
  virtual ~BrowserActionSetBadgeTextFunction() {}
};

class BrowserActionSetBadgeBackgroundColorFunction
    : public ExtensionActionSetBadgeBackgroundColorFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.setBadgeBackgroundColor",
                             BROWSERACTION_SETBADGEBACKGROUNDCOLOR)

 protected:
  virtual ~BrowserActionSetBadgeBackgroundColorFunction() {}
};

class BrowserActionGetBadgeTextFunction
    : public ExtensionActionGetBadgeTextFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.getBadgeText",
                             BROWSERACTION_GETBADGETEXT)

 protected:
  virtual ~BrowserActionGetBadgeTextFunction() {}
};

class BrowserActionGetBadgeBackgroundColorFunction
    : public ExtensionActionGetBadgeBackgroundColorFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.getBadgeBackgroundColor",
                             BROWSERACTION_GETBADGEBACKGROUNDCOLOR)

 protected:
  virtual ~BrowserActionGetBadgeBackgroundColorFunction() {}
};

class BrowserActionEnableFunction : public ExtensionActionShowFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.enable", BROWSERACTION_ENABLE)

 protected:
  virtual ~BrowserActionEnableFunction() {}
};

class BrowserActionDisableFunction : public ExtensionActionHideFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.disable", BROWSERACTION_DISABLE)

 protected:
  virtual ~BrowserActionDisableFunction() {}
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_EXTENSION_BROWSER_ACTIONS_API_H_
