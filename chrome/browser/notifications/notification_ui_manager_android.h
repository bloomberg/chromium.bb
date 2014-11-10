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
  void OnNotificationClicked(JNIEnv* env,
                             jobject java_object,
                             jstring notification_id);

  // Called by the Java implementation when a notification has been closed.
  void OnNotificationClosed(JNIEnv* env,
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
  // Closes the Notification as displayed on the Android system.
  void PlatformCloseNotification(ProfileNotification* profile_notification);

  // Helpers that add/remove the notification from local map.
  // The local map takes ownership of profile_notification object.
  void AddProfileNotification(ProfileNotification* profile_notification);
  void RemoveProfileNotification(ProfileNotification* profile_notification);

  // Returns the ProfileNotification for the |id|, or NULL if no such
  // notification is found.
  ProfileNotification* FindProfileNotification(const std::string& id) const;

  // Map from a notification id to the associated ProfileNotification*.
  std::map<std::string, ProfileNotification*> profile_notifications_;

  // Map from notification id to the associated platform Id.
  std::map<std::string, int> platform_notifications_;

  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  DISALLOW_COPY_AND_ASSIGN(NotificationUIManagerAndroid);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_UI_MANAGER_ANDROID_H_
