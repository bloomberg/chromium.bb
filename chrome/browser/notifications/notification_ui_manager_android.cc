// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_ui_manager_android.h"

#include <utility>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/logging.h"
#include "base/pickle.h"
#include "chrome/browser/browser_process.h"
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

// The maximum size of the serialized pickle that carries a notification's meta
// information. Notifications carrying more data will be silently dropped - with
// an error being written to the log.
const int kMaximumSerializedNotificationSizeBytes = 1024 * 1024;

// Persistent notifications are likely to outlive the browser process they were
// created by on Android. In order to be able to re-surface the notification
// when the user interacts with them, all relevant notification data needs to
// be serialized with the notification itself.
//
// In the near future, as more features get added to Chrome's notification
// implementation, this will be done by storing the persistent notification data
// in a database. However, to support launching Chrome for Android from a
// notification event until that exists, serialize the data in the Intent.
//
// TODO(peter): Move towards storing notification data in a database rather than
//              as a serialized Intent extra.

scoped_ptr<Pickle> SerializePersistentNotification(
    const content::PlatformNotificationData& notification_data,
    const GURL& origin,
    int64 service_worker_registration_id) {
  scoped_ptr<Pickle> pickle(new Pickle);

  // content::PlatformNotificationData
  pickle->WriteString16(notification_data.title);
  pickle->WriteInt(static_cast<int>(notification_data.direction));
  pickle->WriteString(notification_data.lang);
  pickle->WriteString16(notification_data.body);
  pickle->WriteString(notification_data.tag);
  pickle->WriteString(notification_data.icon.spec());
  pickle->WriteBool(notification_data.silent);

  // The origin which is displaying the notification.
  pickle->WriteString(origin.spec());

  // The Service Worker registration this notification is associated with.
  pickle->WriteInt64(service_worker_registration_id);

  if (pickle->size() > kMaximumSerializedNotificationSizeBytes)
    return nullptr;

  return pickle.Pass();
}

bool UnserializePersistentNotification(
    const Pickle& pickle,
    content::PlatformNotificationData* notification_data,
    GURL* origin,
    int64* service_worker_registration_id) {
  DCHECK(notification_data && origin && service_worker_registration_id);
  PickleIterator iterator(pickle);

  std::string icon_url, origin_url;
  int direction_value;

  // Unpack content::PlatformNotificationData.
  if (!iterator.ReadString16(&notification_data->title) ||
      !iterator.ReadInt(&direction_value) ||
      !iterator.ReadString(&notification_data->lang) ||
      !iterator.ReadString16(&notification_data->body) ||
      !iterator.ReadString(&notification_data->tag) ||
      !iterator.ReadString(&icon_url) ||
      !iterator.ReadBool(&notification_data->silent)) {
    return false;
  }

  notification_data->direction =
      static_cast<content::PlatformNotificationData::NotificationDirection>(
          direction_value);
  notification_data->icon = GURL(icon_url);

  // Unpack the origin which displayed this notification.
  if (!iterator.ReadString(&origin_url))
    return false;

  *origin = GURL(origin_url);

  // Unpack the Service Worker registration id.
  if (!iterator.ReadInt64(service_worker_registration_id))
    return false;

  return true;
}

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

  // TODO(peter): Synchronize notifications with the Java side.
}

NotificationUIManagerAndroid::~NotificationUIManagerAndroid() {
  Java_NotificationUIManager_destroy(AttachCurrentThread(),
                                     java_object_.obj());
}

bool NotificationUIManagerAndroid::OnNotificationClicked(
    JNIEnv* env,
    jobject java_object,
    jstring notification_id,
    jint platform_id,
    jbyteArray serialized_notification_data) {
  std::string id = ConvertJavaStringToUTF8(env, notification_id);

  auto iter = profile_notifications_.find(id);
  if (iter != profile_notifications_.end()) {
    const Notification& notification = iter->second->notification();
    notification.delegate()->Click();

    return true;
  }

  // If the Notification were not found, it may be a persistent notification
  // that outlived the Chrome browser process. In this case, try to
  // unserialize the notification's serialized data and trigger the click
  // event manually.

  std::vector<uint8> bytes;
  base::android::JavaByteArrayToByteVector(env, serialized_notification_data,
                                           &bytes);
  if (!bytes.size())
    return false;

  content::PlatformNotificationData notification_data;
  GURL origin;
  int64 service_worker_registration_id;

  Pickle pickle(reinterpret_cast<const char*>(&bytes[0]), bytes.size());
  if (!UnserializePersistentNotification(pickle, &notification_data, &origin,
                                         &service_worker_registration_id)) {
    return false;
  }

  // Store the platform id, tag, and origin of this notification so that it can
  // be closed.
  RegeneratedNotificationInfo notification_info(notification_data.tag,
                                                platform_id, origin.spec());
  regenerated_notification_infos_.insert(std::make_pair(id, notification_info));

  PlatformNotificationServiceImpl* service =
      PlatformNotificationServiceImpl::GetInstance();

  // TODO(peter): Rather than assuming that the last used profile is the
  // appropriate one for this notification, the used profile should be
  // stored as part of the notification's data. See https://crbug.com/437574.
  service->OnPersistentNotificationClick(
      ProfileManager::GetLastUsedProfile(),
      service_worker_registration_id,
      id,
      origin,
      notification_data,
      base::Bind(&OnEventDispatchComplete));

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
  RemoveProfileNotification(iter->second);
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

  ScopedJavaLocalRef<jstring> tag =
      ConvertUTF8ToJavaString(env, notification.tag());
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

  ScopedJavaLocalRef<jbyteArray> notification_data;
  if (true /** is_persistent_notification **/) {
    PersistentNotificationDelegate* delegate =
        static_cast<PersistentNotificationDelegate*>(notification.delegate());
    scoped_ptr<Pickle> pickle = SerializePersistentNotification(
        delegate->notification_data(),
        notification.origin_url(),
        delegate->service_worker_registration_id());

    if (!pickle) {
      LOG(ERROR) <<
          "Unable to serialize the notification, payload too large (max 1MB).";
      RemoveProfileNotification(profile_notification);
      return;
    }

    notification_data = base::android::ToJavaByteArray(
        env, static_cast<const uint8*>(pickle->data()), pickle->size());
  }

  int platform_id = Java_NotificationUIManager_displayNotification(
      env,
      java_object_.obj(),
      tag.obj(),
      id.obj(),
      title.obj(),
      body.obj(),
      icon.obj(),
      origin.obj(),
      notification.silent(),
      notification_data.obj());

  RegeneratedNotificationInfo notification_info(
      notification.tag(), platform_id,
      notification.origin_url().GetOrigin().spec());
  regenerated_notification_infos_.insert(std::make_pair(
      profile_notification->notification().id(), notification_info));

  notification.delegate()->Display();
}

bool NotificationUIManagerAndroid::Update(const Notification& notification,
                                          Profile* profile) {
  const std::string& tag = notification.tag();
  if (tag.empty())
    return false;

  const GURL origin_url = notification.origin_url();
  DCHECK(origin_url.is_valid());

  for (const auto& iterator : profile_notifications_) {
    ProfileNotification* profile_notification = iterator.second;
    if (profile_notification->notification().tag() != tag ||
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
  if (profile_notification) {
    RemoveProfileNotification(profile_notification);
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
  auto iterator = regenerated_notification_infos_.find(notification_id);
  if (iterator == regenerated_notification_infos_.end())
    return;

  RegeneratedNotificationInfo notification_info = iterator->second;
  int platform_id = notification_info.platform_id;
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jstring> tag =
      ConvertUTF8ToJavaString(env, notification_info.tag);
  ScopedJavaLocalRef<jstring> origin =
      ConvertUTF8ToJavaString(env, notification_info.origin);

  regenerated_notification_infos_.erase(notification_id);

  Java_NotificationUIManager_closeNotification(
      env, java_object_.obj(), tag.obj(), platform_id, origin.obj());
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
  std::string notification_id = profile_notification->notification().id();
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

