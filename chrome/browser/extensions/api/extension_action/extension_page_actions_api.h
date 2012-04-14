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
  bool SetVisible(bool visible);
  virtual bool RunImpl() OVERRIDE;

  TabContentsWrapper* contents_;
};

// Base class for deprecated page actions APIs
class PageActionsFunction : public SyncExtensionFunction {
 protected:
  PageActionsFunction();
  virtual ~PageActionsFunction();
  bool SetPageActionEnabled(bool enable);
};

// Implement chrome.pageActions.enableForTab().
class EnablePageActionsFunction : public PageActionsFunction {
  virtual ~EnablePageActionsFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("pageActions.enableForTab")
};

// Implement chrome.pageActions.disableForTab().
class DisablePageActionsFunction : public PageActionsFunction {
  virtual ~DisablePageActionsFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("pageActions.disableForTab")
};

// Implement chrome.pageAction.show().
class PageActionShowFunction : public PageActionFunction {
  virtual ~PageActionShowFunction() {}
  virtual bool RunExtensionAction() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("pageAction.show")
};

// Implement chrome.pageAction.hide().
class PageActionHideFunction : public PageActionFunction {
  virtual ~PageActionHideFunction() {}
  virtual bool RunExtensionAction() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("pageAction.hide")
};

// Implement chrome.pageAction.setIcon().
class PageActionSetIconFunction : public PageActionFunction {
  virtual ~PageActionSetIconFunction() {}
  virtual bool RunExtensionAction() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("pageAction.setIcon")
};

// Implement chrome.pageAction.setTitle().
class PageActionSetTitleFunction : public PageActionFunction {
  virtual ~PageActionSetTitleFunction() {}
  virtual bool RunExtensionAction() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("pageAction.setTitle")
};

// Implement chrome.pageAction.setPopup().
class PageActionSetPopupFunction : public PageActionFunction {
  virtual ~PageActionSetPopupFunction() {}
  virtual bool RunExtensionAction() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("pageAction.setPopup")
};

// Implement chrome.pageAction.setBadgeBackgroundColor().
class PageActionSetBadgeBackgroundColorFunction : public PageActionFunction {
  virtual ~PageActionSetBadgeBackgroundColorFunction() {}
  virtual bool RunExtensionAction() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("pageAction.setBadgeBackgroundColor")
};

// Implement chrome.pageAction.setBadgeTextColor().
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

// Implement chrome.pageAction.getTitle().
class PageActionGetTitleFunction : public PageActionFunction {
  virtual ~PageActionGetTitleFunction() {}
  virtual bool RunExtensionAction() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("pageAction.getTitle")
};

// Implement chrome.pageAction.getPopup().
class PageActionGetPopupFunction : public PageActionFunction {
  virtual ~PageActionGetPopupFunction() {}
  virtual bool RunExtensionAction() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("pageAction.getPopup")
};

// Implement chrome.pageAction.getBadgeText().
class PageActionGetBadgeTextFunction : public PageActionFunction {
  virtual ~PageActionGetBadgeTextFunction() {}
  virtual bool RunExtensionAction() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("pageAction.getBadgeText")
};

// Implement chrome.pageAction.getBadgeBackgroundColor().
class PageActionGetBadgeBackgroundColorFunction : public PageActionFunction {
  virtual ~PageActionGetBadgeBackgroundColorFunction() {}
  virtual bool RunExtensionAction() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("pageAction.getBadgeBackgroundColor")
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_EXTENSION_PAGE_ACTIONS_API_H_
