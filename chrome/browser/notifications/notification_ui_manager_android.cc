// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_ui_manager_android.h"

#include <utility>
#include <vector>

#include "base/android/context_utils.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/persistent_notification_delegate.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/common/persistent_notification_status.h"
#include "content/public/common/platform_notification_data.h"
#include "jni/NotificationUIManager_jni.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;

namespace {

ScopedJavaLocalRef<jobjectArray> ConvertToJavaBitmaps(
    const std::vector<message_center::ButtonInfo>& buttons) {
  std::vector<SkBitmap> skbitmaps;
  for (const message_center::ButtonInfo& button : buttons)
    skbitmaps.push_back(button.icon.AsBitmap());

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jclass> clazz =
      base::android::GetClass(env, "android/graphics/Bitmap");
  jobjectArray array = env->NewObjectArray(skbitmaps.size(), clazz.obj(),
                                           nullptr /* initialElement */);
  base::android::CheckException(env);

  for (size_t i = 0; i < skbitmaps.size(); ++i) {
    if (!skbitmaps[i].drawsNothing()) {
      env->SetObjectArrayElement(
          array, i, gfx::ConvertToJavaBitmap(&(skbitmaps[i])).obj());
    }
  }

  return ScopedJavaLocalRef<jobjectArray>(env, array);
}

}  // namespace

// Called by the Java side when a notification event has been received, but the
// NotificationUIManager has not been initialized yet. Enforce initialization of
// the class.
static void InitializeNotificationUIManager(JNIEnv* env,
                                            const JavaParamRef<jclass>& clazz) {
  g_browser_process->notification_ui_manager();
}

// static
NotificationPlatformBridge* NotificationPlatformBridge::Create() {
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

void NotificationUIManagerAndroid::OnNotificationClicked(
    JNIEnv* env,
    const JavaParamRef<jobject>& java_object,
    jlong persistent_notification_id,
    const JavaParamRef<jstring>& java_origin,
    const JavaParamRef<jstring>& java_profile_id,
    jboolean incognito,
    const JavaParamRef<jstring>& java_tag,
    jint action_index) {
  GURL origin(ConvertJavaStringToUTF8(env, java_origin));
  std::string tag = ConvertJavaStringToUTF8(env, java_tag);
  std::string profile_id = ConvertJavaStringToUTF8(env, java_profile_id);

  regenerated_notification_infos_[persistent_notification_id] =
      std::make_pair(origin.spec(), tag);

  PlatformNotificationServiceImpl::GetInstance()
      ->ProcessPersistentNotificationOperation(
          PlatformNotificationServiceImpl::NOTIFICATION_CLICK, profile_id,
          incognito, origin, persistent_notification_id, action_index);
}

void NotificationUIManagerAndroid::OnNotificationClosed(
    JNIEnv* env,
    const JavaParamRef<jobject>& java_object,
    jlong persistent_notification_id,
    const JavaParamRef<jstring>& java_origin,
    const JavaParamRef<jstring>& java_profile_id,
    jboolean incognito,
    const JavaParamRef<jstring>& java_tag,
    jboolean by_user) {
  GURL origin(ConvertJavaStringToUTF8(env, java_origin));
  std::string profile_id = ConvertJavaStringToUTF8(env, java_profile_id);
  std::string tag = ConvertJavaStringToUTF8(env, java_tag);

  // The notification was closed by the platform, so clear all local state.
  regenerated_notification_infos_.erase(persistent_notification_id);
  PlatformNotificationServiceImpl::GetInstance()
      ->ProcessPersistentNotificationOperation(
          PlatformNotificationServiceImpl::NOTIFICATION_CLOSE, profile_id,
          incognito, origin, persistent_notification_id, -1);
}

void NotificationUIManagerAndroid::Display(const std::string& notification_id,
                                           const std::string& profile_id,
                                           bool incognito,
                                           const Notification& notification) {
  JNIEnv* env = AttachCurrentThread();

  // The Android notification UI manager only supports Web Notifications, which
  // have a PersistentNotificationDelegate. The persistent id of the
  // notification is exposed through it's interface.
  //
  // TODO(peter): When content/ passes a message_center::Notification to the
  // chrome/ layer, the persistent notification id should be captured as a
  // property on that object instead, making this cast unnecessary.
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

  ScopedJavaLocalRef<jobject> notification_icon;
  SkBitmap notification_icon_bitmap = notification.icon().AsBitmap();
  if (!notification_icon_bitmap.drawsNothing())
    notification_icon = gfx::ConvertToJavaBitmap(&notification_icon_bitmap);

  ScopedJavaLocalRef<jobject> badge;
  SkBitmap badge_bitmap = notification.small_image().AsBitmap();
  if (!badge_bitmap.drawsNothing())
    badge = gfx::ConvertToJavaBitmap(&badge_bitmap);

  std::vector<base::string16> action_titles_vector;
  for (const message_center::ButtonInfo& button : notification.buttons())
    action_titles_vector.push_back(button.title);
  ScopedJavaLocalRef<jobjectArray> action_titles =
      base::android::ToJavaArrayOfStrings(env, action_titles_vector);

  ScopedJavaLocalRef<jobjectArray> action_icons =
      ConvertToJavaBitmaps(notification.buttons());

  ScopedJavaLocalRef<jintArray> vibration_pattern =
      base::android::ToJavaIntArray(env, notification.vibration_pattern());

  ScopedJavaLocalRef<jstring> j_profile_id =
      ConvertUTF8ToJavaString(env, profile_id);

  Java_NotificationUIManager_displayNotification(
      env, java_object_.obj(), persistent_notification_id, origin.obj(),
      j_profile_id.obj(), incognito, tag.obj(), title.obj(), body.obj(),
      notification_icon.obj(), badge.obj(), vibration_pattern.obj(),
      notification.timestamp().ToJavaTime(), notification.renotify(),
      notification.silent(), action_titles.obj(), action_icons.obj());

  regenerated_notification_infos_[persistent_notification_id] =
      std::make_pair(origin_url.spec(), notification.tag());

  notification.delegate()->Display();
}

void NotificationUIManagerAndroid::Close(const std::string& profile_id,
                                         const std::string& notification_id) {
  int64_t persistent_notification_id = 0;

  // TODO(peter): Use the |delegate_id| directly when notification ids are being
  // generated by content/ instead of us.
  if (!base::StringToInt64(notification_id, &persistent_notification_id)) {
    LOG(WARNING) << "Unable to decode notification_id " << notification_id;
    return;
  }

  const auto iterator =
      regenerated_notification_infos_.find(persistent_notification_id);
  if (iterator == regenerated_notification_infos_.end())
    return;

  const RegeneratedNotificationInfo& notification_info = iterator->second;

  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jstring> origin =
      ConvertUTF8ToJavaString(env, notification_info.first);
  ScopedJavaLocalRef<jstring> tag =
      ConvertUTF8ToJavaString(env, notification_info.second);
  ScopedJavaLocalRef<jstring> j_profile_id =
      ConvertUTF8ToJavaString(env, profile_id);

  regenerated_notification_infos_.erase(iterator);

  Java_NotificationUIManager_closeNotification(
      env, java_object_.obj(), j_profile_id.obj(), persistent_notification_id,
      origin.obj(), tag.obj());
}

bool NotificationUIManagerAndroid::GetDisplayed(
    const std::string& profile_id,
    bool incognito,
    std::set<std::string>* notifications) const {
  // TODO(miguelg): This can actually be implemented for M+
  return false;
}

bool NotificationUIManagerAndroid::SupportsNotificationCenter() const {
  return true;
}

bool NotificationUIManagerAndroid::RegisterNotificationPlatformBridge(
    JNIEnv* env) {
  return RegisterNativesImpl(env);
}
