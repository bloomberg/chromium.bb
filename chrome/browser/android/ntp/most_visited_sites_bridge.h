// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_NTP_MOST_VISITED_SITES_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_NTP_MOST_VISITED_SITES_BRIDGE_H_

#include <jni.h>

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"

class Profile;

namespace ntp_tiles {
class MostVisitedSites;
}  // namespace ntp_tiles

// Provides the list of most visited sites and their thumbnails to Java.
class MostVisitedSitesBridge {
 public:
  explicit MostVisitedSitesBridge(Profile* profile);

  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  void SetObserver(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj,
                   const base::android::JavaParamRef<jobject>& j_observer,
                   jint num_sites);

  void AddOrRemoveBlacklistedUrl(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& j_url,
      jboolean add_url);
  void RecordPageImpression(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jintArray>& jtile_types,
      const base::android::JavaParamRef<jintArray>& jsources,
      const base::android::JavaParamRef<jobjectArray>& jtile_urls);
  void RecordOpenedMostVisitedItem(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint index,
      jint tile_type,
      jint source);

  // Registers JNI methods.
  static bool Register(JNIEnv* env);

 private:
  ~MostVisitedSitesBridge();

  class JavaObserver;
  std::unique_ptr<JavaObserver> java_observer_;

  std::unique_ptr<ntp_tiles::MostVisitedSites> most_visited_;

  DISALLOW_COPY_AND_ASSIGN(MostVisitedSitesBridge);
};

#endif  // CHROME_BROWSER_ANDROID_NTP_MOST_VISITED_SITES_BRIDGE_H_
