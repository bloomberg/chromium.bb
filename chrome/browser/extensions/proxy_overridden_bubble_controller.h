// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_PROXY_OVERRIDDEN_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_EXTENSIONS_PROXY_OVERRIDDEN_BUBBLE_CONTROLLER_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/extensions/extension_message_bubble_controller.h"

class Browser;

namespace extensions {

class ProxyOverriddenBubbleController
    : public ExtensionMessageBubbleController {
 public:
  explicit ProxyOverriddenBubbleController(Browser* browser);
  ~ProxyOverriddenBubbleController() override;

  // Whether the controller knows that we should show the bubble for extension
  // with |extension_id|. Returns true if so.
  bool ShouldShow(const std::string& extension_id);

  // ExtensionMessageBubbleController:
  bool CloseOnDeactivate() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProxyOverriddenBubbleController);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_PROXY_OVERRIDDEN_BUBBLE_CONTROLLER_H_
