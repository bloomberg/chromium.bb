// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_MESSAGE_BUBBLE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_MESSAGE_BUBBLE_H_

#include "base/bind.h"

class Browser;

namespace extensions {

// The interface between the SuspiciousExtensionBubble bubble and its
// controller.
class ExtensionMessageBubble {
 public:
  // Setup the callback for when the action button is clicked in the
  // bubble.
  virtual void OnActionButtonClicked(const base::Closure& callback) = 0;

  // Setup the callback for when the dismiss button is clicked.
  virtual void OnDismissButtonClicked(const base::Closure& callback) = 0;

  // Setup the callback for when the link is clicked in the bubble.
  virtual void OnLinkClicked(const base::Closure& callback) = 0;

  // Instruct the bubble to appear.
  virtual void Show() = 0;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_MESSAGE_BUBBLE_H_
