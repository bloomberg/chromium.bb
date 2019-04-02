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
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"

namespace {
ExtensionsMenuView* g_extensions_dialog = nullptr;

constexpr int EXTENSIONS_SETTINGS_ID = 42;

std::unique_ptr<views::View> CreateHeaderView(const base::string16& title) {
  auto container = std::make_unique<views::View>();

  auto* layout_manager =
      container->SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout_manager->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

  gfx::Insets header_insets =
      ChromeLayoutProvider::Get()->GetInsetsMetric(views::INSETS_DIALOG);
  container->SetProperty(views::kMarginsKey, new gfx::Insets(header_insets));

  views::Label* title_label =
      new views::Label(title, views::style::CONTEXT_DIALOG_TITLE);
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label->SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);

  container->AddChildView(title_label);
  layout_manager->SetFlexForView(title_label,
                                 views::FlexSpecification::ForSizeRule(
                                     views::MinimumFlexSizeRule::kPreferred,
                                     views::MaximumFlexSizeRule::kUnbounded));
  return container;
}
}  // namespace

ExtensionsMenuView::ExtensionsMenuView(views::View* anchor_view,
                                       Browser* browser)
    : BubbleDialogDelegateView(anchor_view,
                               views::BubbleBorder::Arrow::TOP_RIGHT),
      browser_(browser),
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
  if (sender->id() != EXTENSIONS_SETTINGS_ID)
    return;
  chrome::ShowExtensions(browser_, std::string());
}

base::string16 ExtensionsMenuView::GetAccessibleWindowTitle() const {
  // TODO(pbos): Revisit this when subpanes exist so that the title read by a11y
  // tools are in sync with the current visuals.
  return l10n_util::GetStringUTF16(IDS_EXTENSIONS_MENU_TITLE);
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

  auto header =
      CreateHeaderView(l10n_util::GetStringUTF16(IDS_EXTENSIONS_MENU_TITLE));
  header->AddChildView(CreateImageButtonForHeader(
      kSettingsIcon, EXTENSIONS_SETTINGS_ID,
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_MENU_SETTINGS_TOOLTIP)));
  AddChildView(std::move(header));

  for (auto action_id : model_->action_ids()) {
    AddChildView(std::make_unique<ExtensionsMenuButton>(
        browser_, model_->CreateActionForId(browser_, nullptr, action_id)));
  }

  // TODO(pbos): This is a placeholder until we have proper UI treatment of the
  // no-extensions case.
  if (model_->action_ids().empty()) {
    AddChildView(std::make_unique<views::Label>(
        l10n_util::GetStringUTF16(IDS_EXTENSIONS_MENU_TITLE)));
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

ExtensionsMenuView* ExtensionsMenuView::GetExtensionsMenuViewForTesting() {
  return g_extensions_dialog;
}

std::unique_ptr<views::ImageButton>
ExtensionsMenuView::CreateImageButtonForHeader(const gfx::VectorIcon& icon,
                                               int id,
                                               const base::string16& tooltip) {
  views::ImageButton* image_button = views::CreateVectorImageButton(this);
  views::SetImageFromVectorIconWithColor(
      image_button, icon,
      GetNativeTheme()->SystemDarkModeEnabled()
          ? SkColorSetA(SK_ColorWHITE, 0xDD)
          : gfx::kGoogleGrey700);
  image_button->set_id(id);
  image_button->SetTooltipText(tooltip);
  image_button->SizeToPreferredSize();

  // Let the settings button use a circular inkdrop shape.
  auto highlight_path = std::make_unique<SkPath>();
  highlight_path->addOval(gfx::RectToSkRect(gfx::Rect(image_button->size())));
  image_button->SetProperty(views::kHighlightPathKey, highlight_path.release());

  return base::WrapUnique<views::ImageButton>(image_button);
}
