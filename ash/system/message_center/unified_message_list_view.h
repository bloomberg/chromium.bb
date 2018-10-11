// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MESSAGE_CENTER_UNIFIED_MESSAGE_LIST_VIEW_H_
#define ASH_SYSTEM_MESSAGE_CENTER_UNIFIED_MESSAGE_LIST_VIEW_H_

#include "ash/ash_export.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/message_center/views/message_view.h"
#include "ui/views/view.h"

namespace message_center {
class MessageView;
class Notification;
}  // namespace message_center

namespace ash {

class NewUnifiedMessageCenterView;

// Manages list of notifications. The class doesn't know about the ScrollView
// it's enclosed. This class is used only from NewUnifiedMessageCenterView.
class ASH_EXPORT UnifiedMessageListView
    : public views::View,
      public message_center::MessageCenterObserver,
      public message_center::MessageView::SlideObserver {
 public:
  // |message_center_view| can be null in unit tests.
  explicit UnifiedMessageListView(
      NewUnifiedMessageCenterView* message_center_view);
  ~UnifiedMessageListView() override;

  // Initializes the view with existing notifications. Should be called right
  // after ctor.
  void Init();

  // Get the height of the notification at the bottom. If no notification is
  // added, it returns 0.
  int GetLastNotificationHeight() const;

  // views::View:
  void ChildPreferredSizeChanged(views::View* child) override;
  void PreferredSizeChanged() override;

  // message_center::MessageCenterObserver:
  void OnNotificationAdded(const std::string& id) override;
  void OnNotificationRemoved(const std::string& id, bool by_user) override;
  void OnNotificationUpdated(const std::string& id) override;

  // message_center::MessageView::SlideObserver:
  void OnSlideChanged(const std::string& notification_id) override;

 protected:
  // Virtual for testing.
  virtual message_center::MessageView* CreateMessageView(
      const message_center::Notification& notification);

 private:
  class MessageViewContainer;

  MessageViewContainer* GetContainer(int index);
  void CollapseAllNotifications();
  void UpdateBorders();

  NewUnifiedMessageCenterView* const message_center_view_;

  // If true, ChildPreferredSizeChanged() will be ignored. This is used in
  // CollapseAllNotifications() to prevent PreferredSizeChanged() triggered
  // multiple times because of sequential SetExpanded() calls.
  bool ignore_size_change_ = false;

  DISALLOW_COPY_AND_ASSIGN(UnifiedMessageListView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_MESSAGE_CENTER_UNIFIED_MESSAGE_LIST_VIEW_H_
