// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_MESSAGE_BUBBLE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_MESSAGE_BUBBLE_H_

#include "base/bind.h"

class Browser;

namespace extensions {

// The interface between an extension message bubble's view and its
// corresponding ExtensionMessageBubbleController.
class ExtensionMessageBubble {
 public:
  // Instruct the bubble to appear.
  virtual void Show() = 0;

 protected:
  virtual ~ExtensionMessageBubble() {}
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_MESSAGE_BUBBLE_H_
