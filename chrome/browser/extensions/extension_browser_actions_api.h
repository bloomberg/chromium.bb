// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSER_ACTIONS_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSER_ACTIONS_API_H_

#include "chrome/browser/extensions/extension_function.h"

class BrowserActionSetNameFunction : public SyncExtensionFunction {
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("browserAction.setName")
};

class BrowserActionSetIconFunction : public SyncExtensionFunction {
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("browserAction.setIcon")
};

class BrowserActionSetBadgeTextFunction : public SyncExtensionFunction {
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("browserAction.setBadgeText")
};

class BrowserActionSetBadgeBackgroundColorFunction
    : public SyncExtensionFunction {
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("browserAction.setBadgeBackgroundColor")
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSER_ACTIONS_API_H_
