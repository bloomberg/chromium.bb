// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_BRIDGE_H_

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"

namespace explore_sites {

/**
 * Bridge between C++ and Java for fetching and decoding URLs and images.
 */
class ExploreSitesBridge {
 public:
  explicit ExploreSitesBridge(const base::android::JavaRef<jobject>& j_profile);
  void Destroy(JNIEnv*, const base::android::JavaParamRef<jobject>& obj);

  void GetNtpCategories(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& url,
      const base::android::JavaParamRef<jobject>& j_result_obj,
      const base::android::JavaParamRef<jobject>& j_callback_obj);

 private:
  virtual ~ExploreSitesBridge();

  base::WeakPtrFactory<ExploreSitesBridge> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(ExploreSitesBridge);
};

}  // namespace explore_sites

#endif  // CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_BRIDGE_H_
