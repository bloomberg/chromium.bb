// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_INPUT_INPUT_H_
#define CHROME_BROWSER_EXTENSIONS_API_INPUT_INPUT_H_

#include "base/compiler_specific.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_function.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class VirtualKeyboardPrivateInsertTextFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("virtualKeyboardPrivate.insertText",
                             VIRTUALKEYBOARDPRIVATE_INSERTTEXT);

 protected:
  ~VirtualKeyboardPrivateInsertTextFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class VirtualKeyboardPrivateMoveCursorFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("virtualKeyboardPrivate.moveCursor",
                             VIRTUALKEYBOARDPRIVATE_MOVECURSOR);

 protected:
  ~VirtualKeyboardPrivateMoveCursorFunction() override {}

  // ExtensionFunction.
  bool RunSync() override;
};

class VirtualKeyboardPrivateSendKeyEventFunction
    : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "virtualKeyboardPrivate.sendKeyEvent",
      VIRTUALKEYBOARDPRIVATE_SENDKEYEVENT);

 protected:
  ~VirtualKeyboardPrivateSendKeyEventFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class VirtualKeyboardPrivateHideKeyboardFunction
    : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "virtualKeyboardPrivate.hideKeyboard",
      VIRTUALKEYBOARDPRIVATE_HIDEKEYBOARD);

 protected:
  ~VirtualKeyboardPrivateHideKeyboardFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class VirtualKeyboardPrivateLockKeyboardFunction
    : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "virtualKeyboardPrivate.lockKeyboard",
      VIRTUALKEYBOARDPRIVATE_LOCKKEYBOARD);

 protected:
  ~VirtualKeyboardPrivateLockKeyboardFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class VirtualKeyboardPrivateKeyboardLoadedFunction
    : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "virtualKeyboardPrivate.keyboardLoaded",
      VIRTUALKEYBOARDPRIVATE_KEYBOARDLOADED);

 protected:
  ~VirtualKeyboardPrivateKeyboardLoadedFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class VirtualKeyboardPrivateGetKeyboardConfigFunction
    : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "virtualKeyboardPrivate.getKeyboardConfig",
      VIRTUALKEYBOARDPRIVATE_GETKEYBOARDCONFIG);

 protected:
  ~VirtualKeyboardPrivateGetKeyboardConfigFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class VirtualKeyboardPrivateOpenSettingsFunction
    : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("virtualKeyboardPrivate.openSettings",
                             VIRTUALKEYBOARDPRIVATE_OPENSETTINGS);

 protected:
  ~VirtualKeyboardPrivateOpenSettingsFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};


class InputAPI : public BrowserContextKeyedAPI {
 public:
  explicit InputAPI(content::BrowserContext* context);
  ~InputAPI() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<InputAPI>* GetFactoryInstance();

 private:
  friend class BrowserContextKeyedAPIFactory<InputAPI>;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() {
    return "InputAPI";
  }
  static const bool kServiceIsNULLWhileTesting = true;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_INPUT_INPUT_H_
