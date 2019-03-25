// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_view.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/hover_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_action_view_delegate_views.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/layout/box_layout.h"

namespace {
ExtensionsMenuView* g_extensions_dialog = nullptr;

class ExtensionsMenuButton : public HoverButton,
                             public views::ButtonListener,
                             public ToolbarActionViewDelegateViews,
                             public views::ContextMenuController {
 public:
  ExtensionsMenuButton(Browser* browser,
                       std::unique_ptr<ToolbarActionViewController> controller)
      : HoverButton(this, controller->GetActionName()),
        browser_(browser),
        controller_(std::move(controller)) {
    set_context_menu_controller(this);
    controller_->SetDelegate(this);
    UpdateState();
  }

 private:
  // views::ButtonListener:
  void ButtonPressed(Button* sender, const ui::Event& event) override {
    controller_->ExecuteAction(true);
  }

  // ToolbarActionViewDelegateViews:
  views::View* GetAsView() override { return this; }

  views::FocusManager* GetFocusManagerForAccelerator() override {
    return GetFocusManager();
  }

  views::View* GetReferenceViewForPopup() override {
    return BrowserView::GetBrowserViewForBrowser(browser_)
        ->toolbar()
        ->extensions_button();
  }

  content::WebContents* GetCurrentWebContents() const override {
    return browser_->tab_strip_model()->GetActiveWebContents();
  }

  void UpdateState() override {
    // TODO(pbos): Repopulate button.
  }

  bool IsMenuRunning() const override {
    // TODO(pbos): Implement when able to show context menus inside this bubble.
    return false;
  }

  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(views::View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override {
    // TODO(pbos): Implement this. This is a no-op implementation as it prevents
    // crashing actions that don't have a popup action.
  }

  Browser* const browser_;
  const std::unique_ptr<ToolbarActionViewController> controller_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionsMenuButton);
};

}  // namespace

ExtensionsMenuView::ExtensionsMenuView(views::View* anchor_view,
                                       Browser* browser)
    : BubbleDialogDelegateView(anchor_view,
                               views::BubbleBorder::Arrow::TOP_RIGHT),
      browser_(browser),
      model_(ToolbarActionsModel::Get(browser_->profile())),
      model_observer_(this) {
  model_observer_.Add(model_);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kVertical, gfx::Insets(0),
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_UNRELATED_CONTROL_HORIZONTAL)));
  Repopulate();
}

ExtensionsMenuView::~ExtensionsMenuView() {
  DCHECK_EQ(g_extensions_dialog, this);
  g_extensions_dialog = nullptr;
}

base::string16 ExtensionsMenuView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_EXTENSIONS_MENU_TITLE);
}

void ExtensionsMenuView::Repopulate() {
  RemoveAllChildViews(true);
  for (auto action_id : model_->action_ids()) {
    AddChildView(std::make_unique<ExtensionsMenuButton>(
        browser_, model_->CreateActionForId(browser_, nullptr, action_id)));
  }
}

// TODO(pbos): Revisit observed events below.
void ExtensionsMenuView::OnToolbarActionAdded(
    const ToolbarActionsModel::ActionId& item,
    int index) {
  Repopulate();
}
void ExtensionsMenuView::OnToolbarActionRemoved(
    const ToolbarActionsModel::ActionId& action_id) {
  Repopulate();
}
void ExtensionsMenuView::OnToolbarActionMoved(
    const ToolbarActionsModel::ActionId& action_id,
    int index) {
  Repopulate();
}
void ExtensionsMenuView::OnToolbarActionLoadFailed() {
  Repopulate();
}
void ExtensionsMenuView::OnToolbarActionUpdated(
    const ToolbarActionsModel::ActionId& action_id) {
  Repopulate();
}
void ExtensionsMenuView::OnToolbarVisibleCountChanged() {
  Repopulate();
}
void ExtensionsMenuView::OnToolbarHighlightModeChanged(bool is_highlighting) {
  Repopulate();
}
void ExtensionsMenuView::OnToolbarModelInitialized() {
  Repopulate();
}

// static
void ExtensionsMenuView::ShowBubble(views::View* anchor_view,
                                    Browser* browser) {
  DCHECK(!g_extensions_dialog);
  g_extensions_dialog = new ExtensionsMenuView(anchor_view, browser);
  views::BubbleDialogDelegateView::CreateBubble(g_extensions_dialog)->Show();
}

// static
bool ExtensionsMenuView::IsShowing() {
  return g_extensions_dialog != nullptr;
}

// static
void ExtensionsMenuView::Hide() {
  if (IsShowing())
    g_extensions_dialog->GetWidget()->Close();
}
