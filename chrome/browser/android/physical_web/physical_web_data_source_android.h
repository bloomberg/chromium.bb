// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PHYSICAL_WEB_PHYSICAL_WEB_DATA_SOURCE_ANDROID_H_
#define CHROME_BROWSER_ANDROID_PHYSICAL_WEB_PHYSICAL_WEB_DATA_SOURCE_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "components/physical_web/data_source/physical_web_data_source_impl.h"

namespace base {
class ListValue;
}

class PhysicalWebDataSourceAndroid : public PhysicalWebDataSourceImpl {
 public:
  PhysicalWebDataSourceAndroid();
  ~PhysicalWebDataSourceAndroid() override;

  static bool RegisterPhysicalWebDataSource(JNIEnv* env);

  void Initialize();

  void StartDiscovery(bool network_request_enabled) override;
  void StopDiscovery() override;

  std::unique_ptr<base::ListValue> GetMetadata() override;
  bool HasUnresolvedDiscoveries() override;

 private:
  // A reference to the Java UrlManager singleton.
  base::android::ScopedJavaGlobalRef<jobject> url_manager_;

  DISALLOW_COPY_AND_ASSIGN(PhysicalWebDataSourceAndroid);
};

#endif  // CHROME_BROWSER_ANDROID_PHYSICAL_WEB_PHYSICAL_WEB_DATA_SOURCE_ANDROID_H_
