// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/downloads/offline_page_notification_bridge.h"

#include "base/android/context_utils.h"
#include "base/android/jni_string.h"
#include "chrome/browser/android/offline_pages/downloads/offline_page_download_bridge.h"
#include "jni/OfflinePageNotificationBridge_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::GetApplicationContext;

namespace {

base::android::ScopedJavaLocalRef<jstring> GetDisplayName(
    const offline_pages::DownloadUIItem& item) {
  JNIEnv* env = AttachCurrentThread();
  if (!item.title.empty())
    return ConvertUTF16ToJavaString(env, item.title);

  std::string host = item.url.host();
  if (!host.empty())
    return ConvertUTF8ToJavaString(env, host);

  return ConvertUTF8ToJavaString(env, item.url.spec());
}

}  // namespace

namespace offline_pages {
namespace android {

void OfflinePageNotificationBridge::NotifyDownloadSuccessful(
    const DownloadUIItem& item) {
  JNIEnv* env = AttachCurrentThread();
  Java_OfflinePageNotificationBridge_notifyDownloadSuccessful(
      env, GetApplicationContext(), ConvertUTF8ToJavaString(env, item.guid),
      ConvertUTF8ToJavaString(env, item.url.spec()), GetDisplayName(item));
}

void OfflinePageNotificationBridge::NotifyDownloadFailed(
    const DownloadUIItem& item) {
  JNIEnv* env = AttachCurrentThread();
  Java_OfflinePageNotificationBridge_notifyDownloadFailed(
      env, GetApplicationContext(), ConvertUTF8ToJavaString(env, item.guid),
      ConvertUTF8ToJavaString(env, item.url.spec()), GetDisplayName(item));
}

void OfflinePageNotificationBridge::NotifyDownloadProgress(
    const DownloadUIItem& item) {
  JNIEnv* env = AttachCurrentThread();
  Java_OfflinePageNotificationBridge_notifyDownloadProgress(
      env, GetApplicationContext(), ConvertUTF8ToJavaString(env, item.guid),
      ConvertUTF8ToJavaString(env, item.url.spec()),
      item.start_time.ToJavaTime(), item.download_progress_bytes,
      GetDisplayName(item));
}

void OfflinePageNotificationBridge::NotifyDownloadPaused(
    const DownloadUIItem& item) {
  JNIEnv* env = AttachCurrentThread();
  Java_OfflinePageNotificationBridge_notifyDownloadPaused(
      env, GetApplicationContext(), ConvertUTF8ToJavaString(env, item.guid),
      GetDisplayName(item));
}

void OfflinePageNotificationBridge::NotifyDownloadInterrupted(
    const DownloadUIItem& item) {
  JNIEnv* env = AttachCurrentThread();
  Java_OfflinePageNotificationBridge_notifyDownloadInterrupted(
      env, GetApplicationContext(), ConvertUTF8ToJavaString(env, item.guid),
      GetDisplayName(item));
}

void OfflinePageNotificationBridge::NotifyDownloadCanceled(
    const DownloadUIItem& item) {
  JNIEnv* env = AttachCurrentThread();
  Java_OfflinePageNotificationBridge_notifyDownloadCanceled(
      env, GetApplicationContext(), ConvertUTF8ToJavaString(env, item.guid));
}

void OfflinePageNotificationBridge::ShowDownloadingToast() {
  JNIEnv* env = AttachCurrentThread();
  Java_OfflinePageNotificationBridge_showDownloadingToast(
      env, GetApplicationContext());
}

}  // namespace android
}  // namespace offline_pages
