// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_MESSAGE_BUBBLE_BRIDGE_H_
#define CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_MESSAGE_BUBBLE_BRIDGE_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar_bubble_delegate.h"

@class ToolbarActionsBarBubbleMac;

namespace extensions {
class ExtensionMessageBubbleController;
}

// A bridge between an ExtensionMessageBubbleController and a
// ToolbarActionsBarBubble.
class ExtensionMessageBubbleBridge : public ToolbarActionsBarBubbleDelegate {
 public:
  ExtensionMessageBubbleBridge(
      scoped_ptr<extensions::ExtensionMessageBubbleController> controller,
      bool anchored_to_extension);
  ~ExtensionMessageBubbleBridge() override;

 private:
  // ToolbarActionsBarBubbleDelegate:
  base::string16 GetHeadingText() override;
  base::string16 GetBodyText() override;
  base::string16 GetItemListText() override;
  base::string16 GetActionButtonText() override;
  base::string16 GetDismissButtonText() override;
  base::string16 GetLearnMoreButtonText() override;
  void OnBubbleShown() override;
  void OnBubbleClosed(CloseAction action) override;

  scoped_ptr<extensions::ExtensionMessageBubbleController> controller_;

  // True if the bubble is anchored to an extension action.
  bool anchored_to_extension_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionMessageBubbleBridge);
};

#endif  // CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_MESSAGE_BUBBLE_BRIDGE_H_
