// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/browsing_data/browsing_data_bridge.h"

#include <jni.h>
#include <stddef.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/jni_weak_ref.h"
#include "base/metrics/histogram_macros.h"
#include "base/scoped_observer.h"
#include "base/values.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/engagement/important_sites_util.h"
#include "chrome/browser/history/web_history_service_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/browsing_data/core/history_notice_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/browsing_data_remover.h"
#include "jni/BrowsingDataBridge_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using base::android::ScopedJavaGlobalRef;
using content::BrowsingDataRemover;

namespace {

const size_t kMaxImportantSites = 5;

Profile* GetOriginalProfile() {
  return ProfileManager::GetActiveUserProfile()->GetOriginalProfile();
}

// Merges |task_count| BrowsingDataRemover completion callbacks and redirects
// them back into Java.
class ClearBrowsingDataObserver : public BrowsingDataRemover::Observer {
 public:
  // |obj| is expected to be the object passed into ClearBrowsingData(); e.g. a
  // ChromePreference.
  ClearBrowsingDataObserver(JNIEnv* env,
                            jobject obj,
                            BrowsingDataRemover* browsing_data_remover,
                            int task_count)
      : task_count_(task_count),
        weak_chrome_native_preferences_(env, obj),
        observer_(this) {
    DCHECK_GT(task_count, 0);
    observer_.Add(browsing_data_remover);
  }

  void OnBrowsingDataRemoverDone() override {
    DCHECK(task_count_);
    if (--task_count_)
      return;

    // We delete ourselves when done.
    std::unique_ptr<ClearBrowsingDataObserver> auto_delete(this);

    JNIEnv* env = AttachCurrentThread();
    if (weak_chrome_native_preferences_.get(env).is_null())
      return;

    Java_BrowsingDataBridge_browsingDataCleared(
        env, weak_chrome_native_preferences_.get(env));
  }

 private:
  int task_count_;
  JavaObjectWeakGlobalRef weak_chrome_native_preferences_;
  ScopedObserver<BrowsingDataRemover, BrowsingDataRemover::Observer> observer_;
};

}  // namespace

bool RegisterBrowsingDataBridge(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

static void ClearBrowsingData(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jintArray>& data_types,
    jint time_period,
    const JavaParamRef<jobjectArray>& jexcluding_domains,
    const JavaParamRef<jintArray>& jexcluding_domain_reasons,
    const JavaParamRef<jobjectArray>& jignoring_domains,
    const JavaParamRef<jintArray>& jignoring_domain_reasons) {
  BrowsingDataRemover* browsing_data_remover =
      content::BrowserContext::GetBrowsingDataRemover(GetOriginalProfile());

  std::vector<int> data_types_vector;
  base::android::JavaIntArrayToIntVector(env, data_types, &data_types_vector);

  int remove_mask = 0;
  for (const int data_type : data_types_vector) {
    switch (static_cast<browsing_data::BrowsingDataType>(data_type)) {
      case browsing_data::BrowsingDataType::HISTORY:
        remove_mask |= ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY;
        break;
      case browsing_data::BrowsingDataType::CACHE:
        remove_mask |= BrowsingDataRemover::DATA_TYPE_CACHE;
        break;
      case browsing_data::BrowsingDataType::COOKIES:
        remove_mask |= BrowsingDataRemover::DATA_TYPE_COOKIES;
        remove_mask |= ChromeBrowsingDataRemoverDelegate::DATA_TYPE_SITE_DATA;
        break;
      case browsing_data::BrowsingDataType::PASSWORDS:
        remove_mask |= ChromeBrowsingDataRemoverDelegate::DATA_TYPE_PASSWORDS;
        break;
      case browsing_data::BrowsingDataType::FORM_DATA:
        remove_mask |= ChromeBrowsingDataRemoverDelegate::DATA_TYPE_FORM_DATA;
        break;
      case browsing_data::BrowsingDataType::BOOKMARKS:
        // Bookmarks are deleted separately on the Java side.
        NOTREACHED();
        break;
      case browsing_data::BrowsingDataType::SITE_SETTINGS:
        remove_mask |=
            ChromeBrowsingDataRemoverDelegate::DATA_TYPE_CONTENT_SETTINGS;
        break;
      case browsing_data::BrowsingDataType::NUM_TYPES:
        NOTREACHED();
    }
  }
  std::vector<std::string> excluding_domains;
  std::vector<int32_t> excluding_domain_reasons;
  std::vector<std::string> ignoring_domains;
  std::vector<int32_t> ignoring_domain_reasons;
  base::android::AppendJavaStringArrayToStringVector(
      env, jexcluding_domains.obj(), &excluding_domains);
  base::android::JavaIntArrayToIntVector(env, jexcluding_domain_reasons.obj(),
                                         &excluding_domain_reasons);
  base::android::AppendJavaStringArrayToStringVector(
      env, jignoring_domains.obj(), &ignoring_domains);
  base::android::JavaIntArrayToIntVector(env, jignoring_domain_reasons.obj(),
                                         &ignoring_domain_reasons);
  std::unique_ptr<content::BrowsingDataFilterBuilder> filter_builder(
      content::BrowsingDataFilterBuilder::Create(
          content::BrowsingDataFilterBuilder::BLACKLIST));
  for (const std::string& domain : excluding_domains) {
    filter_builder->AddRegisterableDomain(domain);
  }

  if (!excluding_domains.empty() || !ignoring_domains.empty()) {
    ImportantSitesUtil::RecordBlacklistedAndIgnoredImportantSites(
        GetOriginalProfile(), excluding_domains, excluding_domain_reasons,
        ignoring_domains, ignoring_domain_reasons);
  }

  // Delete the types protected by Important Sites with a filter,
  // and the rest completely.
  int filterable_mask =
      remove_mask &
      ChromeBrowsingDataRemoverDelegate::IMPORTANT_SITES_DATA_TYPES;
  int nonfilterable_mask =
      remove_mask &
      ~ChromeBrowsingDataRemoverDelegate::IMPORTANT_SITES_DATA_TYPES;

  // ClearBrowsingDataObserver deletes itself when |browsing_data_remover| is
  // done with both removal tasks.
  ClearBrowsingDataObserver* observer = new ClearBrowsingDataObserver(
      env, obj, browsing_data_remover, 2 /* tasks_count */);

  browsing_data::TimePeriod period =
      static_cast<browsing_data::TimePeriod>(time_period);
  browsing_data::RecordDeletionForPeriod(period);

  if (filterable_mask) {
    browsing_data_remover->RemoveWithFilterAndReply(
        browsing_data::CalculateBeginDeleteTime(period),
        browsing_data::CalculateEndDeleteTime(period), filterable_mask,
        BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
        std::move(filter_builder), observer);
  } else {
    // Make sure |observer| doesn't wait for the filtered task.
    observer->OnBrowsingDataRemoverDone();
  }

  if (nonfilterable_mask) {
    browsing_data_remover->RemoveAndReply(
        browsing_data::CalculateBeginDeleteTime(period),
        browsing_data::CalculateEndDeleteTime(period), nonfilterable_mask,
        BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB, observer);
  } else {
    // Make sure |observer| doesn't wait for the non-filtered task.
    observer->OnBrowsingDataRemoverDone();
  }
}

static void ShowNoticeAboutOtherFormsOfBrowsingHistory(
    const JavaRef<jobject>& listener,
    bool show) {
  JNIEnv* env = AttachCurrentThread();
  UMA_HISTOGRAM_BOOLEAN(
      "History.ClearBrowsingData.HistoryNoticeShownInFooterWhenUpdated", show);
  if (!show)
    return;
  Java_OtherFormsOfBrowsingHistoryListener_showNoticeAboutOtherFormsOfBrowsingHistory(
      env, listener);
}

static void EnableDialogAboutOtherFormsOfBrowsingHistory(
    const JavaRef<jobject>& listener,
    bool enabled) {
  JNIEnv* env = AttachCurrentThread();
  if (!enabled)
    return;
  Java_OtherFormsOfBrowsingHistoryListener_enableDialogAboutOtherFormsOfBrowsingHistory(
      env, listener);
}

static void RequestInfoAboutOtherFormsOfBrowsingHistory(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& listener) {
  // The permanent notice in the footer.
  browsing_data::ShouldShowNoticeAboutOtherFormsOfBrowsingHistory(
      ProfileSyncServiceFactory::GetForProfile(GetOriginalProfile()),
      WebHistoryServiceFactory::GetForProfile(GetOriginalProfile()),
      base::Bind(&ShowNoticeAboutOtherFormsOfBrowsingHistory,
                 ScopedJavaGlobalRef<jobject>(env, listener)));

  // The one-time notice in the dialog.
  browsing_data::ShouldPopupDialogAboutOtherFormsOfBrowsingHistory(
      ProfileSyncServiceFactory::GetForProfile(GetOriginalProfile()),
      WebHistoryServiceFactory::GetForProfile(GetOriginalProfile()),
      chrome::GetChannel(),
      base::Bind(&EnableDialogAboutOtherFormsOfBrowsingHistory,
                 ScopedJavaGlobalRef<jobject>(env, listener)));
}

static void FetchImportantSites(JNIEnv* env,
                                const JavaParamRef<jclass>& clazz,
                                const JavaParamRef<jobject>& java_callback) {
  Profile* profile = GetOriginalProfile();
  std::vector<ImportantSitesUtil::ImportantDomainInfo> important_sites =
      ImportantSitesUtil::GetImportantRegisterableDomains(profile,
                                                          kMaxImportantSites);
  bool dialog_disabled = ImportantSitesUtil::IsDialogDisabled(profile);

  std::vector<std::string> important_domains;
  std::vector<int32_t> important_domain_reasons;
  std::vector<std::string> important_domain_examples;
  for (const ImportantSitesUtil::ImportantDomainInfo& info : important_sites) {
    important_domains.push_back(info.registerable_domain);
    important_domain_reasons.push_back(info.reason_bitfield);
    important_domain_examples.push_back(info.example_origin.spec());
  }

  ScopedJavaLocalRef<jobjectArray> java_domains =
      base::android::ToJavaArrayOfStrings(env, important_domains);
  ScopedJavaLocalRef<jintArray> java_reasons =
      base::android::ToJavaIntArray(env, important_domain_reasons);
  ScopedJavaLocalRef<jobjectArray> java_origins =
      base::android::ToJavaArrayOfStrings(env, important_domain_examples);

  Java_ImportantSitesCallback_onImportantRegisterableDomainsReady(
      env, java_callback.obj(), java_domains.obj(), java_origins.obj(),
      java_reasons.obj(), dialog_disabled);
}

// This value should not change during a sessions, as it's used for UMA metrics.
static jint GetMaxImportantSites(JNIEnv* env,
                                 const JavaParamRef<jclass>& clazz) {
  return kMaxImportantSites;
}

static void MarkOriginAsImportantForTesting(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jstring>& jorigin) {
  GURL origin(base::android::ConvertJavaStringToUTF8(jorigin));
  CHECK(origin.is_valid());
  ImportantSitesUtil::MarkOriginAsImportantForTesting(GetOriginalProfile(),
                                                      origin);
}
