// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_EXTENSION_SCRIPT_BADGE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_EXTENSION_SCRIPT_BADGE_API_H_
#pragma once

#include "chrome/browser/extensions/api/extension_action/extension_actions_api.h"
#include "chrome/browser/extensions/extension_function.h"

//
// scriptBadge.* aliases for supported scriptBadge APIs.
//

class ScriptBadgeSetPopupFunction : public ExtensionActionSetPopupFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("scriptBadge.setPopup")

 protected:
  virtual ~ScriptBadgeSetPopupFunction() {}
};

class ScriptBadgeGetPopupFunction : public ExtensionActionGetPopupFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("scriptBadge.getPopup")

 protected:
  virtual ~ScriptBadgeGetPopupFunction() {}
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_EXTENSION_SCRIPT_BADGE_API_H_
