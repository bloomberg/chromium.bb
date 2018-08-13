// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_UNIFIED_MESSAGE_CENTER_VIEW_H_
#define ASH_SYSTEM_UNIFIED_UNIFIED_MESSAGE_CENTER_VIEW_H_

#include <stddef.h>

#include "ash/ash_export.h"
#include "ash/message_center/message_center_scroll_bar.h"
#include "ash/message_center/message_list_view.h"
#include "ash/system/tray/tray_constants.h"
#include "base/macros.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/message_center/notification_list.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"

namespace message_center {

class MessageCenter;

}  // namespace message_center

namespace views {

class FocusManager;

}  // namespace views

namespace ash {

class UnifiedSystemTrayController;
class UnifiedSystemTrayView;

class StackingNotificationCounterView : public views::View {
 public:
  StackingNotificationCounterView();
  ~StackingNotificationCounterView() override;

  void SetCount(int stacking_count);

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;

 private:
  int stacking_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(StackingNotificationCounterView);
};

// Container for message list view. Acts as a controller/delegate of message
// list view, passing data back and forth to message center.
class ASH_EXPORT UnifiedMessageCenterView
    : public views::View,
      public message_center::MessageCenterObserver,
      public views::ViewObserver,
      public views::ButtonListener,
      public views::FocusChangeListener,
      public MessageListView::Observer,
      public MessageCenterScrollBar::Observer {
 public:
  UnifiedMessageCenterView(UnifiedSystemTrayController* tray_controller,
                           UnifiedSystemTrayView* parent,
                           message_center::MessageCenter* message_center);
  ~UnifiedMessageCenterView() override;

  // Initialize focus listener.
  void Init();

  // Set the maximum height that the view can take.
  void SetMaxHeight(int max_height);

  // Show the animation of clearing all notifications. After the animation is
  // finished, UnifiedSystemTrayController::OnClearAllAnimationEnded() will be
  // called.
  void ShowClearAllAnimation();

 protected:
  void SetNotifications(
      const message_center::NotificationList::Notifications& notifications);

  // views::View:
  void Layout() override;
  gfx::Size CalculatePreferredSize() const override;

  // message_center::MessageCenterObserver:
  void OnNotificationAdded(const std::string& id) override;
  void OnNotificationRemoved(const std::string& id, bool by_user) override;
  void OnNotificationUpdated(const std::string& id) override;

  // views::ViewObserver:
  void OnViewPreferredSizeChanged(views::View* observed_view) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::FocusChangeListener:
  void OnWillChangeFocus(views::View* before, views::View* now) override;
  void OnDidChangeFocus(views::View* before, views::View* now) override;

  // MessageListView::Observer:
  void OnAllNotificationsCleared() override;

  // MessageCenterScrollBar::Observer:
  void OnMessageCenterScrolled() override;

  // Notify the height below scroll to UnifiedSystemTrayView in order to imitate
  // notification list scrolling under SystemTray.
  void NotifyHeightBelowScroll();

 private:
  void Update();
  void AddNotificationAt(const message_center::Notification& notification,
                         int index);
  void UpdateNotification(const std::string& notification_id);

  // Scroll the notification list to |position_from_bottom_|.
  void ScrollToPositionFromBottom();

  // If |force| is false, it might not do the actual layout i.e. it assumes
  // the reason of layout change is limited to |stacking_counter_| visibility.
  void LayoutInternal(bool force);

  UnifiedSystemTrayController* const tray_controller_;
  UnifiedSystemTrayView* const parent_;
  message_center::MessageCenter* const message_center_;

  StackingNotificationCounterView* const stacking_counter_;
  MessageCenterScrollBar* const scroll_bar_;
  views::ScrollView* const scroller_;
  MessageListView* const message_list_view_;

  // Position from the bottom of scroll contents in dip. Hide Clear All button
  // at the buttom from initial viewport.
  int position_from_bottom_ = 3 * kUnifiedNotificationCenterSpacing;

  views::FocusManager* focus_manager_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(UnifiedMessageCenterView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_UNIFIED_MESSAGE_CENTER_VIEW_H_
