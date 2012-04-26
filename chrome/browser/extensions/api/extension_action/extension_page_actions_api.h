// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_EXTENSION_PAGE_ACTIONS_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_EXTENSION_PAGE_ACTIONS_API_H_
#pragma once

#include "chrome/browser/extensions/api/extension_action/extension_actions_api.h"
#include "chrome/browser/extensions/extension_function.h"

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

//
// pageAction.* aliases for supported extensionActions APIs.
//

class PageActionShowFunction : public ExtensionActionShowFunction {
  virtual ~PageActionShowFunction() {}
  DECLARE_EXTENSION_FUNCTION_NAME("pageAction.show")
};

class PageActionHideFunction : public ExtensionActionHideFunction {
  virtual ~PageActionHideFunction() {}
  DECLARE_EXTENSION_FUNCTION_NAME("pageAction.hide")
};

class PageActionSetIconFunction : public ExtensionActionSetIconFunction {
  virtual ~PageActionSetIconFunction() {}
  DECLARE_EXTENSION_FUNCTION_NAME("pageAction.setIcon")
};

class PageActionSetTitleFunction : public ExtensionActionSetTitleFunction {
  virtual ~PageActionSetTitleFunction() {}
  DECLARE_EXTENSION_FUNCTION_NAME("pageAction.setTitle")
};

class PageActionSetPopupFunction : public ExtensionActionSetPopupFunction {
  virtual ~PageActionSetPopupFunction() {}
  DECLARE_EXTENSION_FUNCTION_NAME("pageAction.setPopup")
};

class PageActionGetTitleFunction : public ExtensionActionGetTitleFunction {
  virtual ~PageActionGetTitleFunction() {}
  DECLARE_EXTENSION_FUNCTION_NAME("pageAction.getTitle")
};

class PageActionGetPopupFunction : public ExtensionActionGetPopupFunction {
  virtual ~PageActionGetPopupFunction() {}
  DECLARE_EXTENSION_FUNCTION_NAME("pageAction.getPopup")
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_EXTENSION_PAGE_ACTIONS_API_H_
