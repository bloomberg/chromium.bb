// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_MESSAGE_BUBBLE_FACTORY_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_MESSAGE_BUBBLE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"

class Browser;

namespace extensions {
class ExtensionMessageBubbleController;
}  // namespace extensions

// Create and show ExtensionMessageBubbles for either extensions that look
// suspicious and have therefore been disabled, or for extensions that are
// running in developer mode that we want to warn the user about.
class ExtensionMessageBubbleFactory {
 public:
  explicit ExtensionMessageBubbleFactory(Browser* browser);
  ~ExtensionMessageBubbleFactory();

  // Returns the controller for the bubble that should be shown, if any.
  scoped_ptr<extensions::ExtensionMessageBubbleController> GetController();

  // Enables the bubbles across all platforms for testing.
  static void set_enabled_for_tests(bool enabled);

 private:
  Browser* browser_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionMessageBubbleFactory);
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_MESSAGE_BUBBLE_FACTORY_H_
