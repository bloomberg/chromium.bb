// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSER_ACTIONS_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSER_ACTIONS_API_H_

#include "chrome/browser/extensions/extension_function.h"

class BrowserActionSetNameFunction : public SyncExtensionFunction {
  virtual bool RunImpl();
};

class BrowserActionSetIconFunction : public SyncExtensionFunction {
  virtual bool RunImpl();
};

class BrowserActionSetBadgeTextFunction : public SyncExtensionFunction {
  virtual bool RunImpl();
};

class BrowserActionSetBadgeBackgroundColorFunction
    : public SyncExtensionFunction {
  virtual bool RunImpl();
};

namespace extension_browser_actions_api_constants {

// Function names.
extern const char kSetNameFunction[];
extern const char kSetIconFunction[];
extern const char kSetBadgeTextFunction[];
extern const char kSetBadgeBackgroundColorFunction[];

};  // namespace extension_browser_actions_api_constants

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSER_ACTIONS_API_H_
