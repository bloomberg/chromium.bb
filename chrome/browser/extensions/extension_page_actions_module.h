// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_PAGE_ACTIONS_MODULE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_PAGE_ACTIONS_MODULE_H_

#include "chrome/browser/extensions/extension_function.h"

class TabContents;
class ExtensionAction2;

class PageActionFunction : public SyncExtensionFunction {
 protected:
  bool SetPageActionEnabled(bool enable);

  bool InitCommon(int tab_id);
  bool SetVisible(bool visible);

  ExtensionAction2* page_action_;
  TabContents* contents_;
};

class EnablePageActionFunction : public PageActionFunction {
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("pageActions.enableForTab")
};

class DisablePageActionFunction : public PageActionFunction {
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("pageActions.disableForTab")
};

class PageActionShowFunction : public PageActionFunction {
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("pageAction.show")
};

class PageActionHideFunction : public PageActionFunction {
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("pageAction.hide")
};

class PageActionSetIconFunction : public PageActionFunction {
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("pageAction.setIcon")
};

class PageActionSetTitleFunction : public PageActionFunction {
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("pageAction.setTitle")
};

class PageActionSetBadgeBackgroundColorFunction : public PageActionFunction {
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("pageAction.setBadgeBackgroundColor")
};

class PageActionSetBadgeTextColorFunction : public PageActionFunction {
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("pageAction.setBadgeTextColor")
};

class PageActionSetBadgeTextFunction : public PageActionFunction {
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("pageAction.setBadgeText")
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_PAGE_ACTIONS_MODULE_H_
