// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/android/downloads/offline_page_notification_bridge.h"

#include "base/android/jni_string.h"
#include "jni/OfflinePageNotificationBridge_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;

namespace {

base::android::ScopedJavaLocalRef<jstring> GetDisplayName(
    const OfflineItem& item) {
  JNIEnv* env = AttachCurrentThread();
  if (!item.title.empty())
    return ConvertUTF8ToJavaString(env, item.title);

  std::string host = item.page_url.host();
  if (!host.empty())
    return ConvertUTF8ToJavaString(env, host);

  return ConvertUTF8ToJavaString(env, item.page_url.spec());
}

}  // namespace

namespace offline_pages {
namespace android {

void OfflinePageNotificationBridge::NotifyDownloadSuccessful(
    const OfflineItem& item) {
  JNIEnv* env = AttachCurrentThread();
  Java_OfflinePageNotificationBridge_notifyDownloadSuccessful(
      env, ConvertUTF8ToJavaString(env, item.id.id),
      ConvertUTF8ToJavaString(env, item.page_url.spec()), GetDisplayName(item));
}

void OfflinePageNotificationBridge::NotifyDownloadFailed(
    const OfflineItem& item) {
  JNIEnv* env = AttachCurrentThread();
  Java_OfflinePageNotificationBridge_notifyDownloadFailed(
      env, ConvertUTF8ToJavaString(env, item.id.id),
      ConvertUTF8ToJavaString(env, item.page_url.spec()), GetDisplayName(item));
}

void OfflinePageNotificationBridge::NotifyDownloadProgress(
    const OfflineItem& item) {
  JNIEnv* env = AttachCurrentThread();
  Java_OfflinePageNotificationBridge_notifyDownloadProgress(
      env, ConvertUTF8ToJavaString(env, item.id.id),
      ConvertUTF8ToJavaString(env, item.page_url.spec()),
      item.creation_time.ToJavaTime(), item.received_bytes,
      GetDisplayName(item));
}

void OfflinePageNotificationBridge::NotifyDownloadPaused(
    const OfflineItem& item) {
  JNIEnv* env = AttachCurrentThread();
  Java_OfflinePageNotificationBridge_notifyDownloadPaused(
      env, ConvertUTF8ToJavaString(env, item.id.id), GetDisplayName(item));
}

void OfflinePageNotificationBridge::NotifyDownloadInterrupted(
    const OfflineItem& item) {
  JNIEnv* env = AttachCurrentThread();
  Java_OfflinePageNotificationBridge_notifyDownloadInterrupted(
      env, ConvertUTF8ToJavaString(env, item.id.id), GetDisplayName(item));
}

void OfflinePageNotificationBridge::NotifyDownloadCanceled(
    const OfflineItem& item) {
  JNIEnv* env = AttachCurrentThread();
  Java_OfflinePageNotificationBridge_notifyDownloadCanceled(
      env, ConvertUTF8ToJavaString(env, item.id.id));
}

void OfflinePageNotificationBridge::ShowDownloadingToast() {
  JNIEnv* env = AttachCurrentThread();
  Java_OfflinePageNotificationBridge_showDownloadingToast(env);
}

}  // namespace android
}  // namespace offline_pages
