// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_button.h"

#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"

const char ExtensionsMenuButton::kClassName[] = "ExtensionsMenuButton";

namespace {

content::WebContents* GetActiveWebContents(Browser* browser) {
  return browser->tab_strip_model()->GetActiveWebContents();
}

void SetIconViewImage(views::ImageView* image_view,
                      Browser* browser,
                      ToolbarActionViewController* controller) {
  image_view->SetImage(
      controller->GetIcon(GetActiveWebContents(browser), gfx::Size(16, 16))
          .AsImageSkia());
}

std::unique_ptr<views::View> CreateIconView(
    Browser* browser,
    ToolbarActionViewController* controller) {
  auto image_view = std::make_unique<views::ImageView>();
  SetIconViewImage(image_view.get(), browser, controller);
  return image_view;
}

}  // namespace

ExtensionsMenuButton::ExtensionsMenuButton(
    Browser* browser,
    std::unique_ptr<ToolbarActionViewController> controller)
    : HoverButton(this,
                  CreateIconView(browser, controller.get()),
                  controller->GetActionName(),
                  base::string16()),
      browser_(browser),
      controller_(std::move(controller)) {
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
  return GetActiveWebContents(browser_);
}

void ExtensionsMenuButton::UpdateState() {
  DCHECK_EQ(views::ImageView::kViewClassName, icon_view()->GetClassName());
  SetIconViewImage(static_cast<views::ImageView*>(icon_view()), browser_,
                   controller_.get());
  SetTitleTextWithHintRange(controller_->GetActionName(),
                            gfx::Range::InvalidRange());
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
