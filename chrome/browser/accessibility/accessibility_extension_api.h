// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_ACCESSIBILITY_EXTENSION_API_H_
#define CHROME_BROWSER_ACCESSIBILITY_ACCESSIBILITY_EXTENSION_API_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/singleton.h"
#include "base/values.h"
#include "extensions/browser/extension_function.h"
#include "ui/accessibility/ax_enums.h"

// API function that enables or disables web content accessibility support.
class AccessibilityPrivateSetNativeAccessibilityEnabledFunction
    : public UIThreadExtensionFunction {
  ~AccessibilityPrivateSetNativeAccessibilityEnabledFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION(
      "accessibilityPrivate.setNativeAccessibilityEnabled",
      ACCESSIBILITY_PRIVATE_SETNATIVEACCESSIBILITYENABLED)
};

// API function that sets the location of the accessibility focus ring.
class AccessibilityPrivateSetFocusRingFunction
    : public UIThreadExtensionFunction {
  ~AccessibilityPrivateSetFocusRingFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.setFocusRing",
                             ACCESSIBILITY_PRIVATE_SETFOCUSRING)
};

// API function that sets keyboard capture mode.
class AccessibilityPrivateSetKeyboardListenerFunction
    : public UIThreadExtensionFunction {
  ~AccessibilityPrivateSetKeyboardListenerFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("accessibilityPrivate.setKeyboardListener",
                             ACCESSIBILITY_PRIVATE_SETKEYBOARDLISTENER)
};

#endif  // CHROME_BROWSER_ACCESSIBILITY_ACCESSIBILITY_EXTENSION_API_H_
