// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_button.h"

#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_view.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"

const char ExtensionsMenuButton::kClassName[] = "ExtensionsMenuButton";

ExtensionsMenuButton::ExtensionsMenuButton(
    Browser* browser,
    std::unique_ptr<ToolbarActionViewController> controller)
    : HoverButton(this,
                  ExtensionsMenuView::CreateFixedSizeIconView(),
                  base::string16(),
                  base::string16()),
      browser_(browser),
      controller_(std::move(controller)) {
  set_auto_compute_tooltip(false);
  set_context_menu_controller(this);
  controller_->SetDelegate(this);
  UpdateState();
}

ExtensionsMenuButton::~ExtensionsMenuButton() = default;

const char* ExtensionsMenuButton::GetClassName() const {
  return kClassName;
}

void ExtensionsMenuButton::ButtonPressed(Button* sender,
                                         const ui::Event& event) {
  controller_->ExecuteAction(true);
}

// ToolbarActionViewDelegateViews:
views::View* ExtensionsMenuButton::GetAsView() {
  return this;
}

views::FocusManager* ExtensionsMenuButton::GetFocusManagerForAccelerator() {
  return GetFocusManager();
}

views::View* ExtensionsMenuButton::GetReferenceViewForPopup() {
  return BrowserView::GetBrowserViewForBrowser(browser_)
      ->toolbar()
      ->extensions_button();
}

content::WebContents* ExtensionsMenuButton::GetCurrentWebContents() const {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

void ExtensionsMenuButton::UpdateState() {
  DCHECK_EQ(views::ImageView::kViewClassName, icon_view()->GetClassName());
  static_cast<views::ImageView*>(icon_view())
      ->SetImage(controller_
                     ->GetIcon(GetCurrentWebContents(),
                               icon_view()->GetPreferredSize())
                     .AsImageSkia());
  SetTitleTextWithHintRange(controller_->GetActionName(),
                            gfx::Range::InvalidRange());
  SetTooltipText(controller_->GetTooltip(GetCurrentWebContents()));
}

bool ExtensionsMenuButton::IsMenuRunning() const {
  // TODO(pbos): Implement when able to show context menus inside this bubble.
  return false;
}

// views::ContextMenuController:
void ExtensionsMenuButton::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  // TODO(pbos): Implement this. This is a no-op implementation as it prevents
  // crashing actions that don't have a popup action.
}
