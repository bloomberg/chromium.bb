// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_UI_MANAGER_ANDROID_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_UI_MANAGER_ANDROID_H_

#include <jni.h>
#include <map>
#include <set>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/notifications/notification_ui_manager.h"

class ProfileNotification;

// Implementation of the Notification UI Manager for Android, which defers to
// the Android framework for displaying notifications.
class NotificationUIManagerAndroid : public NotificationUIManager {
 public:
  NotificationUIManagerAndroid();
  ~NotificationUIManagerAndroid() override;

  // Called by the Java implementation when a notification has been clicked on.
  bool OnNotificationClicked(JNIEnv* env,
                             jobject java_object,
                             jstring notification_id,
                             jint platform_id,
                             jbyteArray serialized_notification_data);

  // Called by the Java implementation when a notification has been closed.
  bool OnNotificationClosed(JNIEnv* env,
                            jobject java_object,
                            jstring notification_id);

  // NotificationUIManager implementation;
  void Add(const Notification& notification, Profile* profile) override;
  bool Update(const Notification& notification,
              Profile* profile) override;
  const Notification* FindById(const std::string& delegate_id,
                               ProfileID profile_id) const override;
  bool CancelById(const std::string& delegate_id,
                  ProfileID profile_id) override;
  std::set<std::string> GetAllIdsByProfileAndSourceOrigin(Profile* profile,
                                                          const GURL& source)
      override;
  bool CancelAllBySourceOrigin(const GURL& source_origin) override;
  bool CancelAllByProfile(ProfileID profile_id) override;
  void CancelAll() override;

  static bool RegisterNotificationUIManager(JNIEnv* env);

 private:
  // Holds all information required to show or close a platform notification.
  struct RegeneratedNotificationInfo {
    RegeneratedNotificationInfo(const std::string& tag,
                                int platform_id,
                                const std::string& origin)
        : tag(tag), platform_id(platform_id), origin(origin) {}
    std::string tag;
    int platform_id;
    std::string origin;
  };

  // Closes the Notification as displayed on the Android system.
  void PlatformCloseNotification(const std::string& notification_id);

  // Helpers that add/remove the notification from local map.
  // The local map takes ownership of profile_notification object.
  void AddProfileNotification(ProfileNotification* profile_notification);
  void RemoveProfileNotification(ProfileNotification* profile_notification);

  // Returns the ProfileNotification for the |id|, or NULL if no such
  // notification is found.
  ProfileNotification* FindProfileNotification(const std::string& id) const;

  // Map from a notification id to the associated ProfileNotification*.
  std::map<std::string, ProfileNotification*> profile_notifications_;

  // Map from notification id to RegeneratedNotificationInfo.
  std::map<std::string, RegeneratedNotificationInfo>
      regenerated_notification_infos_;

  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  DISALLOW_COPY_AND_ASSIGN(NotificationUIManagerAndroid);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_UI_MANAGER_ANDROID_H_
