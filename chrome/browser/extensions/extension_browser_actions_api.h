// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSER_ACTIONS_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSER_ACTIONS_API_H_
#pragma once

#include "chrome/browser/extensions/extension_function.h"
#include "chrome/common/extensions/extension_action.h"

class ExtensionAction;

namespace base {
class DictionaryValue;
}

// Base class for chrome.browserAction.* APIs.
class BrowserActionFunction : public SyncExtensionFunction {
 protected:
  BrowserActionFunction()
      : details_(NULL),
        tab_id_(ExtensionAction::kDefaultTabId),
        browser_action_(NULL) {}
  virtual ~BrowserActionFunction() {}
  virtual bool RunImpl() OVERRIDE;
  virtual bool RunBrowserAction() = 0;

  // All the browser action APIs take a single argument called details that is
  // a dictionary.
  base::DictionaryValue* details_;

  // The tab id the browser action function should apply to, if any, or
  // kDefaultTabId if none was specified.
  int tab_id_;

  // The browser action for the current extension.
  ExtensionAction* browser_action_;
};

// Implement chrome.browserAction.setIcon().
class BrowserActionSetIconFunction : public BrowserActionFunction {
  virtual ~BrowserActionSetIconFunction() {}
  virtual bool RunBrowserAction() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("browserAction.setIcon")
};

// Implement chrome.browserAction.setTitle().
class BrowserActionSetTitleFunction : public BrowserActionFunction {
  virtual ~BrowserActionSetTitleFunction() {}
  virtual bool RunBrowserAction() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("browserAction.setTitle")
};

// Implement chrome.browserActions.setPopup().
class BrowserActionSetPopupFunction : public BrowserActionFunction {
  virtual ~BrowserActionSetPopupFunction() {}
  virtual bool RunBrowserAction() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("browserAction.setPopup")
};

// Implement chrome.browserAction.setBadgeText().
class BrowserActionSetBadgeTextFunction : public BrowserActionFunction {
  virtual ~BrowserActionSetBadgeTextFunction() {}
  virtual bool RunBrowserAction() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("browserAction.setBadgeText")
};

// Implement chrome.browserAction.setBadgeBackgroundColor().
class BrowserActionSetBadgeBackgroundColorFunction
    : public BrowserActionFunction {
  virtual ~BrowserActionSetBadgeBackgroundColorFunction() {}
  virtual bool RunBrowserAction() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("browserAction.setBadgeBackgroundColor")
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSER_ACTIONS_API_H_
