// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/downloads/offline_page_notification_bridge.h"

#include "base/android/context_utils.h"
#include "base/android/jni_string.h"
#include "chrome/browser/android/offline_pages/downloads/offline_page_download_bridge.h"
#include "jni/OfflinePageNotificationBridge_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::GetApplicationContext;

namespace offline_pages {
namespace android {

void OfflinePageNotificationBridge::NotifyDownloadSuccessful(
    const DownloadUIItem& item) {
  JNIEnv* env = AttachCurrentThread();
  Java_OfflinePageNotificationBridge_notifyDownloadSuccessful(
      env, GetApplicationContext(), ConvertUTF8ToJavaString(env, item.guid),
      ConvertUTF8ToJavaString(env, item.url.spec()));
}

void OfflinePageNotificationBridge::NotifyDownloadFailed(
    const DownloadUIItem& item) {
  JNIEnv* env = AttachCurrentThread();
  Java_OfflinePageNotificationBridge_notifyDownloadFailed(
      env, GetApplicationContext(), ConvertUTF8ToJavaString(env, item.guid),
      ConvertUTF8ToJavaString(env, item.url.spec()));
}

void OfflinePageNotificationBridge::NotifyDownloadProgress(
    const DownloadUIItem& item) {
  JNIEnv* env = AttachCurrentThread();
  Java_OfflinePageNotificationBridge_notifyDownloadProgress(
      env, GetApplicationContext(), ConvertUTF8ToJavaString(env, item.guid),
      ConvertUTF8ToJavaString(env, item.url.spec()),
      item.start_time.ToJavaTime());
}

void OfflinePageNotificationBridge::NotifyDownloadPaused(
    const DownloadUIItem& item) {
  JNIEnv* env = AttachCurrentThread();
  Java_OfflinePageNotificationBridge_notifyDownloadPaused(
      env, GetApplicationContext(), ConvertUTF8ToJavaString(env, item.guid));
}

void OfflinePageNotificationBridge::NotifyDownloadInterrupted(
    const DownloadUIItem& item) {
  JNIEnv* env = AttachCurrentThread();
  Java_OfflinePageNotificationBridge_notifyDownloadInterrupted(
      env, GetApplicationContext(), ConvertUTF8ToJavaString(env, item.guid));
}

void OfflinePageNotificationBridge::NotifyDownloadCanceled(
    const DownloadUIItem& item) {
  JNIEnv* env = AttachCurrentThread();
  Java_OfflinePageNotificationBridge_notifyDownloadCanceled(
      env, GetApplicationContext(), ConvertUTF8ToJavaString(env, item.guid));
}

}  // namespace android
}  // namespace offline_pages
