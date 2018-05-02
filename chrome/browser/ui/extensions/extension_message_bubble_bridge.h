// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_MESSAGE_BUBBLE_BRIDGE_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_MESSAGE_BUBBLE_BRIDGE_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar_bubble_delegate.h"

namespace extensions {
class Extension;
class ExtensionMessageBubbleController;
}

// A bridge between an ExtensionMessageBubbleController and a
// ToolbarActionsBarBubble.
class ExtensionMessageBubbleBridge : public ToolbarActionsBarBubbleDelegate {
 public:
  explicit ExtensionMessageBubbleBridge(
      std::unique_ptr<extensions::ExtensionMessageBubbleController> controller);
  ~ExtensionMessageBubbleBridge() override;

 private:
  // ToolbarActionsBarBubbleDelegate:
  bool ShouldShow() override;
  bool ShouldCloseOnDeactivate() override;
  bool IsPolicyIndicationNeeded(const extensions::Extension* extension);
  base::string16 GetHeadingText() override;
  base::string16 GetBodyText(bool anchored_to_action) override;
  base::string16 GetItemListText() override;
  base::string16 GetActionButtonText() override;
  base::string16 GetDismissButtonText() override;
  ui::DialogButton GetDefaultDialogButton() override;
  std::unique_ptr<ExtraViewInfo> GetExtraViewInfo() override;
  std::string GetAnchorActionId() override;
  void OnBubbleShown(const base::Closure& close_bubble_callback) override;
  void OnBubbleClosed(CloseAction action) override;

  std::unique_ptr<extensions::ExtensionMessageBubbleController> controller_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionMessageBubbleBridge);
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_MESSAGE_BUBBLE_BRIDGE_H_
