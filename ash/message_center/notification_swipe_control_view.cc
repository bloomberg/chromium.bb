// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/message_center/notification_swipe_control_view.h"

#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/vector_icons.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

const char NotificationSwipeControlView::kViewClassName[] =
    "NotificationSwipeControlView";

NotificationSwipeControlView::NotificationSwipeControlView() {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kHorizontal,
      gfx::Insets(message_center::kSwipeControlButtonVerticalMargin,
                  message_center::kSwipeControlButtonHorizontalMargin),
      message_center::kSwipeControlButtonHorizontalMargin));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_START);
  layout->set_main_axis_alignment(views::BoxLayout::MAIN_AXIS_ALIGNMENT_END);

  // Draw on its own layer to round corners
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
}

NotificationSwipeControlView::~NotificationSwipeControlView() = default;

void NotificationSwipeControlView::ShowButtons(ButtonPosition button_position,
                                               bool show_settings,
                                               bool show_snooze) {
  views::BoxLayout* layout = static_cast<views::BoxLayout*>(GetLayoutManager());
  if ((button_position == ButtonPosition::RIGHT) != base::i18n::IsRTL()) {
    layout->set_main_axis_alignment(views::BoxLayout::MAIN_AXIS_ALIGNMENT_END);
  } else {
    layout->set_main_axis_alignment(
        views::BoxLayout::MAIN_AXIS_ALIGNMENT_START);
  }
  ShowSettingsButton(show_settings);
  ShowSnoozeButton(show_snooze);
  Layout();
}

void NotificationSwipeControlView::HideButtons() {
  ShowSettingsButton(false);
  ShowSnoozeButton(false);
  Layout();
}

void NotificationSwipeControlView::ShowSettingsButton(bool show) {
  if (show && !settings_button_) {
    settings_button_ = new views::ImageButton(this);
    settings_button_->SetImage(
        views::Button::STATE_NORMAL,
        gfx::CreateVectorIcon(message_center::kNotificationSettingsButtonIcon,
                              message_center::kSwipeControlButtonImageSize,
                              gfx::kChromeIconGrey));
    settings_button_->SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                                        views::ImageButton::ALIGN_MIDDLE);
    settings_button_->SetPreferredSize(
        gfx::Size(message_center::kSwipeControlButtonSize,
                  message_center::kSwipeControlButtonSize));

    settings_button_->SetAccessibleName(l10n_util::GetStringUTF16(
        IDS_MESSAGE_NOTIFICATION_SETTINGS_BUTTON_ACCESSIBLE_NAME));
    settings_button_->SetTooltipText(l10n_util::GetStringUTF16(
        IDS_MESSAGE_NOTIFICATION_SETTINGS_BUTTON_ACCESSIBLE_NAME));
    settings_button_->SetBackground(
        views::CreateSolidBackground(SK_ColorTRANSPARENT));

    AddChildViewAt(settings_button_, child_count());
    Layout();
  } else if (!show && settings_button_) {
    DCHECK(Contains(settings_button_));
    delete settings_button_;
    settings_button_ = nullptr;
  }
}

void NotificationSwipeControlView::ShowSnoozeButton(bool show) {
  if (show && !snooze_button_) {
    snooze_button_ = new views::ImageButton(this);
    snooze_button_->SetImage(
        views::Button::STATE_NORMAL,
        gfx::CreateVectorIcon(message_center::kNotificationSnoozeButtonIcon,
                              message_center::kSwipeControlButtonImageSize,
                              gfx::kChromeIconGrey));
    snooze_button_->SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                                      views::ImageButton::ALIGN_MIDDLE);
    snooze_button_->SetPreferredSize(
        gfx::Size(message_center::kSwipeControlButtonSize,
                  message_center::kSwipeControlButtonSize));

    snooze_button_->SetAccessibleName(l10n_util::GetStringUTF16(
        IDS_MESSAGE_NOTIFICATION_SETTINGS_BUTTON_ACCESSIBLE_NAME));
    snooze_button_->SetTooltipText(l10n_util::GetStringUTF16(
        IDS_MESSAGE_NOTIFICATION_SETTINGS_BUTTON_ACCESSIBLE_NAME));
    snooze_button_->SetBackground(
        views::CreateSolidBackground(SK_ColorTRANSPARENT));

    AddChildViewAt(snooze_button_, 0);
    Layout();
  } else if (!show && snooze_button_) {
    DCHECK(Contains(snooze_button_));
    delete snooze_button_;
    snooze_button_ = nullptr;
  }
}

const char* NotificationSwipeControlView::GetClassName() const {
  return kViewClassName;
}

void NotificationSwipeControlView::ButtonPressed(views::Button* sender,
                                                 const ui::Event& event) {
  DCHECK(sender);
  if (sender == settings_button_) {
    for (Observer& observer : button_observers_)
      observer.OnSettingsButtonPressed(event);
  } else if (sender == snooze_button_) {
    for (Observer& observer : button_observers_)
      observer.OnSnoozeButtonPressed(event);
  }
}

void NotificationSwipeControlView::AddObserver(
    NotificationSwipeControlView::Observer* observer) {
  DCHECK(observer);
  button_observers_.AddObserver(observer);
}

}  // namespace ash
