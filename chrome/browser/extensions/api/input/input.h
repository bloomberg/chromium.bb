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
  virtual ~VirtualKeyboardPrivateInsertTextFunction() {}

  // ExtensionFunction:
  virtual bool RunSync() OVERRIDE;
};

class VirtualKeyboardPrivateMoveCursorFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("virtualKeyboardPrivate.moveCursor",
                             VIRTUALKEYBOARDPRIVATE_MOVECURSOR);

 protected:
  virtual ~VirtualKeyboardPrivateMoveCursorFunction() {}

  // ExtensionFunction.
  virtual bool RunSync() OVERRIDE;
};

class VirtualKeyboardPrivateSendKeyEventFunction
    : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "virtualKeyboardPrivate.sendKeyEvent",
      VIRTUALKEYBOARDPRIVATE_SENDKEYEVENT);

 protected:
  virtual ~VirtualKeyboardPrivateSendKeyEventFunction() {}

  // ExtensionFunction:
  virtual bool RunSync() OVERRIDE;
};

class VirtualKeyboardPrivateHideKeyboardFunction
    : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "virtualKeyboardPrivate.hideKeyboard",
      VIRTUALKEYBOARDPRIVATE_HIDEKEYBOARD);

 protected:
  virtual ~VirtualKeyboardPrivateHideKeyboardFunction() {}

  // ExtensionFunction:
  virtual bool RunSync() OVERRIDE;
};

class VirtualKeyboardPrivateLockKeyboardFunction
    : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "virtualKeyboardPrivate.lockKeyboard",
      VIRTUALKEYBOARDPRIVATE_LOCKKEYBOARD);

 protected:
  virtual ~VirtualKeyboardPrivateLockKeyboardFunction() {}

  // ExtensionFunction:
  virtual bool RunSync() OVERRIDE;
};

class VirtualKeyboardPrivateKeyboardLoadedFunction
    : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "virtualKeyboardPrivate.keyboardLoaded",
      VIRTUALKEYBOARDPRIVATE_KEYBOARDLOADED);

 protected:
  virtual ~VirtualKeyboardPrivateKeyboardLoadedFunction() {}

  // ExtensionFunction:
  virtual bool RunSync() OVERRIDE;
};

class VirtualKeyboardPrivateGetKeyboardConfigFunction
    : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "virtualKeyboardPrivate.getKeyboardConfig",
      VIRTUALKEYBOARDPRIVATE_GETKEYBOARDCONFIG);

 protected:
  virtual ~VirtualKeyboardPrivateGetKeyboardConfigFunction() {}

  // ExtensionFunction:
  virtual bool RunSync() OVERRIDE;
};

class VirtualKeyboardPrivateOpenSettingsFunction
    : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("virtualKeyboardPrivate.openSettings",
                             VIRTUALKEYBOARDPRIVATE_OPENSETTINGS);

 protected:
  virtual ~VirtualKeyboardPrivateOpenSettingsFunction() {}

  // ExtensionFunction:
  virtual bool RunSync() OVERRIDE;
};


class InputAPI : public BrowserContextKeyedAPI {
 public:
  explicit InputAPI(content::BrowserContext* context);
  virtual ~InputAPI();

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
