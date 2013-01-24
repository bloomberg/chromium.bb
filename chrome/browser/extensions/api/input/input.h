// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_INPUT_INPUT_H_
#define CHROME_BROWSER_EXTENSIONS_API_INPUT_INPUT_H_

#include "base/compiler_specific.h"
#include "chrome/browser/extensions/api/profile_keyed_api_factory.h"
#include "chrome/browser/extensions/extension_function.h"

class Profile;

namespace extensions {

// Note that this experimental API is currently only available for
// TOOLKIT_VIEWS (see chrome/chrome_browser.gypi).
//
// We may eventually support other platforms by adding the necessary
// synthetic event distribution code to this Function.
class SendKeyboardEventInputFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "experimental.input.virtualKeyboard.sendKeyboardEvent",
      EXPERIMENTAL_INPUT_VIRTUALKEYBOARD_SENDKEYBOARDEVENT);

 protected:
  virtual ~SendKeyboardEventInputFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class InputAPI : public ProfileKeyedAPI {
 public:
  explicit InputAPI(Profile* profile);
  virtual ~InputAPI();

  // ProfileKeyedAPI implementation.
  static ProfileKeyedAPIFactory<InputAPI>* GetFactoryInstance();

 private:
  friend class ProfileKeyedAPIFactory<InputAPI>;

  // ProfileKeyedAPI implementation.
  static const char* service_name() {
    return "InputAPI";
  }
  static const bool kServiceIsNULLWhileTesting = true;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_INPUT_INPUT_H_
