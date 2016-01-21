// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_INPUT_IME_INPUT_IME_API_NONCHROMEOS_H_
#define CHROME_BROWSER_EXTENSIONS_API_INPUT_IME_INPUT_IME_API_NONCHROMEOS_H_

#include "chrome/browser/extensions/api/input_ime/input_ime_event_router_base.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_function.h"

class Profile;

namespace input_method {
class InputMethodEngine;
}  // namespace input_method

namespace extensions {

class InputImeEventRouterBase;

class InputImeEventRouter : public InputImeEventRouterBase {
 public:
  explicit InputImeEventRouter(Profile* profile);
  ~InputImeEventRouter() override;

  // Registers the extension as an IME extension, allowing it to be the active
  // engine.
  bool RegisterImeExtension(const std::string& extension_id);

  // Unregisters the extension as an IME extension and deactivates the IME
  // engine for it, if it was active.
  void UnregisterImeExtension(const std::string& extension_id);

  // Gets the input method engine if the extension is active.
  input_method::InputMethodEngine* GetActiveEngine(
      const std::string& extension_id);

  // Actives the extension with new input method engine, and deletes the
  // previous engine if another extension was active.
  void SetActiveEngine(const std::string& extension_id);

 private:
  // Deletes the current input method engine.
  void DeleteInputMethodEngine();

  // The active input method engine.
  input_method::InputMethodEngine* active_engine_;

  // The id of the all registered extensions.
  std::vector<std::string> extension_ids_;

  DISALLOW_COPY_AND_ASSIGN(InputImeEventRouter);
};

class InputImeCreateWindowFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.createWindow", INPUT_IME_CREATEWINDOW)

 protected:
  ~InputImeCreateWindowFunction() override {}

  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;
};

class InputImeActivateFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.activate", INPUT_IME_ACTIVATE)

 protected:
  ~InputImeActivateFunction() override {}

  // UIThreadExtensionFunction:
  ResponseAction Run() override;
};

class InputImeDeactivateFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.deactivate", INPUT_IME_DEACTIVATE)

 protected:
  ~InputImeDeactivateFunction() override {}

  // UIThreadExtensionFunction:
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_INPUT_IME_INPUT_IME_API_NONCHROMEOS_H_
