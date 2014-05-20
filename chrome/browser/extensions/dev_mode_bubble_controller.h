// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_DEV_MODE_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_EXTENSIONS_DEV_MODE_BUBBLE_CONTROLLER_H_

#include <string>

#include "chrome/browser/extensions/extension_message_bubble_controller.h"

namespace extensions {

class Extension;

class DevModeBubbleController : public ExtensionMessageBubbleController {
 public:
  // Clears the list of profiles the bubble has been shown for. Should only be
  // used during testing.
  static void ClearProfileListForTesting();

  // Returns true if the extension is considered a Developer Mode extension.
  static bool IsDevModeExtension(const Extension* extension);

  explicit DevModeBubbleController(Profile* profile);
  virtual ~DevModeBubbleController();

  // Whether the controller knows of extensions to list in the bubble. Returns
  // true if so.
  bool ShouldShow();

  // ExtensionMessageBubbleController methods.
  virtual void Show(ExtensionMessageBubble* bubble) OVERRIDE;

 private:
  // A weak pointer to the profile we are associated with. Not owned by us.
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(DevModeBubbleController);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_DEV_MODE_BUBBLE_CONTROLLER_H_
