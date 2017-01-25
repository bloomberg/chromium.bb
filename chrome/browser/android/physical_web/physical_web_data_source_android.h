// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PHYSICAL_WEB_PHYSICAL_WEB_DATA_SOURCE_ANDROID_H_
#define CHROME_BROWSER_ANDROID_PHYSICAL_WEB_PHYSICAL_WEB_DATA_SOURCE_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "components/physical_web/data_source/physical_web_data_source_impl.h"

// A container for Physical Web metadata. This is primarily a wrapper for a
// physical_web::MetadataList so we can append to it over JNI.
class PhysicalWebCollection {
 public:
  PhysicalWebCollection();
  ~PhysicalWebCollection();

  void AppendMetadataItem(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& j_request_url,
      jdouble distance_estimate,
      jlong scan_timestamp,
      const base::android::JavaParamRef<jstring>& j_site_url,
      const base::android::JavaParamRef<jstring>& j_icon_url,
      const base::android::JavaParamRef<jstring>& j_title,
      const base::android::JavaParamRef<jstring>& j_description,
      const base::android::JavaParamRef<jstring>& j_group_id);

  // Returns the metadata list and transfers ownership of the list to the
  // caller. Call only once.
  std::unique_ptr<physical_web::MetadataList> GetMetadataList();

 private:
  std::unique_ptr<physical_web::MetadataList> metadata_list_;
  bool accessed_once_;

  DISALLOW_COPY_AND_ASSIGN(PhysicalWebCollection);
};

class PhysicalWebDataSourceAndroid
    : public physical_web::PhysicalWebDataSourceImpl {
 public:
  PhysicalWebDataSourceAndroid();
  ~PhysicalWebDataSourceAndroid() override;

  static bool RegisterPhysicalWebDataSource(JNIEnv* env);

  void Initialize();

  void StartDiscovery(bool network_request_enabled) override;
  void StopDiscovery() override;

  std::unique_ptr<physical_web::MetadataList> GetMetadataList() override;
  bool HasUnresolvedDiscoveries() override;

  void OnFound(JNIEnv* env,
               const base::android::JavaParamRef<jobject>& obj,
               const base::android::JavaParamRef<jstring>& j_url);
  void OnLost(JNIEnv* env,
              const base::android::JavaParamRef<jobject>& obj,
              const base::android::JavaParamRef<jstring>& j_url);
  void OnDistanceChanged(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj,
                         const base::android::JavaParamRef<jstring>& j_url,
                         jdouble distance_estimate);

 private:
  // A reference to the Java UrlManager singleton.
  base::android::ScopedJavaGlobalRef<jobject> url_manager_;

  DISALLOW_COPY_AND_ASSIGN(PhysicalWebDataSourceAndroid);
};

#endif  // CHROME_BROWSER_ANDROID_PHYSICAL_WEB_PHYSICAL_WEB_DATA_SOURCE_ANDROID_H_
