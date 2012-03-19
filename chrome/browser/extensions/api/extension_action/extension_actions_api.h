// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_EXTENSION_ACTIONS_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_EXTENSION_ACTIONS_API_H_
#pragma once

#include "chrome/browser/extensions/extension_function.h"
#include "chrome/common/extensions/extension_action.h"

namespace base {
class DictionaryValue;
}

// Base class for chrome.(browserAction|pageAction).* APIs.
class ExtensionActionFunction : public SyncExtensionFunction {
 public:
  static bool ParseCSSColorString(const std::string& color_string,
                                  SkColor* result);

 protected:
  ExtensionActionFunction();
  virtual ~ExtensionActionFunction();
  virtual bool RunImpl() OVERRIDE;
  virtual bool RunExtensionAction() = 0;
  bool SetIcon();
  bool SetTitle();
  bool SetPopup();
  bool SetBadgeBackgroundColor();
  bool SetBadgeText();
  bool GetTitle();
  bool GetPopup();
  bool GetBadgeBackgroundColor();
  bool GetBadgeText();

  // All the browser action APIs take a single argument called details that is
  // a dictionary.
  base::DictionaryValue* details_;

  // The tab id the extension action function should apply to, if any, or
  // kDefaultTabId if none was specified.
  int tab_id_;

  // The extension action for the current extension.
  ExtensionAction* extension_action_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_EXTENSION_ACTIONS_API_H_
