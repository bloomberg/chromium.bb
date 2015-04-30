// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_TOOLBAR_ICON_SURFACING_BUBBLE_DELEGATE_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_TOOLBAR_ICON_SURFACING_BUBBLE_DELEGATE_H_

#include "base/macros.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar_bubble_delegate.h"

class Profile;

// The delegate for the bubble to briefly educate users about the toolbar
// redesign.
class ExtensionToolbarIconSurfacingBubbleDelegate
    : public ToolbarActionsBarBubbleDelegate {
 public:
  explicit ExtensionToolbarIconSurfacingBubbleDelegate(Profile* profile);
  ~ExtensionToolbarIconSurfacingBubbleDelegate() override;

  // Returns true if the bubble should be shown for the given |profile|.
  static bool ShouldShowForProfile(Profile* profile);

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

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionToolbarIconSurfacingBubbleDelegate);
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_TOOLBAR_ICON_SURFACING_BUBBLE_DELEGATE_H_
