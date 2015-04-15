// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_ui_manager_android.h"

#include <utility>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/persistent_notification_delegate.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/browser/notifications/profile_notification.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/common/persistent_notification_status.h"
#include "content/public/common/platform_notification_data.h"
#include "jni/NotificationUIManager_jni.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;

namespace {

// Called when the "notificationclick" event in the Service Worker has finished
// executing for a notification that was created in a previous instance of the
// browser.
void OnEventDispatchComplete(content::PersistentNotificationStatus status) {
  // TODO(peter): Add UMA statistics based on |status|.
  // TODO(peter): Decide if we want to forcefully shut down the browser process
  // if we're confident it was created for delivering this event.
}

}  // namespace

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
}

NotificationUIManagerAndroid::~NotificationUIManagerAndroid() {
  Java_NotificationUIManager_destroy(AttachCurrentThread(),
                                     java_object_.obj());
}

bool NotificationUIManagerAndroid::OnNotificationClicked(
    JNIEnv* env,
    jobject java_object,
    jlong persistent_notification_id,
    jstring java_origin,
    jstring java_tag) {
  GURL origin(ConvertJavaStringToUTF8(env, java_origin));
  std::string tag = ConvertJavaStringToUTF8(env, java_tag);

  regenerated_notification_infos_[persistent_notification_id] =
      std::make_pair(origin.spec(), tag);

  // TODO(peter): Rather than assuming that the last used profile is the
  // appropriate one for this notification, the used profile should be
  // stored as part of the notification's data. See https://crbug.com/437574.
  PlatformNotificationServiceImpl::GetInstance()->OnPersistentNotificationClick(
      ProfileManager::GetLastUsedProfile(),
      persistent_notification_id,
      origin,
      base::Bind(&OnEventDispatchComplete));

  return true;
}

bool NotificationUIManagerAndroid::OnNotificationClosed(
    JNIEnv* env,
    jobject java_object,
    jlong persistent_notification_id,
    jstring java_origin,
    jstring java_tag) {
  // TODO(peter): Implement handling when a notification has been closed. The
  // notification database has to reflect this in its own state.
  return true;
}

void NotificationUIManagerAndroid::Add(const Notification& notification,
                                       Profile* profile) {
  // If the given notification is replacing an older one, drop its associated
  // profile notification object without closing the platform notification.
  // We'll use the native Android system to perform a smoother replacement.
  ProfileNotification* notification_to_replace =
      FindNotificationToReplace(notification, profile);
  if (notification_to_replace)
    RemoveProfileNotification(notification_to_replace, false /* close */);

  ProfileNotification* profile_notification =
      new ProfileNotification(profile, notification);

  // Takes ownership of |profile_notification|.
  AddProfileNotification(profile_notification);

  JNIEnv* env = AttachCurrentThread();

  PersistentNotificationDelegate* delegate =
      static_cast<PersistentNotificationDelegate*>(notification.delegate());
  DCHECK(delegate);

  int64_t persistent_notification_id = delegate->persistent_notification_id();
  GURL origin_url(notification.origin_url().GetOrigin());

  ScopedJavaLocalRef<jstring> origin = ConvertUTF8ToJavaString(
      env, origin_url.spec());
  ScopedJavaLocalRef<jstring> tag =
      ConvertUTF8ToJavaString(env, notification.tag());
  ScopedJavaLocalRef<jstring> title = ConvertUTF16ToJavaString(
      env, notification.title());
  ScopedJavaLocalRef<jstring> body = ConvertUTF16ToJavaString(
      env, notification.message());

  ScopedJavaLocalRef<jobject> icon;

  SkBitmap icon_bitmap = notification.icon().AsBitmap();
  if (!icon_bitmap.isNull())
    icon = gfx::ConvertToJavaBitmap(&icon_bitmap);

  Java_NotificationUIManager_displayNotification(
      env,
      java_object_.obj(),
      persistent_notification_id,
      origin.obj(),
      tag.obj(),
      title.obj(),
      body.obj(),
      icon.obj(),
      notification.silent());

  regenerated_notification_infos_[persistent_notification_id] =
      std::make_pair(origin_url.spec(), notification.tag());

  notification.delegate()->Display();
}

bool NotificationUIManagerAndroid::Update(const Notification& notification,
                                          Profile* profile) {
  // This method is currently only called from extensions and local discovery,
  // both of which are not supported on Android.
  NOTIMPLEMENTED();
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
  if (profile_notification) {
    RemoveProfileNotification(profile_notification, true /* close */);
    return true;
  }

  // On Android, it's still possible that the notification can be closed in case
  // the platform Id is known, even if no delegate exists. This happens when the
  // browser process is started because of interaction with a Notification.
  PlatformCloseNotification(delegate_id);
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
      RemoveProfileNotification(profile_notification, true /* close */);
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
      RemoveProfileNotification(profile_notification, true /* close */);
      removed = true;
    }
  }

  return removed;
}

void NotificationUIManagerAndroid::CancelAll() {
  for (auto iterator : profile_notifications_) {
    ProfileNotification* profile_notification = iterator.second;

    PlatformCloseNotification(profile_notification->notification().id());
    delete profile_notification;
  }

  profile_notifications_.clear();
}

bool NotificationUIManagerAndroid::RegisterNotificationUIManager(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void NotificationUIManagerAndroid::PlatformCloseNotification(
    const std::string& notification_id) {
  int64_t persistent_notification_id = 0;
  if (!base::StringToInt64(notification_id, &persistent_notification_id))
    return;

  const auto iterator =
      regenerated_notification_infos_.find(persistent_notification_id);
  if (iterator == regenerated_notification_infos_.end())
    return;

  RegeneratedNotificationInfo notification_info = iterator->second;
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jstring> origin =
      ConvertUTF8ToJavaString(env, notification_info.first);
  ScopedJavaLocalRef<jstring> tag =
      ConvertUTF8ToJavaString(env, notification_info.second);

  regenerated_notification_infos_.erase(iterator);

  Java_NotificationUIManager_closeNotification(env,
                                               java_object_.obj(),
                                               persistent_notification_id,
                                               origin.obj(),
                                               tag.obj());
}

void NotificationUIManagerAndroid::AddProfileNotification(
    ProfileNotification* profile_notification) {
  std::string id = profile_notification->notification().id();

  // Notification ids should be unique.
  DCHECK(profile_notifications_.find(id) == profile_notifications_.end());

  profile_notifications_[id] = profile_notification;
}

void NotificationUIManagerAndroid::RemoveProfileNotification(
    ProfileNotification* profile_notification, bool close) {
  std::string notification_id = profile_notification->notification().id();
  if (close)
    PlatformCloseNotification(notification_id);

  profile_notifications_.erase(notification_id);
  delete profile_notification;
}

ProfileNotification* NotificationUIManagerAndroid::FindProfileNotification(
    const std::string& id) const {
  auto iter = profile_notifications_.find(id);
  if (iter == profile_notifications_.end())
    return nullptr;

  return iter->second;
}

ProfileNotification* NotificationUIManagerAndroid::FindNotificationToReplace(
    const Notification& notification, Profile* profile) const {
  const std::string& tag = notification.tag();
  if (tag.empty())
    return nullptr;

  const GURL origin_url = notification.origin_url();
  DCHECK(origin_url.is_valid());

  for (const auto& iterator : profile_notifications_) {
    ProfileNotification* profile_notification = iterator.second;
    if (profile_notification->notification().tag() == tag ||
        profile_notification->notification().origin_url() == origin_url ||
        profile_notification->profile() == profile) {
      return profile_notification;
    }
  }
  return nullptr;
}
