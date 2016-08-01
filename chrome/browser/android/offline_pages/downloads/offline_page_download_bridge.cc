// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/downloads/offline_page_download_bridge.h"

#include <vector>

#include "base/android/jni_string.h"
#include "chrome/browser/android/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "components/offline_pages/downloads/download_ui_item.h"
#include "components/offline_pages/offline_page_model.h"
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

void ToJavaOfflinePageDownloadItemList(JNIEnv* env,
                                       jobject j_result_obj,
                                       const DownloadUIItemsMap& items_map) {
  for (const auto& guid_item_pair : items_map) {
    const DownloadUIItem& item = *(guid_item_pair.second.get());
    Java_OfflinePageDownloadBridge_createDownloadItemAndAddToList(
        env, j_result_obj, ConvertUTF8ToJavaString(env, item.guid).obj(),
        ConvertUTF8ToJavaString(env, item.url.spec()).obj(),
        ConvertUTF8ToJavaString(env, item.target_path.value()).obj(),
        item.start_time.ToJavaTime(), item.total_bytes);
  }
}

ScopedJavaLocalRef<jobject> ToJavaOfflinePageDownloadItem(
    JNIEnv* env,
    const DownloadUIItem& item) {
  return Java_OfflinePageDownloadBridge_createDownloadItem(
      env, ConvertUTF8ToJavaString(env, item.guid).obj(),
      ConvertUTF8ToJavaString(env, item.url.spec()).obj(),
      ConvertUTF8ToJavaString(env, item.target_path.value()).obj(),
      item.start_time.ToJavaTime(), item.total_bytes);
}
}  // namespace

OfflinePageDownloadBridge::OfflinePageDownloadBridge(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    DownloadUIAdapter* download_ui_adapter)
    : download_ui_adapter_(download_ui_adapter) {
  DCHECK(download_ui_adapter_);
  download_ui_adapter_->AddObserver(this);
}

OfflinePageDownloadBridge::~OfflinePageDownloadBridge() {}

// static
bool OfflinePageDownloadBridge::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void OfflinePageDownloadBridge::Destroy(JNIEnv* env,
                                        const JavaParamRef<jobject>&) {
  download_ui_adapter_->RemoveObserver(this);
  delete this;
}

void OfflinePageDownloadBridge::GetAllItems(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_result_obj) {
  DCHECK(j_result_obj);

  const DownloadUIItemsMap& items_map = download_ui_adapter_->GetAllItems();
  ToJavaOfflinePageDownloadItemList(env, j_result_obj, items_map);
}

ScopedJavaLocalRef<jobject> OfflinePageDownloadBridge::GetItemByGuid(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jstring>& j_guid) {
  std::string guid = ConvertJavaStringToUTF8(env, j_guid);
  const DownloadUIItem* item = download_ui_adapter_->GetItem(guid);
  return ToJavaOfflinePageDownloadItem(env, *item);
}

void OfflinePageDownloadBridge::ItemsLoaded() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_OfflinePageDownloadBridge_downloadItemsLoaded(env, obj.obj());
}

void OfflinePageDownloadBridge::ItemAdded(const DownloadUIItem& item) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_OfflinePageDownloadBridge_downloadItemAdded(
      env, obj.obj(), ToJavaOfflinePageDownloadItem(env, item).obj());
}

void OfflinePageDownloadBridge::ItemDeleted(const std::string& guid) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_OfflinePageDownloadBridge_downloadItemDeleted(
      env, obj.obj(), ConvertUTF8ToJavaString(env, guid).obj());
}

void OfflinePageDownloadBridge::ItemUpdated(const DownloadUIItem& item) {
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
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  OfflinePageModel* offline_page_model =
      OfflinePageModelFactory::GetForBrowserContext(profile);

  DownloadUIAdapter* adapter =
      DownloadUIAdapter::FromOfflinePageModel(offline_page_model);

  return reinterpret_cast<jlong>(
      new OfflinePageDownloadBridge(env, obj, adapter));
}

}  // namespace android
}  // namespace offline_pages
