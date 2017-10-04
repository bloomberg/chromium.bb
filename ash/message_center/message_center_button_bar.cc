// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/message_center/message_center_button_bar.h"

#include "ash/message_center/message_center_view.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/text_constants.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_tray.h"
#include "ui/message_center/notifier_settings.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/menu_button_listener.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/painter.h"

using message_center::MessageCenter;
using message_center::NotifierSettingsProvider;

namespace ash {

namespace {
constexpr int kButtonSize = 40;
}  // namespace

views::ToggleImageButton* CreateNotificationCenterButton(
    views::ButtonListener* listener,
    int normal_id,
    int hover_id,
    int pressed_id,
    int text_id) {
  auto* button = new views::ToggleImageButton(listener);
  ui::ResourceBundle& resource_bundle = ui::ResourceBundle::GetSharedInstance();
  button->SetImage(views::Button::STATE_NORMAL,
                   *resource_bundle.GetImageSkiaNamed(normal_id));
  button->SetImage(views::Button::STATE_HOVERED,
                   *resource_bundle.GetImageSkiaNamed(hover_id));
  button->SetImage(views::Button::STATE_PRESSED,
                   *resource_bundle.GetImageSkiaNamed(pressed_id));
  button->SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                            views::ImageButton::ALIGN_MIDDLE);
  if (text_id)
    button->SetTooltipText(resource_bundle.GetLocalizedString(text_id));

  button->SetFocusForPlatform();

  button->SetFocusPainter(views::Painter::CreateSolidFocusPainter(
      message_center::kFocusBorderColor, gfx::Insets(1, 2, 2, 2)));

  button->SetPreferredSize(gfx::Size(kButtonSize, kButtonSize));
  return button;
}

// MessageCenterButtonBar /////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
MessageCenterButtonBar::MessageCenterButtonBar(
    MessageCenterView* message_center_view,
    MessageCenter* message_center,
    NotifierSettingsProvider* notifier_settings_provider,
    bool settings_initially_visible,
    const base::string16& title)
    : message_center_view_(message_center_view),
      message_center_(message_center),
      title_arrow_(nullptr),
      notification_label_(nullptr),
      button_container_(nullptr),
      close_all_button_(nullptr),
      settings_button_(nullptr),
      quiet_mode_button_(nullptr) {
  SetPaintToLayer();
  SetBackground(
      views::CreateSolidBackground(MessageCenterView::kBackgroundColor));

  ui::ResourceBundle& resource_bundle = ui::ResourceBundle::GetSharedInstance();

  title_arrow_ = CreateNotificationCenterButton(
      this, IDR_NOTIFICATION_ARROW, IDR_NOTIFICATION_ARROW_HOVER,
      IDR_NOTIFICATION_ARROW_PRESSED, 0);

  // Keyboardists can use the gear button to switch modes.
  title_arrow_->SetFocusBehavior(FocusBehavior::NEVER);
  AddChildView(title_arrow_);

  notification_label_ = new views::Label(title);
  notification_label_->SetAutoColorReadabilityEnabled(false);
  notification_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  notification_label_->SetEnabledColor(message_center::kRegularTextColor);
  AddChildView(notification_label_);

  button_container_ = new views::View;
  button_container_->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal));
  quiet_mode_button_ = CreateNotificationCenterButton(
      this, IDR_NOTIFICATION_DO_NOT_DISTURB,
      IDR_NOTIFICATION_DO_NOT_DISTURB_HOVER,
      IDR_NOTIFICATION_DO_NOT_DISTURB_PRESSED,
      IDS_ASH_MESSAGE_CENTER_QUIET_MODE_BUTTON_TOOLTIP);
  quiet_mode_button_->SetToggledImage(
      views::Button::STATE_NORMAL,
      resource_bundle.GetImageSkiaNamed(
          IDR_NOTIFICATION_DO_NOT_DISTURB_PRESSED));
  quiet_mode_button_->SetToggledImage(
      views::Button::STATE_HOVERED,
      resource_bundle.GetImageSkiaNamed(
          IDR_NOTIFICATION_DO_NOT_DISTURB_PRESSED));
  quiet_mode_button_->SetToggledImage(
      views::Button::STATE_PRESSED,
      resource_bundle.GetImageSkiaNamed(
          IDR_NOTIFICATION_DO_NOT_DISTURB_PRESSED));
  SetQuietModeState(message_center->IsQuietMode());
  button_container_->AddChildView(quiet_mode_button_);

  close_all_button_ = CreateNotificationCenterButton(
      this, IDR_NOTIFICATION_CLEAR_ALL, IDR_NOTIFICATION_CLEAR_ALL_HOVER,
      IDR_NOTIFICATION_CLEAR_ALL_PRESSED,
      IDS_ASH_MESSAGE_CENTER_CLEAR_ALL_BUTTON_TOOLTIP);
  close_all_button_->SetImage(
      views::Button::STATE_DISABLED,
      *resource_bundle.GetImageSkiaNamed(IDR_NOTIFICATION_CLEAR_ALL_DISABLED));
  button_container_->AddChildView(close_all_button_);

  settings_button_ = CreateNotificationCenterButton(
      this, IDR_NOTIFICATION_SETTINGS, IDR_NOTIFICATION_SETTINGS_HOVER,
      IDR_NOTIFICATION_SETTINGS_PRESSED,
      IDS_ASH_MESSAGE_CENTER_SETTINGS_BUTTON_TOOLTIP);
  button_container_->AddChildView(settings_button_);

  SetCloseAllButtonEnabled(!settings_initially_visible);
  SetBackArrowVisible(settings_initially_visible);
  ViewVisibilityChanged();
}

void MessageCenterButtonBar::ViewVisibilityChanged() {
  views::GridLayout* layout = views::GridLayout::CreateAndInstall(this);
  views::ColumnSet* column = layout->AddColumnSet(0);
  constexpr int kFooterLeftMargin = 4;
  column->AddPaddingColumn(0, kFooterLeftMargin);
  if (title_arrow_->visible()) {
    // Column for the left-arrow used to back out of settings.
    column->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                      0.0f, views::GridLayout::FIXED, kButtonSize, 0);
  } else {
    constexpr int kLeftPaddingWidthForNonArrows = 16;
    column->AddPaddingColumn(0.0f, kLeftPaddingWidthForNonArrows);
  }

  // Column for the label "Notifications".
  column->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER, 0.0f,
                    views::GridLayout::USE_PREF, 0, 0);

  // Fills in the remaining space between "Notifications" and buttons.
  gfx::ImageSkia* settings_image =
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_NOTIFICATION_SETTINGS);
  const int image_margin =
      std::max(0, (kButtonSize - settings_image->width()) / 2);
  column->AddPaddingColumn(1.0f, image_margin);

  // The button area column.
  column->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER, 0.0f,
                    views::GridLayout::USE_PREF, 0, 0);

  constexpr int kFooterRightMargin = 14;
  const int right_margin = std::max(0, kFooterRightMargin - image_margin);
  column->AddPaddingColumn(0, right_margin);

  constexpr int kFooterTopMargin = 6;
  layout->AddPaddingRow(0, kFooterTopMargin);
  layout->StartRow(0, 0, kButtonSize);
  if (title_arrow_->visible())
    layout->AddView(title_arrow_);
  layout->AddView(notification_label_);
  layout->AddView(button_container_);
  constexpr int kFooterBottomMargin = 3;
  layout->AddPaddingRow(0, kFooterBottomMargin);
}

MessageCenterButtonBar::~MessageCenterButtonBar() {}

void MessageCenterButtonBar::SetSettingsAndQuietModeButtonsEnabled(
    bool enabled) {
  settings_button_->SetEnabled(enabled);
  quiet_mode_button_->SetEnabled(enabled);
}

void MessageCenterButtonBar::SetCloseAllButtonEnabled(bool enabled) {
  if (close_all_button_)
    close_all_button_->SetEnabled(enabled);
}

views::Button* MessageCenterButtonBar::GetCloseAllButtonForTest() const {
  return close_all_button_;
}

views::Button* MessageCenterButtonBar::GetQuietModeButtonForTest() const {
  return quiet_mode_button_;
}

views::Button* MessageCenterButtonBar::GetSettingsButtonForTest() const {
  return settings_button_;
}

void MessageCenterButtonBar::SetBackArrowVisible(bool visible) {
  if (title_arrow_)
    title_arrow_->SetVisible(visible);
  ViewVisibilityChanged();
  Layout();
}

void MessageCenterButtonBar::SetTitle(const base::string16& title) {
  notification_label_->SetText(title);
}

void MessageCenterButtonBar::SetButtonsVisible(bool visible) {
  settings_button_->SetVisible(visible);
  quiet_mode_button_->SetVisible(visible);

  if (close_all_button_)
    close_all_button_->SetVisible(visible);

  ViewVisibilityChanged();
  Layout();
}

void MessageCenterButtonBar::SetQuietModeState(bool is_quiet_mode) {
  quiet_mode_button_->SetToggled(is_quiet_mode);
}

void MessageCenterButtonBar::ChildVisibilityChanged(views::View* child) {
  InvalidateLayout();
}

void MessageCenterButtonBar::ButtonPressed(views::Button* sender,
                                           const ui::Event& event) {
  if (sender == close_all_button_) {
    message_center_view()->ClearAllClosableNotifications();
  } else if (sender == settings_button_ || sender == title_arrow_) {
    MessageCenterView* center_view = message_center_view();
    center_view->SetSettingsVisible(!center_view->settings_visible());
  } else if (sender == quiet_mode_button_) {
    if (message_center()->IsQuietMode())
      message_center()->SetQuietMode(false);
    else
      message_center()->EnterQuietModeWithExpire(base::TimeDelta::FromDays(1));
  } else {
    NOTREACHED();
  }
}

}  // namespace ash
