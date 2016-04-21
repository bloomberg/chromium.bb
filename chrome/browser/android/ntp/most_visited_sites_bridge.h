// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_NTP_MOST_VISITED_SITES_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_NTP_MOST_VISITED_SITES_BRIDGE_H_

#include <jni.h>
#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "chrome/browser/android/ntp/most_visited_sites.h"

class Profile;

// Provides the list of most visited sites and their thumbnails to Java.
class MostVisitedSitesBridge {
 public:
  explicit MostVisitedSitesBridge(Profile* profile);

  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  void SetMostVisitedURLsObserver(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_observer,
      jint num_sites);

  void GetURLThumbnail(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& url,
      const base::android::JavaParamRef<jobject>& j_callback);
  void AddOrRemoveBlacklistedUrl(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& j_url,
      jboolean add_url);
  void RecordTileTypeMetrics(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jintArray>& jtile_types);
  void RecordOpenedMostVisitedItem(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint index,
      jint tile_type);

  // Registers JNI methods.
  static bool Register(JNIEnv* env);

 private:
  ~MostVisitedSitesBridge();

  class Observer;
  std::unique_ptr<Observer> observer_;

  MostVisitedSites most_visited_;

  DISALLOW_COPY_AND_ASSIGN(MostVisitedSitesBridge);
};

#endif  // CHROME_BROWSER_ANDROID_NTP_MOST_VISITED_SITES_BRIDGE_H_
