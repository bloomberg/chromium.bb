// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NOTIFICATIONS_SYSTEM_NOTIFICATION_H_
#define CHROME_BROWSER_CHROMEOS_NOTIFICATIONS_SYSTEM_NOTIFICATION_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "base/basictypes.h"
#include "base/string16.h"
#include "chrome/browser/chromeos/notifications/balloon_view_host_chromeos.h"  // MessageCallback
#include "chrome/browser/notifications/notification_delegate.h"
#include "chromeos/dbus/power_manager_client.h"
#include "googleurl/src/gurl.h"

class Notification;
class Profile;

namespace chromeos {

class BalloonCollectionImplAura;
typedef class BalloonCollectionImplAura BalloonCollectionImplType;

// The system notification object handles the display of a system notification

class SystemNotification : public PowerManagerClient::Observer {
 public:
  // The profile is the current user profile. The id is any string used
  // to uniquely identify this notification. The title is the title of
  // the message to be displayed. On creation, the message is hidden.
  SystemNotification(Profile* profile,
                     const std::string& id,
                     int icon_resource_id,
                     const string16& title);

  // Allows to provide custom NotificationDelegate.
  SystemNotification(Profile* profile,
                     NotificationDelegate* delegate,
                     int icon_resource_id,
                     const string16& title);

  virtual ~SystemNotification();

  // PowerManagerClient::Observer override.
  virtual void UnlockScreen() OVERRIDE;

  void set_title(const string16& title) { title_ = title; }

  // Show will show or update the message for this notification
  // on a transition to urgent, the notification will be shown if it was
  // previously hidden or minimized by the user.
  void Show(const string16& message, bool urgent, bool sticky);

  // Same as Show() above with a footer link at the bottom and a callback
  // for when the link is clicked.
  void Show(const string16& message, const string16& link_text,
            const BalloonViewHost::MessageCallback& callback,
            bool urgent, bool sticky);

  // Hide will dismiss the notification, if the notification is already
  // hidden it does nothing
  void Hide();

  // Current visibility state for this notification.
  bool visible() const { return visible_; }

  // Current urgent state for this notification.
  bool urgent() const { return urgent_; }

 private:
  class Delegate : public NotificationDelegate {
   public:
    explicit Delegate(const std::string& id);
    virtual void Display() OVERRIDE {}
    virtual void Error() OVERRIDE {}
    virtual void Close(bool by_user) OVERRIDE {}
    virtual void Click() OVERRIDE {}
    virtual std::string id() const OVERRIDE;

   private:
    std::string id_;

    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  void Init(int icon_resource_id);
  void ShowNotification(const Notification& notify);

  Profile* profile_;
  BalloonCollectionImplType* collection_;
  scoped_refptr<NotificationDelegate> delegate_;
  string16 message_;
  string16 link_;
  BalloonViewHost::MessageCallback callback_;
  GURL icon_;
  string16 title_;
  bool visible_;
  bool sticky_;
  bool urgent_;
  bool show_on_unlock_;

  DISALLOW_COPY_AND_ASSIGN(SystemNotification);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NOTIFICATIONS_SYSTEM_NOTIFICATION_H_
