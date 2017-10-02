// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MESSAGE_CENTER_MESSAGE_CENTER_VIEW_H_
#define ASH_MESSAGE_CENTER_MESSAGE_CENTER_VIEW_H_

#include <stddef.h>

#include "ash/ash_export.h"
#include "ash/message_center/message_list_view.h"
#include "base/macros.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/message_center/notification_list.h"
#include "ui/message_center/views/message_center_controller.h"
#include "ui/message_center/views/message_view_context_menu_controller.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"

namespace gfx {
class MultiAnimation;
}  // namespace gfx

namespace message_center {

class MessageCenter;
class MessageCenterTray;
class MessageView;

}  // namespace message_center

namespace ash {

class MessageCenterButtonBar;
class NotifierSettingsView;

// Container for all the top-level views in the notification center, such as the
// button bar, settings view, scrol view, and message list view.  Acts as a
// controller for the message list view, passing data back and forth to message
// center.
class ASH_EXPORT MessageCenterView
    : public views::View,
      public message_center::MessageCenterObserver,
      public message_center::MessageCenterController,
      public MessageListView::Observer,
      public gfx::AnimationDelegate,
      public views::FocusChangeListener {
 public:
  MessageCenterView(message_center::MessageCenter* message_center,
                    message_center::MessageCenterTray* tray,
                    int max_height,
                    bool initially_settings_visible);
  ~MessageCenterView() override;

  void Init();

  void SetNotifications(
      const message_center::NotificationList::Notifications& notifications);

  void ClearAllClosableNotifications();

  size_t NumMessageViewsForTest() const;

  void SetSettingsVisible(bool visible);
  void OnSettingsChanged();
  bool settings_visible() const { return settings_visible_; }
  message_center::MessageCenterTray* tray() { return tray_; }

  void SetIsClosing(bool is_closing);

  // Overridden from views::FocusChangeListener
  void OnWillChangeFocus(views::View* before, views::View* now) override {}
  void OnDidChangeFocus(views::View* before, views::View* now) override;

  static const SkColor kBackgroundColor;
  static const size_t kMaxVisibleNotifications;

 protected:
  // Potentially sets the reposition target, and then returns whether or not it
  // was was set.
  virtual bool SetRepositionTarget();

  // Overridden from views::View:
  void Layout() override;
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  bool OnMouseWheel(const ui::MouseWheelEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;

  // Overridden from MessageCenterObserver:
  void OnNotificationAdded(const std::string& id) override;
  void OnNotificationRemoved(const std::string& id, bool by_user) override;
  void OnNotificationUpdated(const std::string& id) override;
  void OnLockedStateChanged(bool locked) override;

  // Overridden from MessageCenterController:
  void ClickOnNotification(const std::string& notification_id) override;
  void RemoveNotification(const std::string& notification_id,
                          bool by_user) override;
  std::unique_ptr<ui::MenuModel> CreateMenuModel(
      const message_center::NotifierId& notifier_id,
      const base::string16& display_source) override;
  bool HasClickedListener(const std::string& notification_id) override;
  void ClickOnNotificationButton(const std::string& notification_id,
                                 int button_index) override;
  void ClickOnSettingsButton(const std::string& notification_id) override;
  void UpdateNotificationSize(const std::string& notification_id) override;

  // Overridden from MessageListView::Observer:
  void OnAllNotificationsCleared() override;

  // Overridden from gfx::AnimationDelegate:
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;

 private:
  friend class MessageCenterViewTest;

  enum class Mode { NOTIFICATIONS, SETTINGS, BUTTONS_ONLY };

  static bool disable_animation_for_testing;

  void AddNotificationAt(const message_center::Notification& notification,
                         int index);
  base::string16 GetButtonBarTitle() const;
  void Update(bool animate);
  void SetVisibilityMode(Mode mode, bool animate);
  void UpdateButtonBarStatus();
  void EnableCloseAllIfAppropriate();
  void SetNotificationViewForTest(message_center::MessageView* view);
  void UpdateNotification(const std::string& notification_id);

  message_center::MessageCenter* message_center_;
  message_center::MessageCenterTray* tray_;

  // Child views.
  views::ScrollView* scroller_;
  std::unique_ptr<MessageListView> message_list_view_;
  NotifierSettingsView* settings_view_;
  MessageCenterButtonBar* button_bar_;

  // Data for transition animation between settings view and message list.
  bool settings_visible_;

  // Animation managing transition between message center and settings (and vice
  // versa).
  std::unique_ptr<gfx::MultiAnimation> settings_transition_animation_;

  // Helper data to keep track of the transition between settings and
  // message center views.
  views::View* source_view_;
  int source_height_;
  views::View* target_view_;
  int target_height_;

  // True when the widget is closing so that further operations should be
  // ignored.
  bool is_closing_;

  bool is_clearing_all_notifications_ = false;
  bool is_locked_ = false;

  // Current view mode. During animation, it is the target mode.
  Mode mode_ = Mode::BUTTONS_ONLY;

  message_center::MessageViewContextMenuController context_menu_controller_;

  views::FocusManager* focus_manager_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(MessageCenterView);
};

}  // namespace ash

#endif  // ASH_MESSAGE_CENTER_MESSAGE_CENTER_VIEW_H_
