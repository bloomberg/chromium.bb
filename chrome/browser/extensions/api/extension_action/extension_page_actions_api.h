// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_EXTENSION_PAGE_ACTIONS_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_EXTENSION_PAGE_ACTIONS_API_H_

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
 public:
  DECLARE_EXTENSION_FUNCTION("pageActions.enableForTab",
                             PAGEACTIONS_ENABLEFORTAB)

 protected:
  virtual ~EnablePageActionsFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

// Implement chrome.pageActions.disableForTab().
class DisablePageActionsFunction : public PageActionsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("pageActions.disableForTab",
                             PAGEACTIONS_DISABLEFORTAB)

 protected:
  virtual ~DisablePageActionsFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

//
// pageAction.* aliases for supported extensionActions APIs.
//

class PageActionShowFunction : public ExtensionActionShowFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("pageAction.show", PAGEACTION_SHOW)

 protected:
  virtual ~PageActionShowFunction() {}
};

class PageActionHideFunction : public ExtensionActionHideFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("pageAction.hide", PAGEACTION_HIDE)

 protected:
  virtual ~PageActionHideFunction() {}
};

class PageActionSetIconFunction : public ExtensionActionSetIconFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("pageAction.setIcon", PAGEACTION_SETICON)

 protected:
  virtual ~PageActionSetIconFunction() {}
};

class PageActionSetTitleFunction : public ExtensionActionSetTitleFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("pageAction.setTitle", PAGEACTION_SETTITLE)

 protected:
  virtual ~PageActionSetTitleFunction() {}
};

class PageActionSetPopupFunction : public ExtensionActionSetPopupFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("pageAction.setPopup", PAGEACTION_SETPOPUP)

 protected:
  virtual ~PageActionSetPopupFunction() {}
};

class PageActionGetTitleFunction : public ExtensionActionGetTitleFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("pageAction.getTitle", PAGEACTION_GETTITLE)

 protected:
  virtual ~PageActionGetTitleFunction() {}
};

class PageActionGetPopupFunction : public ExtensionActionGetPopupFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("pageAction.getPopup", PAGEACTION_GETPOPUP)

 protected:
  virtual ~PageActionGetPopupFunction() {}
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_EXTENSION_PAGE_ACTIONS_API_H_
