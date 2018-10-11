// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/new_unified_message_center_view.h"

#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/message_center/message_center_scroll_bar.h"
#include "ash/system/message_center/unified_message_list_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/sign_out_button.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "base/metrics/user_metrics.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/views/message_view.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {

NewUnifiedMessageCenterView::NewUnifiedMessageCenterView()
    : scroller_(new views::ScrollView()),
      message_list_view_(new UnifiedMessageListView(this)) {
  message_list_view_->Init();

  // Need to set the transparent background explicitly, since ScrollView has
  // set the default opaque background color.
  scroller_->SetContents(CreateScrollerContents());
  scroller_->SetBackgroundColor(SK_ColorTRANSPARENT);
  scroller_->SetVerticalScrollBar(new MessageCenterScrollBar(this));
  scroller_->set_draw_overflow_indicator(false);
  AddChildView(scroller_);

  UpdateVisibility();
}

NewUnifiedMessageCenterView::~NewUnifiedMessageCenterView() = default;

void NewUnifiedMessageCenterView::SetMaxHeight(int max_height) {
  scroller_->ClipHeightTo(0, max_height);
}

void NewUnifiedMessageCenterView::ListPreferredSizeChanged() {
  UpdateVisibility();
  PreferredSizeChanged();

  if (GetWidget())
    GetWidget()->SynthesizeMouseMoveEvent();
}

void NewUnifiedMessageCenterView::ConfigureMessageView(
    message_center::MessageView* message_view) {
  message_view->set_scroller(scroller_);
}

void NewUnifiedMessageCenterView::Layout() {
  // We have to manually layout because we want to override
  // CalculatePreferredSize().
  scroller_->SetBoundsRect(GetContentsBounds());
}

gfx::Size NewUnifiedMessageCenterView::CalculatePreferredSize() const {
  gfx::Size preferred_size = scroller_->GetPreferredSize();
  // Hide Clear All button at the buttom from initial viewport.
  preferred_size.set_height(preferred_size.height() -
                            3 * kUnifiedNotificationCenterSpacing);
  return preferred_size;
}

void NewUnifiedMessageCenterView::OnMessageCenterScrolled() {}

void NewUnifiedMessageCenterView::ButtonPressed(views::Button* sender,
                                                const ui::Event& event) {
  base::RecordAction(
      base::UserMetricsAction("StatusArea_Notifications_ClearAll"));

  message_center::MessageCenter::Get()->RemoveAllNotifications(
      true /* by_user */,
      message_center::MessageCenter::RemoveType::NON_PINNED);
}

void NewUnifiedMessageCenterView::UpdateVisibility() {
  SessionController* session_controller = Shell::Get()->session_controller();
  SetVisible(message_list_view_->child_count() > 0 &&
             session_controller->ShouldShowNotificationTray() &&
             !session_controller->IsScreenLocked());
}

views::View* NewUnifiedMessageCenterView::CreateScrollerContents() {
  views::View* scroller_contents = new views::View;
  auto* contents_layout = scroller_contents->SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical));
  contents_layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_STRETCH);
  scroller_contents->AddChildView(message_list_view_);

  views::View* button_container = new views::View;
  auto* button_layout =
      button_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::kHorizontal,
          gfx::Insets(kUnifiedNotificationCenterSpacing), 0));
  button_layout->set_main_axis_alignment(
      views::BoxLayout::MAIN_AXIS_ALIGNMENT_END);

  auto* clear_all_button = new RoundedLabelButton(
      this,
      l10n_util::GetStringUTF16(IDS_ASH_MESSAGE_CENTER_CLEAR_ALL_BUTTON_LABEL));
  clear_all_button->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_ASH_MESSAGE_CENTER_CLEAR_ALL_BUTTON_TOOLTIP));
  button_container->AddChildView(clear_all_button);
  scroller_contents->AddChildView(button_container);
  return scroller_contents;
}

}  // namespace ash
