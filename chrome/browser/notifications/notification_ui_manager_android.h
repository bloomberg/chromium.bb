// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_UI_MANAGER_ANDROID_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_UI_MANAGER_ANDROID_H_

#include <jni.h>
#include <map>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/notifications/notification_ui_manager.h"

// Implementation of the Notification UI Manager for Android, which defers to
// the Android framework for displaying notifications.
//
// Android does not support getting the notifications currently shown by an
// app without very intrusive permissions, which means that it's not possible
// to provide reliable implementations for methods which have to iterate over
// the existing notifications. Because of this, these have not been implemented.
//
// The UI manager implementation *is* reliable for adding and canceling single
// notifications based on their delegate id. Finally, events for persistent Web
// Notifications will be forwarded directly to the associated event handlers,
// as such notifications may outlive the browser process on Android.
class NotificationUIManagerAndroid : public NotificationUIManager {
 public:
  NotificationUIManagerAndroid();
  ~NotificationUIManagerAndroid() override;

  // Called by the Java implementation when the notification has been clicked.
  bool OnNotificationClicked(JNIEnv* env,
                             jobject java_object,
                             jlong persistent_notification_id,
                             jstring java_origin,
                             jstring java_tag,
                             jint action_index);

  // Called by the Java implementation when the notification has been closed.
  bool OnNotificationClosed(JNIEnv* env,
                            jobject java_object,
                            jlong persistent_notification_id,
                            jstring java_origin,
                            jstring java_tag,
                            jboolean by_user);

  // NotificationUIManager implementation.
  void Add(const Notification& notification, Profile* profile) override;
  bool Update(const Notification& notification,
              Profile* profile) override;
  const Notification* FindById(const std::string& delegate_id,
                               ProfileID profile_id) const override;
  bool CancelById(const std::string& delegate_id,
                  ProfileID profile_id) override;
  std::set<std::string> GetAllIdsByProfileAndSourceOrigin(
      ProfileID profile_id,
      const GURL& source) override;
  std::set<std::string> GetAllIdsByProfile(ProfileID profile_id) override;
  bool CancelAllBySourceOrigin(const GURL& source_origin) override;
  bool CancelAllByProfile(ProfileID profile_id) override;
  void CancelAll() override;

  static bool RegisterNotificationUIManager(JNIEnv* env);

 private:
  // Pair containing the information necessary in order to enable closing
  // notifications that were not created by this instance of the manager: the
  // notification's origin and tag. This list may not contain the notifications
  // that have not been interacted with since the last restart of Chrome.
  using RegeneratedNotificationInfo = std::pair<std::string, std::string>;

  // Mapping of a persistent notification id to renegerated notification info.
  // TODO(peter): Remove this map once notification delegate ids for Web
  // notifications are created by the content/ layer.
  std::map<int64_t, RegeneratedNotificationInfo>
      regenerated_notification_infos_;

  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  DISALLOW_COPY_AND_ASSIGN(NotificationUIManagerAndroid);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_UI_MANAGER_ANDROID_H_
