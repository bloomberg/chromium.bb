// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/unified_detailed_view_delegate.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_item_style.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/unified/top_shortcut_button.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

void ConfigureTitleTriView(TriView* tri_view, TriView::Container container) {
  std::unique_ptr<views::BoxLayout> layout;

  switch (container) {
    case TriView::Container::START:
      FALLTHROUGH;
    case TriView::Container::END:
      layout = std::make_unique<views::BoxLayout>(views::BoxLayout::kHorizontal,
                                                  gfx::Insets(),
                                                  kUnifiedTopShortcutSpacing);
      layout->set_main_axis_alignment(
          views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
      layout->set_cross_axis_alignment(
          views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
      break;
    case TriView::Container::CENTER:
      tri_view->SetFlexForContainer(TriView::Container::CENTER, 1.f);

      layout = std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical);
      layout->set_main_axis_alignment(
          views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
      layout->set_cross_axis_alignment(
          views::BoxLayout::CROSS_AXIS_ALIGNMENT_STRETCH);
      break;
  }

  tri_view->SetContainerLayout(container, std::move(layout));
  tri_view->SetMinSize(container,
                       gfx::Size(0, kUnifiedDetailedViewTitleRowHeight));
}

}  // namespace

UnifiedDetailedViewDelegate::UnifiedDetailedViewDelegate(
    UnifiedSystemTrayController* tray_controller)
    : tray_controller_(tray_controller) {}

UnifiedDetailedViewDelegate::~UnifiedDetailedViewDelegate() = default;

void UnifiedDetailedViewDelegate::TransitionToMainView(bool restore_focus) {
  tray_controller_->TransitionToMainView(restore_focus);
}

void UnifiedDetailedViewDelegate::CloseBubble() {
  tray_controller_->CloseBubble();
}

SkColor UnifiedDetailedViewDelegate::GetBackgroundColor(
    ui::NativeTheme* native_theme) {
  return SK_ColorTRANSPARENT;
}

bool UnifiedDetailedViewDelegate::IsOverflowIndicatorEnabled() const {
  return false;
}

TriView* UnifiedDetailedViewDelegate::CreateTitleRow(int string_id) {
  auto* tri_view = new TriView(kUnifiedTopShortcutSpacing);

  ConfigureTitleTriView(tri_view, TriView::Container::START);
  ConfigureTitleTriView(tri_view, TriView::Container::CENTER);
  ConfigureTitleTriView(tri_view, TriView::Container::END);

  auto* label = TrayPopupUtils::CreateDefaultLabel();
  label->SetText(l10n_util::GetStringUTF16(string_id));
  TrayPopupItemStyle style(TrayPopupItemStyle::FontStyle::TITLE);
  style.SetupLabel(label);
  tri_view->AddView(TriView::Container::CENTER, label);

  tri_view->SetContainerVisible(TriView::Container::END, false);
  tri_view->SetBorder(views::CreateEmptyBorder(kUnifiedTopShortcutPadding));

  return tri_view;
}

views::View* UnifiedDetailedViewDelegate::CreateTitleSeparator() {
  views::Separator* separator = new views::Separator();
  separator->SetColor(kUnifiedMenuSeparatorColor);
  separator->SetBorder(views::CreateEmptyBorder(
      kTitleRowProgressBarHeight - views::Separator::kThickness, 0, 0, 0));
  return separator;
}

views::Button* UnifiedDetailedViewDelegate::CreateBackButton(
    views::ButtonListener* listener) {
  // TODO(tetsui): Implement custom back button derived from CollapseButton.
  return new TopShortcutButton(listener, kSystemMenuArrowBackIcon,
                               IDS_ASH_STATUS_TRAY_PREVIOUS_MENU);
}

views::Button* UnifiedDetailedViewDelegate::CreateInfoButton(
    views::ButtonListener* listener,
    int info_accessible_name_id) {
  return new TopShortcutButton(listener, kSystemMenuInfoIcon,
                               info_accessible_name_id);
}

views::Button* UnifiedDetailedViewDelegate::CreateSettingsButton(
    views::ButtonListener* listener,
    int setting_accessible_name_id) {
  auto* button = new TopShortcutButton(listener, kSystemMenuSettingsIcon,
                                       setting_accessible_name_id);
  if (!TrayPopupUtils::CanOpenWebUISettings())
    button->SetEnabled(false);
  return button;
}

views::Button* UnifiedDetailedViewDelegate::CreateHelpButton(
    views::ButtonListener* listener) {
  auto* button = new TopShortcutButton(listener, kSystemMenuHelpIcon,
                                       IDS_ASH_STATUS_TRAY_HELP);
  // Help opens a web page, so treat it like Web UI settings.
  if (!TrayPopupUtils::CanOpenWebUISettings())
    button->SetEnabled(false);
  return button;
}

}  // namespace ash
