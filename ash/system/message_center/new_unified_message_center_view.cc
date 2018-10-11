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

namespace {

constexpr int kClearAllButtonRowHeight = 3 * kUnifiedNotificationCenterSpacing;

class ScrollerContentsView : public views::View {
 public:
  ScrollerContentsView(UnifiedMessageListView* message_list_view,
                       views::ButtonListener* listener) {
    auto* contents_layout = SetLayoutManager(
        std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical));
    contents_layout->set_cross_axis_alignment(
        views::BoxLayout::CROSS_AXIS_ALIGNMENT_STRETCH);
    AddChildView(message_list_view);

    views::View* button_container = new views::View;
    auto* button_layout =
        button_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::kHorizontal,
            gfx::Insets(kUnifiedNotificationCenterSpacing), 0));
    button_layout->set_main_axis_alignment(
        views::BoxLayout::MAIN_AXIS_ALIGNMENT_END);

    auto* clear_all_button = new RoundedLabelButton(
        listener, l10n_util::GetStringUTF16(
                      IDS_ASH_MESSAGE_CENTER_CLEAR_ALL_BUTTON_LABEL));
    clear_all_button->SetTooltipText(l10n_util::GetStringUTF16(
        IDS_ASH_MESSAGE_CENTER_CLEAR_ALL_BUTTON_TOOLTIP));
    button_container->AddChildView(clear_all_button);
    AddChildView(button_container);
  }

  ~ScrollerContentsView() override = default;

  // views::View:
  void ChildPreferredSizeChanged(views::View* view) override {
    PreferredSizeChanged();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScrollerContentsView);
};

}  // namespace

NewUnifiedMessageCenterView::NewUnifiedMessageCenterView()
    : scroll_bar_(new MessageCenterScrollBar(this)),
      scroller_(new views::ScrollView()),
      message_list_view_(new UnifiedMessageListView(this)),
      position_from_bottom_(kClearAllButtonRowHeight) {
  message_list_view_->Init();

  // Need to set the transparent background explicitly, since ScrollView has
  // set the default opaque background color.
  scroller_->SetContents(new ScrollerContentsView(message_list_view_, this));
  scroller_->SetBackgroundColor(SK_ColorTRANSPARENT);
  scroller_->SetVerticalScrollBar(scroll_bar_);
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
  ScrollToPositionFromBottom();

  if (GetWidget() && !GetWidget()->IsClosed())
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

  // If the last notification is taller than |scroller_|, we should align the
  // top of the notification with the top of |scroller_|.
  const int last_notification_offset =
      message_list_view_->GetLastNotificationHeight() - scroller_->height() +
      kUnifiedNotificationCenterSpacing;
  if (position_from_bottom_ == kClearAllButtonRowHeight &&
      last_notification_offset > 0) {
    position_from_bottom_ += last_notification_offset;
  }
  ScrollToPositionFromBottom();
}

gfx::Size NewUnifiedMessageCenterView::CalculatePreferredSize() const {
  gfx::Size preferred_size = scroller_->GetPreferredSize();
  // Hide Clear All button at the buttom from initial viewport.
  preferred_size.set_height(preferred_size.height() - kClearAllButtonRowHeight);
  return preferred_size;
}

void NewUnifiedMessageCenterView::OnMessageCenterScrolled() {
  position_from_bottom_ =
      scroll_bar_->GetMaxPosition() - scroller_->GetVisibleRect().y();
}

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
  // When notification list went invisible, |position_from_bottom_| should be
  // reset.
  if (!visible())
    position_from_bottom_ = kClearAllButtonRowHeight;
}

void NewUnifiedMessageCenterView::ScrollToPositionFromBottom() {
  scroller_->ScrollToPosition(
      scroll_bar_,
      std::max(0, scroll_bar_->GetMaxPosition() - position_from_bottom_));
}

}  // namespace ash
