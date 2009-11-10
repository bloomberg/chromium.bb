// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSER_ACTIONS_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSER_ACTIONS_API_H_

#include "chrome/browser/extensions/extension_function.h"
#include "chrome/common/extensions/extension_action.h"

class ExtensionAction;
class ExtensionActionState;

class BrowserActionFunction : public SyncExtensionFunction {
 protected:
  BrowserActionFunction() : tab_id_(ExtensionAction::kDefaultTabId) {}
  virtual ~BrowserActionFunction() {}
  virtual bool RunImpl();
  virtual bool RunBrowserAction() = 0;

  // All the browser action APIs take a single argument called details that is
  // a dictionary.
  const DictionaryValue* details_;

  // The tab id the browser action function should apply to, if any, or
  // kDefaultTabId if none was specified.
  int tab_id_;

  // The browser action for the current extension.
  ExtensionAction* browser_action_;
};

class BrowserActionSetIconFunction : public BrowserActionFunction {
  ~BrowserActionSetIconFunction() {}
  virtual bool RunBrowserAction();
  DECLARE_EXTENSION_FUNCTION_NAME("browserAction.setIcon")
};

class BrowserActionSetTitleFunction : public BrowserActionFunction {
  ~BrowserActionSetTitleFunction() {}
  virtual bool RunBrowserAction();
  DECLARE_EXTENSION_FUNCTION_NAME("browserAction.setTitle")
};

class BrowserActionSetBadgeTextFunction : public BrowserActionFunction {
  ~BrowserActionSetBadgeTextFunction() {}
  virtual bool RunBrowserAction();
  DECLARE_EXTENSION_FUNCTION_NAME("browserAction.setBadgeText")
};

class BrowserActionSetBadgeBackgroundColorFunction
    : public BrowserActionFunction {
  ~BrowserActionSetBadgeBackgroundColorFunction() {}
  virtual bool RunBrowserAction();
  DECLARE_EXTENSION_FUNCTION_NAME("browserAction.setBadgeBackgroundColor")
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSER_ACTIONS_API_H_
