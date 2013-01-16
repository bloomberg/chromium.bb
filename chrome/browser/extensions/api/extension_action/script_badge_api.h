// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_SCRIPT_BADGE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_SCRIPT_BADGE_API_H_

#include "chrome/browser/extensions/api/extension_action/extension_actions_api.h"
#include "chrome/browser/extensions/api/profile_keyed_api_factory.h"
#include "chrome/browser/extensions/extension_function.h"

namespace extensions {

class ScriptBadgeAPI : public ProfileKeyedAPI {
 public:
  explicit ScriptBadgeAPI(Profile* profile);
  virtual ~ScriptBadgeAPI();

  // ProfileKeyedAPI implementation.
  static ProfileKeyedAPIFactory<ScriptBadgeAPI>* GetFactoryInstance();

 private:
  friend class ProfileKeyedAPIFactory<ScriptBadgeAPI>;

  // ProfileKeyedAPI implementation.
  static const char* service_name() { return "ScriptBadgeAPI"; }

  DISALLOW_COPY_AND_ASSIGN(ScriptBadgeAPI);
};

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

// scriptBadge.getAttention(tabId)
class ScriptBadgeGetAttentionFunction : public ExtensionActionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("scriptBadge.getAttention")

  virtual bool RunExtensionAction() OVERRIDE;

 protected:
  virtual ~ScriptBadgeGetAttentionFunction();
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_SCRIPT_BADGE_API_H_
