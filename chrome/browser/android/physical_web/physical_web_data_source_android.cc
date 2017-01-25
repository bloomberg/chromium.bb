// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/physical_web/physical_web_data_source_android.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/browser_process.h"
#include "jni/UrlManager_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

PhysicalWebCollection::PhysicalWebCollection()
    : metadata_list_(base::MakeUnique<physical_web::MetadataList>()),
      accessed_once_(false) {}

PhysicalWebCollection::~PhysicalWebCollection() {}

void PhysicalWebCollection::AppendMetadataItem(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& j_request_url,
    jdouble distance_estimate,
    jlong scan_timestamp,
    const JavaParamRef<jstring>& j_site_url,
    const JavaParamRef<jstring>& j_icon_url,
    const JavaParamRef<jstring>& j_title,
    const JavaParamRef<jstring>& j_description,
    const JavaParamRef<jstring>& j_group_id) {
  metadata_list_->emplace_back();
  physical_web::Metadata& metadata = metadata_list_->back();
  metadata.scanned_url = GURL(ConvertJavaStringToUTF8(j_request_url));
  metadata.resolved_url = GURL(ConvertJavaStringToUTF8(j_site_url));
  metadata.icon_url = GURL(ConvertJavaStringToUTF8(j_icon_url));
  metadata.title = ConvertJavaStringToUTF8(j_title);
  metadata.description = ConvertJavaStringToUTF8(j_description);
  metadata.group_id = ConvertJavaStringToUTF8(j_group_id);
  metadata.distance_estimate = distance_estimate;
  metadata.scan_timestamp = base::Time::FromJavaTime(scan_timestamp);
}

std::unique_ptr<physical_web::MetadataList>
    PhysicalWebCollection::GetMetadataList() {
  DCHECK(!accessed_once_);
  accessed_once_ = true;
  return std::move(metadata_list_);
}

PhysicalWebDataSourceAndroid::PhysicalWebDataSourceAndroid() {
  Initialize();
}

PhysicalWebDataSourceAndroid::~PhysicalWebDataSourceAndroid() {}

void PhysicalWebDataSourceAndroid::Initialize() {
  JNIEnv* env = AttachCurrentThread();

  // Cache a reference to the singleton instance of UrlManager.
  url_manager_.Reset(Java_UrlManager_getInstance(env));
  DCHECK(url_manager_.obj());
}

void PhysicalWebDataSourceAndroid::StartDiscovery(
    bool network_request_enabled) {
  // On Android, scanning is started and stopped through the Java layer.
  NOTREACHED();
}

void PhysicalWebDataSourceAndroid::StopDiscovery() {
  // On Android, scanning is started and stopped through the Java layer.
  NOTREACHED();
}

std::unique_ptr<physical_web::MetadataList>
    PhysicalWebDataSourceAndroid::GetMetadataList() {
  JNIEnv* env = AttachCurrentThread();

  auto pw_collection = base::MakeUnique<PhysicalWebCollection>();
  Java_UrlManager_getPwCollection(env, url_manager_.obj(),
                                  reinterpret_cast<long>(pw_collection.get()));

  return pw_collection->GetMetadataList();
}

bool PhysicalWebDataSourceAndroid::HasUnresolvedDiscoveries() {
  NOTIMPLEMENTED();
  return false;
}

void PhysicalWebDataSourceAndroid::OnFound(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jstring>& j_url) {
  NotifyOnFound(GURL(ConvertJavaStringToUTF8(env, j_url)));
}

void PhysicalWebDataSourceAndroid::OnLost(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jstring>& j_url) {
  NotifyOnLost(GURL(ConvertJavaStringToUTF8(env, j_url)));
}

void PhysicalWebDataSourceAndroid::OnDistanceChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jstring>& j_url,
    jdouble distance_estimate) {
  NotifyOnDistanceChanged(GURL(ConvertJavaStringToUTF8(env, j_url)),
                          distance_estimate);
}

// static
bool PhysicalWebDataSourceAndroid::RegisterPhysicalWebDataSource(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

static jlong Init(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  physical_web::PhysicalWebDataSource* data_source =
      g_browser_process->GetPhysicalWebDataSource();
  return reinterpret_cast<intptr_t>(data_source);
}
