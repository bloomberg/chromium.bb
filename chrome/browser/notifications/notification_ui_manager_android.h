// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_UI_MANAGER_ANDROID_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_UI_MANAGER_ANDROID_H_

#include <jni.h>
#include <stdint.h>
#include <map>
#include <set>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "chrome/browser/notifications/notification_platform_bridge.h"

// Implementation of the NotificationPlatformBridge for Android, which defers to
// the Android framework for displaying notifications.
//
// Prior to Android Marshmellow, Android did not have the ability to retrieve
// the notifications currently showing for an app without a rather intrusive
// permission. The GetDisplayed() method may return false because of this.
//
// The Android implementation *is* reliable for adding and canceling
// single notifications based on their delegate id. Finally, events for
// persistent Web Notifications will be forwarded directly to the associated
// event handlers, as such notifications may outlive the browser process on
// Android.
class NotificationUIManagerAndroid : public NotificationPlatformBridge {
 public:
  NotificationUIManagerAndroid();
  ~NotificationUIManagerAndroid() override;

  // Called by the Java implementation when the notification has been clicked.
  void OnNotificationClicked(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& java_object,
      jlong persistent_notification_id,
      const base::android::JavaParamRef<jstring>& java_origin,
      const base::android::JavaParamRef<jstring>& java_profile_id,
      jboolean incognito,
      const base::android::JavaParamRef<jstring>& java_tag,
      jint action_index);

  // Called by the Java implementation when the notification has been closed.
  void OnNotificationClosed(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& java_object,
      jlong persistent_notification_id,
      const base::android::JavaParamRef<jstring>& java_origin,
      const base::android::JavaParamRef<jstring>& java_profile_id,
      jboolean incognito,
      const base::android::JavaParamRef<jstring>& java_tag,
      jboolean by_user);

  // NotificationPlatformBridge implementation.
  void Display(const std::string& notification_id,
               const std::string& profile_id,
               bool incognito,
               const Notification& notification) override;
  void Close(const std::string& profile_id,
             const std::string& notification_id) override;
  bool GetDisplayed(const std::string& profile_id,
                    bool incognito,
                    std::set<std::string>* notifications) const override;
  bool SupportsNotificationCenter() const override;

  static bool RegisterNotificationPlatformBridge(JNIEnv* env);

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
