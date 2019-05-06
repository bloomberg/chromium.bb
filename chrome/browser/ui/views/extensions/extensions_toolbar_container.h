// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_CONTAINER_H_

#include "chrome/browser/ui/extensions/extensions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_icon_container_view.h"

class Browser;
class ExtensionsToolbarButton;

// Container for extensions shown in the toolbar. These include pinned
// extensions and extensions that are 'popped out' transitively to show dialogs
// or be called out to the user.
// This container is used when the extension-menu experiment is active as a
// replacement for BrowserActionsContainer and ToolbarActionsBar which are
// intended to be removed.
// TODO(crbug.com/943702): Remove note related to extensions menu when cleaning
// up after the experiment.
class ExtensionsToolbarContainer : public ToolbarIconContainerView,
                                   public ExtensionsContainer {
 public:
  explicit ExtensionsToolbarContainer(Browser* browser);
  ~ExtensionsToolbarContainer() override;

  ExtensionsToolbarButton* extensions_button() const {
    return extensions_button_;
  }

  // ToolbarIconContainerView:
  void UpdateAllIcons() override;

 private:
  // ExtensionsContainer:
  ToolbarActionViewController* GetActionForId(
      const std::string& action_id) override;
  ToolbarActionViewController* GetPoppedOutAction() const override;
  bool IsActionVisibleOnToolbar(
      const ToolbarActionViewController* action) const override;
  void UndoPopOut() override;
  void SetPopupOwner(ToolbarActionViewController* popup_owner) override;
  void HideActivePopup() override;
  bool CloseOverflowMenuIfOpen() override;
  void PopOutAction(ToolbarActionViewController* action,
                    bool is_sticky,
                    const base::Closure& closure) override;

  Browser* const browser_;
  ExtensionsToolbarButton* const extensions_button_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionsToolbarContainer);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_CONTAINER_H_
