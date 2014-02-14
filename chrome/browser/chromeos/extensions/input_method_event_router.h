// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_INPUT_METHOD_EVENT_ROUTER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_INPUT_METHOD_EVENT_ROUTER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chromeos/ime/input_method_manager.h"

namespace content {
class BrowserContext;
}

namespace chromeos {

// Event router class for the input method events.
class ExtensionInputMethodEventRouter
    : public input_method::InputMethodManager::Observer {
 public:
  explicit ExtensionInputMethodEventRouter(content::BrowserContext* context);
  virtual ~ExtensionInputMethodEventRouter();

  // Implements input_method::InputMethodManager::Observer:
  virtual void InputMethodChanged(
      input_method::InputMethodManager* manager,
      bool show_message) OVERRIDE;

 private:
  content::BrowserContext* context_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInputMethodEventRouter);
};

}  // namespace chromeos
#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_INPUT_METHOD_EVENT_ROUTER_H_
