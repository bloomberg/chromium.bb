// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_content_provider_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/supervised_user/supervised_user_interstitial.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "jni/SupervisedUserContentProvider_jni.h"

using base::android::JavaRef;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::AttachCurrentThread;

namespace {

class UrlFilterObserver : public SupervisedUserURLFilter::Observer {
 public:
  UrlFilterObserver(JNIEnv* env,
                    const ScopedJavaGlobalRef<jobject>& java_content_provider)
      : java_content_provider_(java_content_provider) {}

  virtual ~UrlFilterObserver() {}

 private:
  void OnSiteListUpdated() override {
    Java_SupervisedUserContentProvider_onSupervisedUserFilterUpdated(
        AttachCurrentThread(), java_content_provider_.obj());
  }
  ScopedJavaGlobalRef<jobject> java_content_provider_;
};

}  // namespace

static jlong CreateSupervisedUserContentProvider(
    JNIEnv* env,
    const JavaParamRef<jobject>& caller) {
  return reinterpret_cast<intptr_t>(
      new SupervisedUserContentProvider(env, caller));
}

SupervisedUserContentProvider::SupervisedUserContentProvider(
    JNIEnv* env,
    const JavaParamRef<jobject>& caller)
    : profile_(ProfileManager::GetLastUsedProfile()),
      java_content_provider_(env, caller),
      weak_factory_(this) {
  if (profile_->IsSupervised()) {
    SupervisedUserService* supervised_user_service =
        SupervisedUserServiceFactory::GetForProfile(profile_);
    SupervisedUserURLFilter* url_filter =
        supervised_user_service->GetURLFilterForUIThread();
    url_filter->AddObserver(new UrlFilterObserver(env, java_content_provider_));
  }
}

SupervisedUserContentProvider::~SupervisedUserContentProvider() {}

void SupervisedUserContentProvider::ShouldProceed(
    JNIEnv* env,
    const JavaParamRef<jobject>& caller,
    const JavaParamRef<jobject>& query_result_jobj,
    const JavaParamRef<jstring>& url) {
  if (!profile_->IsSupervised()) {
    // User isn't supervised
    Java_SupervisedUserQueryReply_onQueryComplete(env, query_result_jobj.obj());
    return;
  }
  SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(profile_);
  SupervisedUserURLFilter* url_filter =
      supervised_user_service->GetURLFilterForUIThread();
  url_filter->GetFilteringBehaviorForURLWithAsyncChecks(
      GURL(base::android::ConvertJavaStringToUTF16(env, url)),
      base::Bind(&SupervisedUserContentProvider::OnQueryComplete,
                 weak_factory_.GetWeakPtr(),
                 ScopedJavaGlobalRef<jobject>(env, query_result_jobj.obj())));
}

void SupervisedUserContentProvider::RequestInsert(
    JNIEnv* env,
    const JavaParamRef<jobject>& caller,
    const JavaParamRef<jobject>& insert_result_jobj,
    const JavaParamRef<jstring>& url) {
  if (!profile_->IsSupervised())
    return;
  SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(profile_);
  supervised_user_service->AddURLAccessRequest(
      GURL(base::android::ConvertJavaStringToUTF16(env, url)),
      base::Bind(&SupervisedUserContentProvider::OnInsertRequestSendComplete,
                 weak_factory_.GetWeakPtr(),
                 ScopedJavaGlobalRef<jobject>(env, insert_result_jobj.obj())));
}

void SupervisedUserContentProvider::OnQueryComplete(
    ScopedJavaGlobalRef<jobject> query_reply_jobj,
    SupervisedUserURLFilter::FilteringBehavior behavior,
    supervised_user_error_page::FilteringBehaviorReason reason,
    bool /* uncertain */) {
  if (behavior != SupervisedUserURLFilter::BLOCK) {
    Java_SupervisedUserQueryReply_onQueryComplete(AttachCurrentThread(),
                                                  query_reply_jobj.obj());
  } else {
    JNIEnv* env = AttachCurrentThread();
    SupervisedUserService* service =
        SupervisedUserServiceFactory::GetForProfile(profile_);
    Java_SupervisedUserQueryReply_onQueryFailed(
        env, query_reply_jobj.obj(), reason, service->AccessRequestsEnabled(),
        profile_->IsChild(),
        base::android::ConvertUTF8ToJavaString(
            env, profile_->GetPrefs()->GetString(
                     prefs::kSupervisedUserCustodianProfileImageURL))
            .obj(),
        base::android::ConvertUTF8ToJavaString(
            env, profile_->GetPrefs()->GetString(
                     prefs::kSupervisedUserSecondCustodianProfileImageURL))
            .obj(),
        base::android::ConvertUTF8ToJavaString(env, service->GetCustodianName())
            .obj(),
        base::android::ConvertUTF8ToJavaString(
            env, service->GetCustodianEmailAddress())
            .obj(),
        base::android::ConvertUTF8ToJavaString(
            env, service->GetSecondCustodianName())
            .obj(),
        base::android::ConvertUTF8ToJavaString(
            env, service->GetSecondCustodianEmailAddress())
            .obj());
  }
}

void SupervisedUserContentProvider::SetFilterForTesting(JNIEnv* env,
                                                        jobject caller) {
  if (!profile_->IsSupervised())
    return;
  SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(profile_);
  SupervisedUserURLFilter* url_filter =
      supervised_user_service->GetURLFilterForUIThread();
  url_filter->SetDefaultFilteringBehavior(SupervisedUserURLFilter::BLOCK);
}

void SupervisedUserContentProvider::OnInsertRequestSendComplete(
    ScopedJavaGlobalRef<jobject> insert_reply_jobj,
    bool sent_ok) {
  Java_SupervisedUserInsertReply_onInsertRequestSendComplete(
      AttachCurrentThread(), insert_reply_jobj.obj(), sent_ok);
}

bool SupervisedUserContentProvider::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
