// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/downloads/offline_page_download_bridge.h"

#include <vector>

#include "base/android/jni_string.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "components/offline_pages/offline_page_item.h"
#include "content/public/browser/browser_context.h"
#include "jni/OfflinePageDownloadBridge_jni.h"
#include "net/base/filename_util.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace offline_pages {
namespace android {

namespace {

void ToJavaOfflinePageDownloadItemList(
    JNIEnv* env,
    jobject j_result_obj,
    const std::vector<OfflinePageItem>& offline_pages) {
  for (const OfflinePageItem& offline_page : offline_pages) {
    Java_OfflinePageDownloadBridge_createDownloadItemAndAddToList(
        env, j_result_obj,
        ConvertUTF8ToJavaString(env, offline_page.client_id.id).obj(),
        ConvertUTF8ToJavaString(env, offline_page.url.spec()).obj(),
        ConvertUTF8ToJavaString(env, offline_page.GetOfflineURL().spec()).obj(),
        offline_page.creation_time.ToJavaTime(), offline_page.file_size);
  }
}

ScopedJavaLocalRef<jobject> ToJavaOfflinePageDownloadItem(
    JNIEnv* env,
    const OfflinePageItem& offline_page) {
  return Java_OfflinePageDownloadBridge_createDownloadItem(
      env, ConvertUTF8ToJavaString(env, offline_page.client_id.id).obj(),
      ConvertUTF8ToJavaString(env, offline_page.url.spec()).obj(),
      ConvertUTF8ToJavaString(env, offline_page.GetOfflineURL().spec()).obj(),
      offline_page.creation_time.ToJavaTime(), offline_page.file_size);
}

}  // namespace

OfflinePageDownloadBridge::OfflinePageDownloadBridge(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    content::BrowserContext* browser_context) {}

OfflinePageDownloadBridge::~OfflinePageDownloadBridge() {}

// static
bool OfflinePageDownloadBridge::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void OfflinePageDownloadBridge::Destroy(JNIEnv* env,
                                        const JavaParamRef<jobject>&) {
  delete this;
}

void OfflinePageDownloadBridge::GetAllItems(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_result_obj) {
  DCHECK(j_result_obj);

  // Get the cached list of offline pages/download items here, and populate
  // result object.
  std::vector<OfflinePageItem> offline_pages;
  ToJavaOfflinePageDownloadItemList(env, j_result_obj, offline_pages);
}

ScopedJavaLocalRef<jobject> OfflinePageDownloadBridge::GetItemByGuid(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jstring>& j_guid) {
  OfflinePageItem offline_page;
  return ToJavaOfflinePageDownloadItem(env, offline_page);
}

void OfflinePageDownloadBridge::OnOfflinePageDownloadBridgeLoaded() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_OfflinePageDownloadBridge_downloadItemsLoaded(env, obj.obj());
}

void OfflinePageDownloadBridge::OnOfflinePageDownloadItemAdded(
    const OfflinePageItem& item) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_OfflinePageDownloadBridge_downloadItemAdded(
      env, obj.obj(), ToJavaOfflinePageDownloadItem(env, item).obj());
}

void OfflinePageDownloadBridge::OnOfflinePageDownloadItemDeleted(
    const std::string& guid) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_OfflinePageDownloadBridge_downloadItemDeleted(
      env, obj.obj(), ConvertUTF8ToJavaString(env, guid).obj());
}

void OfflinePageDownloadBridge::OnOfflinePageDownloadItemUpdated(
    const OfflinePageItem& item) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_OfflinePageDownloadBridge_downloadItemUpdated(
      env, obj.obj(), ToJavaOfflinePageDownloadItem(env, item).obj());
}

static jlong Init(JNIEnv* env,
                  const JavaParamRef<jobject>& obj,
                  const JavaParamRef<jobject>& j_profile) {
  return reinterpret_cast<jlong>(new OfflinePageDownloadBridge(
      env, obj, ProfileAndroid::FromProfileAndroid(j_profile)));
}

}  // namespace android
}  // namespace offline_pages
