// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_EXTENSION_PAGE_ACTIONS_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_EXTENSION_PAGE_ACTIONS_API_H_
#pragma once

#include "chrome/browser/extensions/api/extension_action/extension_actions_api.h"
#include "chrome/browser/extensions/extension_function.h"

class TabContentsWrapper;

// Base class for page action APIs.
class PageActionFunction : public ExtensionActionFunction {
 protected:
  virtual ~PageActionFunction() {}
  bool SetPageActionEnabled(bool enable);
  bool SetVisible(bool visible);
  virtual bool RunImpl() OVERRIDE;

  TabContentsWrapper* contents_;
};

// Implement chrome.pageActions.enableForTab().
class EnablePageActionFunction : public PageActionFunction {
  virtual ~EnablePageActionFunction() {}
  virtual bool RunExtensionAction() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("pageActions.enableForTab")
};

// Implement chrome.pageActions.disableForTab().
class DisablePageActionFunction : public PageActionFunction {
  virtual ~DisablePageActionFunction() {}
  virtual bool RunExtensionAction() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("pageActions.disableForTab")
};

// Implement chrome.pageActions.show().
class PageActionShowFunction : public PageActionFunction {
  virtual ~PageActionShowFunction() {}
  virtual bool RunExtensionAction() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("pageAction.show")
};

// Implement chrome.pageActions.hide().
class PageActionHideFunction : public PageActionFunction {
  virtual ~PageActionHideFunction() {}
  virtual bool RunExtensionAction() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("pageAction.hide")
};

// Implement chrome.pageActions.setIcon().
class PageActionSetIconFunction : public PageActionFunction {
  virtual ~PageActionSetIconFunction() {}
  virtual bool RunExtensionAction() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("pageAction.setIcon")
};

// Implement chrome.pageActions.setTitle().
class PageActionSetTitleFunction : public PageActionFunction {
  virtual ~PageActionSetTitleFunction() {}
  virtual bool RunExtensionAction() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("pageAction.setTitle")
};

// Implement chrome.pageActions.setPopup().
class PageActionSetPopupFunction : public PageActionFunction {
  virtual ~PageActionSetPopupFunction() {}
  virtual bool RunExtensionAction() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("pageAction.setPopup")
};

// Implement chrome.pageActions.setBadgeBackgroundColor().
class PageActionSetBadgeBackgroundColorFunction : public PageActionFunction {
  virtual ~PageActionSetBadgeBackgroundColorFunction() {}
  virtual bool RunExtensionAction() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("pageAction.setBadgeBackgroundColor")
};

// Implement chrome.pageActions.setBadgeTextColor().
class PageActionSetBadgeTextColorFunction : public PageActionFunction {
  virtual ~PageActionSetBadgeTextColorFunction() {}
  virtual bool RunExtensionAction() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("pageAction.setBadgeTextColor")
};

// Implement chrome.pageActions.setBadgeText().
class PageActionSetBadgeTextFunction : public PageActionFunction {
  virtual ~PageActionSetBadgeTextFunction() {}
  virtual bool RunExtensionAction() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("pageAction.setBadgeText")
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_EXTENSION_PAGE_ACTIONS_API_H_
