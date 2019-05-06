// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"

#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"

ExtensionsToolbarContainer::ExtensionsToolbarContainer(Browser* browser)
    : browser_(browser),
      extensions_button_(new ExtensionsToolbarButton(browser_, this)) {
  AddMainView(extensions_button_);
}

ExtensionsToolbarContainer::~ExtensionsToolbarContainer() = default;

void ExtensionsToolbarContainer::UpdateAllIcons() {
  extensions_button_->UpdateIcon();
}

ToolbarActionViewController* ExtensionsToolbarContainer::GetActionForId(
    const std::string& action_id) {
  return nullptr;
}

ToolbarActionViewController* ExtensionsToolbarContainer::GetPoppedOutAction()
    const {
  return nullptr;
}

bool ExtensionsToolbarContainer::IsActionVisibleOnToolbar(
    const ToolbarActionViewController* action) const {
  return false;
}

void ExtensionsToolbarContainer::UndoPopOut() {}

void ExtensionsToolbarContainer::SetPopupOwner(
    ToolbarActionViewController* popup_owner) {}

void ExtensionsToolbarContainer::HideActivePopup() {}

bool ExtensionsToolbarContainer::CloseOverflowMenuIfOpen() {
  return false;
}

void ExtensionsToolbarContainer::PopOutAction(
    ToolbarActionViewController* action,
    bool is_sticky,
    const base::Closure& closure) {}
