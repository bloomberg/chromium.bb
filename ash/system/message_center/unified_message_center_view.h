// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MESSAGE_CENTER_UNIFIED_MESSAGE_CENTER_VIEW_H_
#define ASH_SYSTEM_MESSAGE_CENTER_UNIFIED_MESSAGE_CENTER_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/message_center/message_center_scroll_bar.h"
#include "ash/system/message_center/unified_message_list_view.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"

namespace views {
class ScrollView;
}  // namespace views

namespace ash {

class MessageCenterScrollBar;
class UnifiedSystemTrayModel;
class UnifiedSystemTrayView;

// The header shown above the notification list displaying the number of hidden
// notifications. There are currently two UI implementations toggled by the
// NotificationStackingBarRedesign feature flag.
class StackingNotificationCounterView : public views::View {
 public:
  explicit StackingNotificationCounterView(views::ButtonListener* listener);
  ~StackingNotificationCounterView() override;

  void SetCount(int total_notification_count, int stacked_notification_count);

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;

 private:
  friend class UnifiedMessageCenterViewTest;

  int total_notification_count_ = 0;
  int stacked_notification_count_ = 0;

  // These UI elements are only created and shown when the
  // NotificationStackingBarRedesign feature is enabled.
  views::Label* count_label_ = nullptr;
  views::Button* clear_all_button_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(StackingNotificationCounterView);
};

// Manages scrolling of notification list.
class ASH_EXPORT UnifiedMessageCenterView
    : public views::View,
      public MessageCenterScrollBar::Observer,
      public views::ButtonListener,
      public views::FocusChangeListener {
 public:
  UnifiedMessageCenterView(UnifiedSystemTrayView* parent,
                           UnifiedSystemTrayModel* model);
  ~UnifiedMessageCenterView() override;

  // Sets the maximum height that the view can take.
  void SetMaxHeight(int max_height);

  // Called from UnifiedMessageListView when the preferred size is changed.
  void ListPreferredSizeChanged();

  // Configures MessageView to forward scroll events. Called from
  // UnifiedMessageListView.
  void ConfigureMessageView(message_center::MessageView* message_view);

  // Count number of notifications that are above visible area.
  int GetStackedNotificationCount() const;

  // views::View:
  void AddedToWidget() override;
  void RemovedFromWidget() override;
  void Layout() override;
  gfx::Size CalculatePreferredSize() const override;

  // MessageCenterScrollBar::Observer:
  void OnMessageCenterScrolled() override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::FocusChangeListener:
  void OnWillChangeFocus(views::View* before, views::View* now) override;
  void OnDidChangeFocus(views::View* before, views::View* now) override;

 protected:
  // Virtual for testing.
  virtual void SetNotificationRectBelowScroll(
      const gfx::Rect& rect_below_scroll);

 private:
  friend class UnifiedMessageCenterViewTest;

  void UpdateVisibility();

  // Scroll the notification list to the target position.
  void ScrollToTarget();

  // Notifies rect below scroll to |parent_| so that it can update
  // TopCornerBorder.
  void NotifyRectBelowScroll();

  UnifiedSystemTrayView* const parent_;
  UnifiedSystemTrayModel* const model_;
  StackingNotificationCounterView* const stacking_counter_;
  MessageCenterScrollBar* const scroll_bar_;
  views::ScrollView* const scroller_;
  UnifiedMessageListView* const message_list_view_;

  // Position from the bottom of scroll contents in dip.
  int last_scroll_position_from_bottom_;

  views::FocusManager* focus_manager_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(UnifiedMessageCenterView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_MESSAGE_CENTER_UNIFIED_MESSAGE_CENTER_VIEW_H_
