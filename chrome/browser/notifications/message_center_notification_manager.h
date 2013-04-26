// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_MESSAGE_CENTER_NOTIFICATION_MANAGER_H_
#define CHROME_BROWSER_NOTIFICATIONS_MESSAGE_CENTER_NOTIFICATION_MANAGER_H_

#include <map>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/notifications/notification_ui_manager_impl.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/message_center/message_center_tray_delegate.h"

class MessageCenterSettingsController;
class Notification;
class Profile;

// This class extends NotificationUIManagerImpl and delegates actual display
// of notifications to MessageCenter, doing necessary conversions.
class MessageCenterNotificationManager
    : public NotificationUIManagerImpl,
      public message_center::MessageCenter::Delegate,
      public message_center::MessageCenterObserver {
 public:
  explicit MessageCenterNotificationManager(
      message_center::MessageCenter* message_center);
  virtual ~MessageCenterNotificationManager();

  // NotificationUIManager
  virtual bool DoesIdExist(const std::string& notification_id) OVERRIDE;
  virtual bool CancelById(const std::string& notification_id) OVERRIDE;
  virtual bool CancelAllBySourceOrigin(const GURL& source_origin) OVERRIDE;
  virtual bool CancelAllByProfile(Profile* profile) OVERRIDE;
  virtual void CancelAll() OVERRIDE;

  // NotificationUIManagerImpl
  virtual bool ShowNotification(const Notification& notification,
                                Profile* profile) OVERRIDE;
  virtual bool UpdateNotification(const Notification& notification,
                                  Profile* profile) OVERRIDE;

  // MessageCenter::Delegate
  virtual void DisableExtension(const std::string& notification_id) OVERRIDE;
  virtual void DisableNotificationsFromSource(
      const std::string& notification_id) OVERRIDE;
  virtual void ShowSettings(const std::string& notification_id) OVERRIDE;
  virtual void ShowSettingsDialog(gfx::NativeView context) OVERRIDE;

  // MessageCenterObserver
  virtual void OnNotificationRemoved(const std::string& notification_id,
                                     bool by_user) OVERRIDE;
  virtual void OnNotificationClicked(
      const std::string& notification_id) OVERRIDE;
  virtual void OnNotificationButtonClicked(
      const std::string& notification_id,
      int button_index) OVERRIDE;
  virtual void OnNotificationDisplayed(
      const std::string& notification_id) OVERRIDE;

 private:
  class ImageDownloadsObserver {
   public:
    virtual void OnDownloadsCompleted() = 0;
  };

  typedef base::Callback<void(const gfx::Image&)> SetImageCallback;
  class ImageDownloads
      : public base::SupportsWeakPtr<ImageDownloads> {
   public:
    ImageDownloads(
        message_center::MessageCenter* message_center,
        ImageDownloadsObserver* observer);
    virtual ~ImageDownloads();

    void StartDownloads(const Notification& notification);
    void StartDownloadWithImage(const Notification& notification,
                                const gfx::Image* image,
                                const GURL& url,
                                int size,
                                const SetImageCallback& callback);
    void StartDownloadByKey(const Notification& notification,
                            const char* key,
                            int size,
                            const SetImageCallback& callback);

    // FaviconHelper callback.
    void DownloadComplete(const SetImageCallback& callback,
                          int download_id,
                          const GURL& image_url,
                          int requested_size,
                          const std::vector<SkBitmap>& bitmaps);
   private:
    // Used to keep track of the number of pending downloads.  Once this
    // reaches zero, we can tell the delegate that we don't need the
    // RenderViewHost anymore.
    void AddPendingDownload();
    void PendingDownloadCompleted();

    // Weak reference to global message center.
    message_center::MessageCenter* message_center_;

    // Count of downloads that remain.
    size_t pending_downloads_;

    // Weak.
    ImageDownloadsObserver* observer_;

    DISALLOW_COPY_AND_ASSIGN(ImageDownloads);
  };

  // This class keeps a set of original Notification objects and corresponding
  // Profiles, so when MessageCenter calls back with a notification_id, this
  // class has necessary mapping to other source info - for example, it calls
  // NotificationDelegate supplied by client when someone clicks on a
  // Notification in MessageCenter. Likewise, if a Profile or Extension is
  // being removed, the  map makes it possible to revoke the notifications from
  // MessageCenter.   To keep that set, we use the private ProfileNotification
  // class that stores  a superset of all information about a notification.

  // TODO(dimich): Consider merging all 4 types (Notification,
  // QueuedNotification, ProfileNotification and NotificationList::Notification)
  // into a single class.
  class ProfileNotification : public ImageDownloadsObserver {
   public:
    ProfileNotification(Profile* profile,
                        const Notification& notification,
                        message_center::MessageCenter* message_center);
    virtual ~ProfileNotification();

    void StartDownloads();

    // Overridden from ImageDownloadsObserver.
    virtual void OnDownloadsCompleted() OVERRIDE;

    Profile* profile() const { return profile_; }
    const Notification& notification() const { return notification_; }

    // Returns extension_id if the notification originates from an extension,
    // empty string otherwise.
    std::string GetExtensionId();

   private:
    // Weak, guaranteed not to be used after profile removal by parent class.
    Profile* profile_;
    Notification notification_;
    // Track the downloads for this notification so the notification can be
    // updated properly.
    scoped_ptr<ImageDownloads> downloads_;
  };

  scoped_ptr<message_center::MessageCenterTrayDelegate> tray_;
  message_center::MessageCenter* message_center_;  // Weak, global.

  // Use a map by notification_id since this mapping is the most often used.
  typedef std::map<std::string, ProfileNotification*> NotificationMap;
  NotificationMap profile_notifications_;

  scoped_ptr<MessageCenterSettingsController> settings_controller_;

  // Helpers that add/remove the notification from local map and MessageCenter.
  // They take ownership of profile_notification object.
  void AddProfileNotification(ProfileNotification* profile_notification);
  void RemoveProfileNotification(ProfileNotification* profile_notification,
                                 bool by_user);

  // Returns the ProfileNotification for the |id|, or NULL if no such
  // notification is found.
  ProfileNotification* FindProfileNotification(const std::string& id) const;

  DISALLOW_COPY_AND_ASSIGN(MessageCenterNotificationManager);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_MESSAGE_CENTER_NOTIFICATION_MANAGER_H_
