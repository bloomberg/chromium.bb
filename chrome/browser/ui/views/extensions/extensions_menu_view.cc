// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_view.h"

#include "base/memory/ptr_util.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_button.h"
#include "chrome/grit/generated_resources.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"

namespace {
ExtensionsMenuView* g_extensions_dialog = nullptr;

constexpr int EXTENSIONS_SETTINGS_ID = 42;

gfx::ImageSkia CreateVectorIcon(const gfx::VectorIcon& icon) {
  return gfx::CreateVectorIcon(
      icon, 16,
      ui::NativeTheme::GetInstanceForNativeUi()->SystemDarkModeEnabled()
          ? gfx::kGoogleGrey500
          : gfx::kChromeIconGrey);
}

}  // namespace

ExtensionsMenuView::ExtensionsMenuView(views::View* anchor_view,
                                       Browser* browser,
                                       ToolbarActionsBar* toolbar_actions_bar)
    : BubbleDialogDelegateView(anchor_view,
                               views::BubbleBorder::Arrow::TOP_RIGHT),
      browser_(browser),
      toolbar_actions_bar_(toolbar_actions_bar),
      model_(ToolbarActionsModel::Get(browser_->profile())),
      model_observer_(this) {
  model_observer_.Add(model_);
  set_margins(gfx::Insets(0));

  AddAccelerator(ui::Accelerator(ui::VKEY_DOWN, ui::EF_NONE));
  AddAccelerator(ui::Accelerator(ui::VKEY_UP, ui::EF_NONE));

  SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical));
  Repopulate();
}

ExtensionsMenuView::~ExtensionsMenuView() {
  DCHECK_EQ(g_extensions_dialog, this);
  g_extensions_dialog = nullptr;
}

void ExtensionsMenuView::ButtonPressed(views::Button* sender,
                                       const ui::Event& event) {
  DCHECK_EQ(sender->id(), EXTENSIONS_SETTINGS_ID);
  chrome::ShowExtensions(browser_, std::string());
}

base::string16 ExtensionsMenuView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_EXTENSIONS_MENU_TITLE);
}

bool ExtensionsMenuView::ShouldShowCloseButton() const {
  return true;
}

bool ExtensionsMenuView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  if (accelerator.key_code() != ui::VKEY_DOWN &&
      accelerator.key_code() != ui::VKEY_UP)
    return BubbleDialogDelegateView::AcceleratorPressed(accelerator);

  // Move the focus up or down.
  GetFocusManager()->AdvanceFocus(accelerator.key_code() != ui::VKEY_DOWN);
  return true;
}

int ExtensionsMenuView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

bool ExtensionsMenuView::ShouldSnapFrameWidth() const {
  return true;
}

void ExtensionsMenuView::Repopulate() {
  RemoveAllChildViews(true);

  for (auto action_id : model_->action_ids()) {
    AddChildView(std::make_unique<ExtensionsMenuButton>(
        browser_, model_->CreateActionForId(browser_, toolbar_actions_bar_,
                                            false, action_id)));
  }

  AddChildView(std::make_unique<views::Separator>());
  auto footer = std::make_unique<HoverButton>(
      this, CreateVectorIcon(kSettingsIcon),
      l10n_util::GetStringUTF16(IDS_MANAGE_EXTENSION));
  footer->set_id(EXTENSIONS_SETTINGS_ID);
  manage_extensions_button_for_testing_ = footer.get();
  AddChildView(std::move(footer));
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
                                    Browser* browser,
                                    ToolbarActionsBar* toolbar_actions_bar) {
  DCHECK(!g_extensions_dialog);
  g_extensions_dialog =
      new ExtensionsMenuView(anchor_view, browser, toolbar_actions_bar);
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

ExtensionsMenuView* ExtensionsMenuView::GetExtensionsMenuViewForTesting() {
  return g_extensions_dialog;
}
