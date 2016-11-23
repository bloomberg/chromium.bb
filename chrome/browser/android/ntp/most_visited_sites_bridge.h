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
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_observer.h"
#include "components/ntp_tiles/most_visited_sites.h"

using ntp_tiles::NTPTilesVector;

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

  void AddOrRemoveBlacklistedUrl(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& j_url,
      jboolean add_url);
  void RecordPageImpression(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jintArray>& jtile_types,
      const base::android::JavaParamRef<jintArray>& jsources);
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

  class SupervisorBridge : public ntp_tiles::MostVisitedSitesSupervisor,
                           public SupervisedUserServiceObserver {
   public:
    explicit SupervisorBridge(Profile* profile);
    ~SupervisorBridge() override;

    void SetObserver(Observer* observer) override;
    bool IsBlocked(const GURL& url) override;
    std::vector<MostVisitedSitesSupervisor::Whitelist> whitelists() override;
    bool IsChildProfile() override;

    // SupervisedUserServiceObserver implementation.
    void OnURLFilterChanged() override;

   private:
    Profile* const profile_;
    Observer* supervisor_observer_;
    ScopedObserver<SupervisedUserService, SupervisedUserServiceObserver>
        register_observer_;
  };
  SupervisorBridge supervisor_;

  ntp_tiles::MostVisitedSites most_visited_;

  DISALLOW_COPY_AND_ASSIGN(MostVisitedSitesBridge);
};

#endif  // CHROME_BROWSER_ANDROID_NTP_MOST_VISITED_SITES_BRIDGE_H_
