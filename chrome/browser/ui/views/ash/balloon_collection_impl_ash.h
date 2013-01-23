// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ASH_BALLOON_COLLECTION_IMPL_ASH_H_
#define CHROME_BROWSER_UI_VIEWS_ASH_BALLOON_COLLECTION_IMPL_ASH_H_

#include <set>

#include "chrome/browser/chromeos/notifications/balloon_view_host_chromeos.h"  // MessageCallback
#include "chrome/browser/notifications/balloon_collection_impl.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notifier_settings.h"

// Wrapper on top of ::BalloonCollectionImpl to provide integration between
// the Chrome notification UI and Ash notifications (ash::WebNotificationTray).
class BalloonCollectionImplAsh
    : public BalloonCollectionImpl,
      public message_center::MessageCenter::Delegate,
      public message_center::NotifierSettingsView::Delegate {
 public:
  BalloonCollectionImplAsh();
  virtual ~BalloonCollectionImplAsh();

  // Overridden from BalloonCollectionImpl.
  virtual void Add(const Notification& notification,
                   Profile* profile) OVERRIDE;
  virtual bool HasSpace() const OVERRIDE;

  // Overridden from message_center::MessageCenter::Delegate.
  virtual void NotificationRemoved(const std::string& notification_id) OVERRIDE;
  virtual void DisableExtension(const std::string& notification_id) OVERRIDE;
  virtual void DisableNotificationsFromSource(
      const std::string& notification_id) OVERRIDE;
  virtual void ShowSettings(const std::string& notification_id) OVERRIDE;
  virtual void OnClicked(const std::string& notification_id) OVERRIDE;
  virtual void OnButtonClicked(const std::string& notification_id,
                               int button_index) OVERRIDE;

  // Overridden from message_center::NotifierSettingsView::Delegate.
  virtual void GetNotifierList(
      std::vector<message_center::NotifierSettingsView::Notifier>* notifiers)
      OVERRIDE;
  virtual void SetNotifierEnabled(const std::string& id, bool enabled) OVERRIDE;
  virtual void OnNotifierSettingsClosing(
      message_center::NotifierSettingsView* view) OVERRIDE;

  // Adds a callback for WebUI message. Returns true if the callback
  // is succssfully registered, or false otherwise. It fails to add if
  // there is no notification that matches NotificationDelegate::id(),
  // or a callback for given message already exists. The callback
  // object is owned and deleted by callee.
  bool AddWebUIMessageCallback(
      const Notification& notification,
      const std::string& message,
      const chromeos::BalloonViewHost::MessageCallback& callback);

  // Updates the notification's content. It uses
  // NotificationDelegate::id() to check the equality of notifications.
  // Returns true if the notification has been updated. False if
  // no corresponding notification is found. This will not change the
  // visibility of the notification.
  bool UpdateNotification(const Notification& notification);

  // On Ash this behaves the same as UpdateNotification.
  bool UpdateAndShowNotification(const Notification& notification);

 protected:
  // Creates a new balloon. Overridable by unit tests.  The caller is
  // responsible for freeing the pointer returned.
  virtual Balloon* MakeBalloon(const Notification& notification,
                               Profile* profile) OVERRIDE;

  const extensions::Extension* GetBalloonExtension(Balloon* balloon);

 private:
  // The view displaying notifier settings. NULL if the settings are not
  // visible.
  message_center::NotifierSettingsView* settings_view_;

  DISALLOW_COPY_AND_ASSIGN(BalloonCollectionImplAsh);
};

#endif  // CHROME_BROWSER_UI_VIEWS_ASH_BALLOON_COLLECTION_IMPL_ASH_H_
