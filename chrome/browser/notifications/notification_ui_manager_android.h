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
//
// The Android notification manager only operates reliably on notifications
// that were opened or interacted with since the last restart of Chrome.
class NotificationUIManagerAndroid : public NotificationUIManager {
 public:
  NotificationUIManagerAndroid();
  ~NotificationUIManagerAndroid() override;

  // Called by the Java implementation when the notification has been clicked.
  bool OnNotificationClicked(JNIEnv* env,
                             jobject java_object,
                             jlong persistent_notification_id,
                             jstring java_origin,
                             jstring java_tag);

  // Called by the Java implementation when the notification has been closed.
  bool OnNotificationClosed(JNIEnv* env,
                            jobject java_object,
                            jlong persistent_notification_id,
                            jstring java_origin,
                            jstring java_tag);

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
  void PlatformCloseNotification(const std::string& notification_id);

  // Adds |profile_notification| to a private local map. The map takes ownership
  // of the notification object.
  void AddProfileNotification(ProfileNotification* profile_notification);

  // Erases |profile_notification| from the private local map and deletes it.
  // If |close| is true, also closes the notification.
  void RemoveProfileNotification(ProfileNotification* profile_notification,
                                 bool close);

  // Returns the ProfileNotification for the |id|, or NULL if no such
  // notification is found.
  ProfileNotification* FindProfileNotification(const std::string& id) const;

  // Returns the ProfileNotification for |profile| that has the same origin and
  // tag as |notification|. Returns NULL if no valid match is found.
  ProfileNotification* FindNotificationToReplace(
      const Notification& notification, Profile* profile) const;

  // Map from a notification id to the associated ProfileNotification*.
  std::map<std::string, ProfileNotification*> profile_notifications_;

  // Pair containing the information necessary in order to enable closing
  // notifications that were not created by this instance of the manager: the
  // notification's origin and tag. This list may not contain the notifications
  // that have not been interacted with since the last restart of Chrome.
  using RegeneratedNotificationInfo = std::pair<std::string, std::string>;

  // Map from persistent notification id to RegeneratedNotificationInfo.
  std::map<int64_t, RegeneratedNotificationInfo>
      regenerated_notification_infos_;

  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  DISALLOW_COPY_AND_ASSIGN(NotificationUIManagerAndroid);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_UI_MANAGER_ANDROID_H_
