// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_ui_manager_android.h"

#include "base/android/jni_string.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/profile_notification.h"
#include "jni/NotificationUIManager_jni.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;

// Called by the Java side when a notification event has been received, but the
// NotificationUIManager has not been initialized yet. Enforce initialization of
// the class.
static void InitializeNotificationUIManager(JNIEnv* env, jclass clazz) {
  g_browser_process->notification_ui_manager();
}

// static
NotificationUIManager* NotificationUIManager::Create(PrefService* local_state) {
  return new NotificationUIManagerAndroid();
}

NotificationUIManagerAndroid::NotificationUIManagerAndroid() {
  java_object_.Reset(
      Java_NotificationUIManager_create(
          AttachCurrentThread(),
          reinterpret_cast<intptr_t>(this),
          base::android::GetApplicationContext()));

  // TODO(peter): Synchronize notifications with the Java side.
}

NotificationUIManagerAndroid::~NotificationUIManagerAndroid() {
  Java_NotificationUIManager_destroy(AttachCurrentThread(),
                                     java_object_.obj());
}

bool NotificationUIManagerAndroid::OnNotificationClicked(
    JNIEnv* env, jobject java_object, jstring notification_id) {
  std::string id = ConvertJavaStringToUTF8(env, notification_id);

  auto iter = profile_notifications_.find(id);
  if (iter == profile_notifications_.end()) {
    // TODO(peter): Handle the "click" event for notifications which have
    // outlived the browser process that created them.
    return false;
  }

  const Notification& notification = iter->second->notification();
  notification.delegate()->Click();
  return true;
}

bool NotificationUIManagerAndroid::OnNotificationClosed(
    JNIEnv* env, jobject java_object, jstring notification_id) {
  std::string id = ConvertJavaStringToUTF8(env, notification_id);

  auto iter = profile_notifications_.find(id);
  if (iter == profile_notifications_.end())
    return false;

  const Notification& notification = iter->second->notification();
  notification.delegate()->Close(true /** by_user **/);
  return true;
}

void NotificationUIManagerAndroid::Add(const Notification& notification,
                                       Profile* profile) {
  if (Update(notification, profile))
    return;

  ProfileNotification* profile_notification =
      new ProfileNotification(profile, notification);

  // Takes ownership of |profile_notification|.
  AddProfileNotification(profile_notification);

  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jstring> id = ConvertUTF8ToJavaString(
      env, profile_notification->notification().id());
  ScopedJavaLocalRef<jstring> title = ConvertUTF16ToJavaString(
      env, notification.title());
  ScopedJavaLocalRef<jstring> body = ConvertUTF16ToJavaString(
      env, notification.message());
  ScopedJavaLocalRef<jstring> origin = ConvertUTF8ToJavaString(
      env, notification.origin_url().GetOrigin().spec());

  ScopedJavaLocalRef<jobject> icon;

  SkBitmap icon_bitmap = notification.icon().AsBitmap();
  if (!icon_bitmap.isNull())
    icon = gfx::ConvertToJavaBitmap(&icon_bitmap);

  int platform_id = Java_NotificationUIManager_displayNotification(
      env, java_object_.obj(), id.obj(), title.obj(), body.obj(), icon.obj(),
      origin.obj());

  std::string notification_id = profile_notification->notification().id();
  platform_notifications_[notification_id] = platform_id;

  notification.delegate()->Display();
}

bool NotificationUIManagerAndroid::Update(const Notification& notification,
                                          Profile* profile) {
  const base::string16& replace_id = notification.replace_id();
  if (replace_id.empty())
    return false;

  const GURL origin_url = notification.origin_url();
  DCHECK(origin_url.is_valid());

  for (const auto& iterator : profile_notifications_) {
    ProfileNotification* profile_notification = iterator.second;
    if (profile_notification->notification().replace_id() != replace_id ||
        profile_notification->notification().origin_url() != origin_url ||
        profile_notification->profile() != profile)
      continue;

    std::string notification_id = profile_notification->notification().id();

    // TODO(peter): Use Android's native notification replacement mechanism.
    // Right now FALSE is returned from this function even when we would be
    // able to update the notification, so that Add() creates a new one.
    RemoveProfileNotification(profile_notification);
    break;
  }

  return false;
}

const Notification* NotificationUIManagerAndroid::FindById(
    const std::string& delegate_id,
    ProfileID profile_id) const {
  std::string profile_notification_id =
      ProfileNotification::GetProfileNotificationId(delegate_id, profile_id);
  ProfileNotification* profile_notification =
      FindProfileNotification(profile_notification_id);
  if (!profile_notification)
    return 0;

  return &profile_notification->notification();
}

bool NotificationUIManagerAndroid::CancelById(const std::string& delegate_id,
                                              ProfileID profile_id) {
  std::string profile_notification_id =
      ProfileNotification::GetProfileNotificationId(delegate_id, profile_id);
  ProfileNotification* profile_notification =
      FindProfileNotification(profile_notification_id);
  if (!profile_notification)
    return false;

  RemoveProfileNotification(profile_notification);
  return true;
}

std::set<std::string>
NotificationUIManagerAndroid::GetAllIdsByProfileAndSourceOrigin(
    Profile* profile,
    const GURL& source) {
  // |profile| may be invalid, so no calls must be made based on the instance.
  std::set<std::string> delegate_ids;

  for (auto iterator : profile_notifications_) {
    ProfileNotification* profile_notification = iterator.second;
    if (profile_notification->notification().origin_url() == source &&
        profile_notification->profile() == profile)
      delegate_ids.insert(profile_notification->notification().id());
  }

  return delegate_ids;
}

bool NotificationUIManagerAndroid::CancelAllBySourceOrigin(
    const GURL& source_origin) {
  bool removed = true;

  for (auto iterator = profile_notifications_.begin();
       iterator != profile_notifications_.end();) {
    auto current_iterator = iterator++;

    ProfileNotification* profile_notification = current_iterator->second;
    if (profile_notification->notification().origin_url() == source_origin) {
      RemoveProfileNotification(profile_notification);
      removed = true;
    }
  }

  return removed;
}

bool NotificationUIManagerAndroid::CancelAllByProfile(ProfileID profile_id) {
  bool removed = true;

  for (auto iterator = profile_notifications_.begin();
       iterator != profile_notifications_.end();) {
    auto current_iterator = iterator++;

    ProfileNotification* profile_notification = current_iterator->second;
    ProfileID current_profile_id =
        NotificationUIManager::GetProfileID(profile_notification->profile());
    if (current_profile_id == profile_id) {
      RemoveProfileNotification(profile_notification);
      removed = true;
    }
  }

  return removed;
}

void NotificationUIManagerAndroid::CancelAll() {
  for (auto iterator : profile_notifications_) {
    ProfileNotification* profile_notification = iterator.second;

    PlatformCloseNotification(profile_notification);
    delete profile_notification;
  }

  profile_notifications_.clear();
}

bool NotificationUIManagerAndroid::RegisterNotificationUIManager(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void NotificationUIManagerAndroid::PlatformCloseNotification(
    ProfileNotification* profile_notification) {
  std::string id = profile_notification->notification().id();

  auto iterator = platform_notifications_.find(id);
  if (iterator == platform_notifications_.end())
    return;

  int platform_id = iterator->second;
  platform_notifications_.erase(id);

  Java_NotificationUIManager_closeNotification(AttachCurrentThread(),
                                               java_object_.obj(),
                                               platform_id);
}

void NotificationUIManagerAndroid::AddProfileNotification(
    ProfileNotification* profile_notification) {
  std::string id = profile_notification->notification().id();

  // Notification ids should be unique.
  DCHECK(profile_notifications_.find(id) == profile_notifications_.end());

  profile_notifications_[id] = profile_notification;
}

void NotificationUIManagerAndroid::RemoveProfileNotification(
    ProfileNotification* profile_notification) {
  PlatformCloseNotification(profile_notification);

  std::string id = profile_notification->notification().id();
  profile_notifications_.erase(id);
  delete profile_notification;
}

ProfileNotification* NotificationUIManagerAndroid::FindProfileNotification(
    const std::string& id) const {
  auto iter = profile_notifications_.find(id);
  if (iter == profile_notifications_.end())
    return nullptr;

  return iter->second;
}

