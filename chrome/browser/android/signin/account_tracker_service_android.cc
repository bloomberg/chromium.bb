// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/signin/account_tracker_service_android.h"

#include "base/android/jni_array.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "components/signin/core/browser/account_info.h"
#include "jni/AccountTrackerService_jni.h"

AccountTrackerServiceAndroid::AccountTrackerServiceAndroid(JNIEnv* env,
                                                           jobject obj) {
  java_account_tracker_service_.Reset(env, obj);
}

void AccountTrackerServiceAndroid::SeedAccountsInfo(JNIEnv* env,
                                                    jobject obj,
                                                    jobjectArray gaiaIds,
                                                    jobjectArray accountNames) {
  std::vector<std::string> gaia_ids;
  std::vector<std::string> account_names;
  base::android::AppendJavaStringArrayToStringVector(env, gaiaIds, &gaia_ids);
  base::android::AppendJavaStringArrayToStringVector(env, accountNames,
                                                     &account_names);
  DCHECK_EQ(gaia_ids.size(), account_names.size());

  DVLOG(1) << "AccountTrackerServiceAndroid::SeedAccountsInfo: "
           << " number of accounts " << gaia_ids.size();
  Profile* profile = ProfileManager::GetActiveUserProfile();
  AccountTrackerService* account_tracker_service_ =
      AccountTrackerServiceFactory::GetForProfile(profile);

  for (unsigned int i = 0; i < gaia_ids.size(); i++) {
    account_tracker_service_->SeedAccountInfo(gaia_ids[i], account_names[i]);
  }
}

static jlong Init(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  AccountTrackerServiceAndroid* account_tracker_service_android =
      new AccountTrackerServiceAndroid(env, obj);
  return reinterpret_cast<intptr_t>(account_tracker_service_android);
}

// static
bool AccountTrackerServiceAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
