// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SUSPICIOUS_EXTENSION_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_EXTENSIONS_SUSPICIOUS_EXTENSION_BUBBLE_CONTROLLER_H_

#include "chrome/browser/extensions/extension_message_bubble_controller.h"

class Profile;

namespace extensions {

class SuspiciousExtensionBubble;

class SuspiciousExtensionBubbleController
    : public extensions::ExtensionMessageBubbleController {
 public:
  // Clears the list of profiles the bubble has been shown for. Should only be
  // used during testing.
  static void ClearProfileListForTesting();

  explicit SuspiciousExtensionBubbleController(Profile* profile);
  virtual ~SuspiciousExtensionBubbleController();

  // Whether the controller knows of extensions to list in the bubble. Returns
  // true if so.
  bool ShouldShow();

  // ExtensionMessageBubbleController methods.
  virtual void Show(ExtensionMessageBubble* bubble) OVERRIDE;

 private:
  // A weak pointer to the profile we are associated with. Not owned by us.
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(SuspiciousExtensionBubbleController);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SUSPICIOUS_EXTENSION_BUBBLE_CONTROLLER_H_
