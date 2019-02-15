// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/unified_message_center_view.h"

#include <algorithm>

#include "ash/public/cpp/ash_features.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/message_center/ash_message_center_lock_screen_controller.h"
#include "ash/system/message_center/message_center_scroll_bar.h"
#include "ash/system/message_center/unified_message_list_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/sign_out_button.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "ash/system/unified/unified_system_tray_view.h"
#include "base/metrics/user_metrics.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
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

StackingNotificationCounterView::StackingNotificationCounterView() {
  SetBorder(views::CreateSolidSidedBorder(
      0, 0, 1, 0, kStackingNotificationCounterBorderColor));
}

StackingNotificationCounterView::~StackingNotificationCounterView() = default;

void StackingNotificationCounterView::SetCount(int stacking_count) {
  stacking_count_ = std::min(stacking_count, kStackingNotificationCounterMax);
  SetVisible(stacking_count > 0);
  SchedulePaint();
}

void StackingNotificationCounterView::OnPaint(gfx::Canvas* canvas) {
  int x = kStackingNotificationCounterStartX;
  const int y = kStackingNotificationCounterHeight / 2;

  cc::PaintFlags flags;
  flags.setColor(message_center::kNotificationBackgroundColor);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setAntiAlias(true);

  SkPath background_path;
  SkScalar top_radius = SkIntToScalar(kUnifiedTrayCornerRadius);
  SkScalar radii[8] = {top_radius, top_radius, top_radius, top_radius,
                       0,          0,          0,          0};
  background_path.addRoundRect(gfx::RectToSkRect(GetLocalBounds()), radii);
  canvas->DrawPath(background_path, flags);

  flags.setColor(kStackingNotificationCounterColor);
  for (int i = 0; i < stacking_count_; ++i) {
    canvas->DrawCircle(gfx::Point(x, y), kStackingNotificationCounterRadius,
                       flags);
    x += kStackingNotificationCounterDistanceX;
  }

  views::View::OnPaint(canvas);
}

UnifiedMessageCenterView::UnifiedMessageCenterView(
    UnifiedSystemTrayView* parent,
    UnifiedSystemTrayModel* model)
    : parent_(parent),
      model_(model),
      stacking_counter_(new StackingNotificationCounterView()),
      scroll_bar_(new MessageCenterScrollBar(this)),
      scroller_(new views::ScrollView()),
      message_list_view_(new UnifiedMessageListView(this, model)),
      last_scroll_position_from_bottom_(kClearAllButtonRowHeight) {
  message_list_view_->Init();

  AddChildView(stacking_counter_);

  // Need to set the transparent background explicitly, since ScrollView has
  // set the default opaque background color.
  scroller_->SetContents(new ScrollerContentsView(message_list_view_, this));
  scroller_->SetBackgroundColor(SK_ColorTRANSPARENT);
  scroller_->SetVerticalScrollBar(scroll_bar_);
  scroller_->set_draw_overflow_indicator(false);
  AddChildView(scroller_);

  UpdateVisibility();
}

UnifiedMessageCenterView::~UnifiedMessageCenterView() {
  model_->set_notification_target_mode(
      UnifiedSystemTrayModel::NotificationTargetMode::LAST_NOTIFICATION);

  RemovedFromWidget();
}

void UnifiedMessageCenterView::SetMaxHeight(int max_height) {
  scroller_->ClipHeightTo(0, max_height);
}

void UnifiedMessageCenterView::ListPreferredSizeChanged() {
  UpdateVisibility();
  PreferredSizeChanged();
  ScrollToTarget();
  Layout();

  if (GetWidget() && !GetWidget()->IsClosed())
    GetWidget()->SynthesizeMouseMoveEvent();
}

void UnifiedMessageCenterView::ConfigureMessageView(
    message_center::MessageView* message_view) {
  message_view->set_scroller(scroller_);
}

void UnifiedMessageCenterView::AddedToWidget() {
  focus_manager_ = GetFocusManager();
  if (focus_manager_)
    focus_manager_->AddFocusChangeListener(this);
}

void UnifiedMessageCenterView::RemovedFromWidget() {
  if (!focus_manager_)
    return;
  focus_manager_->RemoveFocusChangeListener(this);
  focus_manager_ = nullptr;
}

void UnifiedMessageCenterView::Layout() {
  stacking_counter_->SetCount(GetStackedNotificationCount());
  if (stacking_counter_->visible()) {
    gfx::Rect counter_bounds(GetContentsBounds());
    counter_bounds.set_height(kStackingNotificationCounterHeight);
    stacking_counter_->SetBoundsRect(counter_bounds);

    gfx::Rect scroller_bounds(GetContentsBounds());
    scroller_bounds.Inset(
        gfx::Insets(kStackingNotificationCounterHeight, 0, 0, 0));
    scroller_->SetBoundsRect(scroller_bounds);
  } else {
    scroller_->SetBoundsRect(GetContentsBounds());
  }

  ScrollToTarget();
  NotifyRectBelowScroll();
}

gfx::Size UnifiedMessageCenterView::CalculatePreferredSize() const {
  gfx::Size preferred_size = scroller_->GetPreferredSize();
  // Hide Clear All button at the buttom from initial viewport.
  preferred_size.set_height(preferred_size.height() - kClearAllButtonRowHeight);
  return preferred_size;
}

void UnifiedMessageCenterView::OnMessageCenterScrolled() {
  last_scroll_position_from_bottom_ =
      scroll_bar_->GetMaxPosition() - scroller_->GetVisibleRect().y();

  // Reset the target if user scrolls the list manually.
  model_->set_notification_target_mode(
      UnifiedSystemTrayModel::NotificationTargetMode::LAST_POSITION);

  const bool was_visible = stacking_counter_->visible();
  stacking_counter_->SetCount(GetStackedNotificationCount());
  if (was_visible != stacking_counter_->visible()) {
    const int previous_y = scroller_->y();
    Layout();
    // Adjust scroll position when counter visibility is changed so that
    // on-screen position of notification list does not change.
    scroll_bar_->ScrollByContentsOffset(previous_y - scroller_->y());
  }

  NotifyRectBelowScroll();
}

void UnifiedMessageCenterView::ButtonPressed(views::Button* sender,
                                             const ui::Event& event) {
  base::RecordAction(
      base::UserMetricsAction("StatusArea_Notifications_ClearAll"));
  message_list_view_->ClearAllWithAnimation();
}

void UnifiedMessageCenterView::OnWillChangeFocus(views::View* before,
                                                 views::View* now) {}

void UnifiedMessageCenterView::OnDidChangeFocus(views::View* before,
                                                views::View* now) {
  OnMessageCenterScrolled();
}

void UnifiedMessageCenterView::SetNotificationRectBelowScroll(
    const gfx::Rect& rect_below_scroll) {
  parent_->SetNotificationRectBelowScroll(rect_below_scroll);
}

void UnifiedMessageCenterView::UpdateVisibility() {
  SessionController* session_controller = Shell::Get()->session_controller();
  SetVisible(message_list_view_->GetPreferredSize().height() > 0 &&
             session_controller->ShouldShowNotificationTray() &&
             (!session_controller->IsScreenLocked() ||
              AshMessageCenterLockScreenController::IsEnabled()));
  // When notification list went invisible, the last notification should be
  // targeted next time.
  if (!visible()) {
    model_->set_notification_target_mode(
        UnifiedSystemTrayModel::NotificationTargetMode::LAST_NOTIFICATION);
  }
}

void UnifiedMessageCenterView::ScrollToTarget() {
  // Following logic doesn't work when the view is invisible, because it uses
  // the height of |scroller_|.
  if (!visible())
    return;

  int position;
  switch (model_->notification_target_mode()) {
    case UnifiedSystemTrayModel::NotificationTargetMode::LAST_POSITION:
      // Restore the previous scrolled position with matching the distance from
      // the bottom.
      position =
          scroll_bar_->GetMaxPosition() - last_scroll_position_from_bottom_;
      break;
    case UnifiedSystemTrayModel::NotificationTargetMode::NOTIFICATION_ID:
      FALLTHROUGH;
    case UnifiedSystemTrayModel::NotificationTargetMode::LAST_NOTIFICATION: {
      const gfx::Rect& target_rect =
          (model_->notification_target_mode() ==
           UnifiedSystemTrayModel::NotificationTargetMode::NOTIFICATION_ID)
              ? message_list_view_->GetNotificationBounds(
                    model_->notification_target_id())
              : message_list_view_->GetLastNotificationBounds();

      const int last_notification_offset = target_rect.height() -
                                           scroller_->height() +
                                           kUnifiedNotificationCenterSpacing;
      if (last_notification_offset > 0) {
        // If the target notification is taller than |scroller_|, we should
        // align the top of the notification with the top of |scroller_|.
        position = target_rect.y();
      } else {
        // Otherwise, we align the bottom of the notification with the bottom of
        // |scroller_|;
        position = target_rect.bottom() - scroller_->height();

        if (model_->notification_target_mode() ==
            UnifiedSystemTrayModel::NotificationTargetMode::LAST_NOTIFICATION) {
          position += kUnifiedNotificationCenterSpacing;
        }
      }
    }
  }

  scroller_->ScrollToPosition(scroll_bar_, position);
}

int UnifiedMessageCenterView::GetStackedNotificationCount() const {
  // CountNotificationsAboveY() only works after SetBoundsRect() is called at
  // least once.
  if (scroller_->bounds().IsEmpty())
    scroller_->SetBoundsRect(GetContentsBounds());

  // Consistently use the y offset absolutely kStackingNotificationCounterHeight
  // below the top of UnifiedMessageCenterView to count number of hidden
  // notifications.
  const int y_offset = scroller_->GetVisibleRect().y() - scroller_->y() +
                       kStackingNotificationCounterHeight;
  return message_list_view_->CountNotificationsAboveY(y_offset);
}

void UnifiedMessageCenterView::NotifyRectBelowScroll() {
  if (!visible())
    return;

  gfx::Rect rect_below_scroll;
  rect_below_scroll.set_height(
      std::max(0, message_list_view_->GetLastNotificationBounds().bottom() -
                      scroller_->GetVisibleRect().bottom()));

  gfx::Rect notification_bounds =
      message_list_view_->GetNotificationBoundsBelowY(
          scroller_->GetVisibleRect().bottom());
  rect_below_scroll.set_x(notification_bounds.x());
  rect_below_scroll.set_width(notification_bounds.width());

  SetNotificationRectBelowScroll(rect_below_scroll);
}

}  // namespace ash
