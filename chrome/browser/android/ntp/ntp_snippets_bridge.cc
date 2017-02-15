// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/ntp/ntp_snippets_bridge.h"

#include <jni.h>
#include <utility>
#include <vector>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/callback.h"
#include "base/time/time.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/ntp_snippets/content_suggestions_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/history/core/browser/history_service.h"
#include "components/ntp_snippets/content_suggestion.h"
#include "components/ntp_snippets/content_suggestions_metrics.h"
#include "components/ntp_snippets/remote/remote_suggestions_provider.h"
#include "components/ntp_snippets/remote/remote_suggestions_scheduler.h"
#include "jni/SnippetsBridge_jni.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertUTF16ToJavaString;
using base::android::JavaIntArrayToIntVector;
using base::android::AppendJavaStringArrayToStringVector;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaIntArray;
using ntp_snippets::Category;
using ntp_snippets::CategoryInfo;
using ntp_snippets::CategoryStatus;
using ntp_snippets::KnownCategories;
using ntp_snippets::ContentSuggestion;

namespace {

// Converts a vector of ContentSuggestions to its Java equivalent.
ScopedJavaLocalRef<jobject> ToJavaSuggestionList(
    JNIEnv* env,
    const Category& category,
    const std::vector<ContentSuggestion>& suggestions) {
  ScopedJavaLocalRef<jobject> result =
      Java_SnippetsBridge_createSuggestionList(env);
  for (const ContentSuggestion& suggestion : suggestions) {
    ScopedJavaLocalRef<jobject> java_suggestion =
        Java_SnippetsBridge_addSuggestion(
            env, result, category.id(),
            ConvertUTF8ToJavaString(env, suggestion.id().id_within_category()),
            ConvertUTF16ToJavaString(env, suggestion.title()),
            ConvertUTF16ToJavaString(env, suggestion.publisher_name()),
            ConvertUTF16ToJavaString(env, suggestion.snippet_text()),
            ConvertUTF8ToJavaString(env, suggestion.url().spec()),
            suggestion.publish_date().ToJavaTime(), suggestion.score(),
            suggestion.fetch_date().ToJavaTime());
    if (suggestion.id().category().IsKnownCategory(
            KnownCategories::DOWNLOADS) &&
        suggestion.download_suggestion_extra() != nullptr) {
      if (suggestion.download_suggestion_extra()->is_download_asset) {
        Java_SnippetsBridge_setAssetDownloadDataForSuggestion(
            env, java_suggestion,
            ConvertUTF8ToJavaString(env, suggestion.download_suggestion_extra()
                                             ->target_file_path.value()),
            ConvertUTF8ToJavaString(
                env, suggestion.download_suggestion_extra()->mime_type));
      } else {
        Java_SnippetsBridge_setOfflinePageDownloadDataForSuggestion(
            env, java_suggestion,
            suggestion.download_suggestion_extra()->offline_page_id);
      }
    }
    if (suggestion.id().category().IsKnownCategory(
            KnownCategories::RECENT_TABS) &&
        suggestion.recent_tab_suggestion_extra() != nullptr) {
      Java_SnippetsBridge_setRecentTabDataForSuggestion(
          env, java_suggestion,
          suggestion.recent_tab_suggestion_extra()->tab_id,
          suggestion.recent_tab_suggestion_extra()->offline_page_id);
    }
  }

  return result;
}

ntp_snippets::RemoteSuggestionsScheduler* GetRemoteSuggestionsScheduler() {
  ntp_snippets::ContentSuggestionsService* content_suggestions_service =
      ContentSuggestionsServiceFactory::GetForProfile(
          ProfileManager::GetLastUsedProfile());
  // Can maybe be null in some cases? (Incognito profile?) crbug.com/647920
  if (!content_suggestions_service) {
    return nullptr;
  }
  return content_suggestions_service->remote_suggestions_scheduler();
}

}  // namespace

static jlong Init(JNIEnv* env,
                  const JavaParamRef<jobject>& j_bridge,
                  const JavaParamRef<jobject>& j_profile) {
  NTPSnippetsBridge* snippets_bridge =
      new NTPSnippetsBridge(env, j_bridge, j_profile);
  return reinterpret_cast<intptr_t>(snippets_bridge);
}

// Initiates a background fetch for remote suggestions.
static void RemoteSuggestionsSchedulerOnFetchDue(
    JNIEnv* env,
    const JavaParamRef<jclass>& caller) {
  ntp_snippets::RemoteSuggestionsScheduler* scheduler =
      GetRemoteSuggestionsScheduler();
  if (!scheduler) {
    return;
  }

  scheduler->OnPersistentSchedulerWakeUp();
}

// Reschedules the fetching of snippets. If tasks are already scheduled, they
// will be rescheduled anyway, so all running intervals will be reset.
static void RemoteSuggestionsSchedulerRescheduleFetching(
    JNIEnv* env,
    const JavaParamRef<jclass>& caller) {
  ntp_snippets::RemoteSuggestionsScheduler* scheduler =
      GetRemoteSuggestionsScheduler();
  // Can be null if the feature has been disabled but the scheduler has not been
  // unregistered yet. The next start should unregister it.
  if (!scheduler) {
    return;
  }

  scheduler->RescheduleFetching();
}

static void OnSuggestionTargetVisited(JNIEnv* env,
                                      const JavaParamRef<jclass>& caller,
                                      jint j_category_id,
                                      jlong visit_time_ms) {
  ntp_snippets::metrics::OnSuggestionTargetVisited(
      Category::FromIDValue(j_category_id),
      base::TimeDelta::FromMilliseconds(visit_time_ms));
}

NTPSnippetsBridge::NTPSnippetsBridge(JNIEnv* env,
                                     const JavaParamRef<jobject>& j_bridge,
                                     const JavaParamRef<jobject>& j_profile)
    : content_suggestions_service_observer_(this),
      bridge_(env, j_bridge),
      weak_ptr_factory_(this) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  content_suggestions_service_ =
      ContentSuggestionsServiceFactory::GetForProfile(profile);
  history_service_ =
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  content_suggestions_service_observer_.Add(content_suggestions_service_);
}

void NTPSnippetsBridge::Destroy(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  delete this;
}

ScopedJavaLocalRef<jintArray> NTPSnippetsBridge::GetCategories(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  std::vector<int> category_ids;
  for (Category category : content_suggestions_service_->GetCategories()) {
    category_ids.push_back(category.id());
  }
  return ToJavaIntArray(env, category_ids);
}

int NTPSnippetsBridge::GetCategoryStatus(JNIEnv* env,
                                         const JavaParamRef<jobject>& obj,
                                         jint j_category_id) {
  return static_cast<int>(content_suggestions_service_->GetCategoryStatus(
      Category::FromIDValue(j_category_id)));
}

base::android::ScopedJavaLocalRef<jobject> NTPSnippetsBridge::GetCategoryInfo(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jint j_category_id) {
  base::Optional<CategoryInfo> info =
      content_suggestions_service_->GetCategoryInfo(
          Category::FromIDValue(j_category_id));
  if (!info) {
    return base::android::ScopedJavaLocalRef<jobject>(env, nullptr);
  }
  return Java_SnippetsBridge_createSuggestionsCategoryInfo(
      env, j_category_id, ConvertUTF16ToJavaString(env, info->title()),
      static_cast<int>(info->card_layout()), info->has_fetch_action(),
      info->has_view_all_action(), info->show_if_empty(),
      ConvertUTF16ToJavaString(env, info->no_suggestions_message()));
}

ScopedJavaLocalRef<jobject> NTPSnippetsBridge::GetSuggestionsForCategory(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jint j_category_id) {
  Category category = Category::FromIDValue(j_category_id);
  return ToJavaSuggestionList(
      env, category,
      content_suggestions_service_->GetSuggestionsForCategory(category));
}

void NTPSnippetsBridge::FetchSuggestionImage(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint j_category_id,
    const JavaParamRef<jstring>& id_within_category,
    const JavaParamRef<jobject>& j_callback) {
  base::android::ScopedJavaGlobalRef<jobject> callback(j_callback);
  content_suggestions_service_->FetchSuggestionImage(
      ContentSuggestion::ID(Category::FromIDValue(j_category_id),
                            ConvertJavaStringToUTF8(env, id_within_category)),
      base::Bind(&NTPSnippetsBridge::OnImageFetched,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void NTPSnippetsBridge::Fetch(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint j_category_id,
    const JavaParamRef<jobjectArray>& j_displayed_suggestions) {
  std::vector<std::string> known_suggestion_ids;
  AppendJavaStringArrayToStringVector(env, j_displayed_suggestions,
                                      &known_suggestion_ids);

  Category category = Category::FromIDValue(j_category_id);
  content_suggestions_service_->Fetch(
      category, std::set<std::string>(known_suggestion_ids.begin(),
                                      known_suggestion_ids.end()),
      base::Bind(&NTPSnippetsBridge::OnSuggestionsFetched,
                 weak_ptr_factory_.GetWeakPtr(), category));
}

void NTPSnippetsBridge::ReloadSuggestions(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj) {
  content_suggestions_service_->ReloadSuggestions();
}

void NTPSnippetsBridge::DismissSuggestion(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& jurl,
    jint global_position,
    jint j_category_id,
    jint position_in_category,
    const JavaParamRef<jstring>& id_within_category) {
  Category category = Category::FromIDValue(j_category_id);

  content_suggestions_service_->DismissSuggestion(ContentSuggestion::ID(
      category, ConvertJavaStringToUTF8(env, id_within_category)));

  history_service_->QueryURL(
      GURL(ConvertJavaStringToUTF8(env, jurl)), /*want_visits=*/false,
      base::Bind(
          [](int global_position, Category category, int position_in_category,
             bool success, const history::URLRow& row,
             const history::VisitVector& visit_vector) {
            bool visited = success && row.visit_count() != 0;
            ntp_snippets::metrics::OnSuggestionDismissed(
                global_position, category, position_in_category, visited);
          },
          global_position, category, position_in_category),
      &tracker_);
}

void NTPSnippetsBridge::DismissCategory(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj,
                                        jint j_category_id) {
  Category category = Category::FromIDValue(j_category_id);

  content_suggestions_service_->DismissCategory(category);

  ntp_snippets::metrics::OnCategoryDismissed(category);
}

void NTPSnippetsBridge::RestoreDismissedCategories(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  content_suggestions_service_->RestoreDismissedCategories();
}

void NTPSnippetsBridge::OnPageShown(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jintArray>& jcategories,
    const JavaParamRef<jintArray>& jsuggestions_per_category) {
  std::vector<int> categories_int;
  JavaIntArrayToIntVector(env, jcategories, &categories_int);
  std::vector<int> suggestions_per_category_int;
  JavaIntArrayToIntVector(env, jsuggestions_per_category,
                          &suggestions_per_category_int);
  DCHECK_EQ(categories_int.size(), suggestions_per_category_int.size());
  std::vector<std::pair<Category, int>> suggestions_per_category;
  for (size_t i = 0; i < categories_int.size(); i++) {
    suggestions_per_category.push_back(
        std::make_pair(Category::FromIDValue(categories_int[i]),
                       suggestions_per_category_int[i]));
  }
  ntp_snippets::metrics::OnPageShown(suggestions_per_category);
  content_suggestions_service_->user_classifier()->OnEvent(
      ntp_snippets::UserClassifier::Metric::NTP_OPENED);
}

void NTPSnippetsBridge::OnSuggestionShown(JNIEnv* env,
                                          const JavaParamRef<jobject>& obj,
                                          jint global_position,
                                          jint j_category_id,
                                          jint position_in_category,
                                          jlong publish_timestamp_ms,
                                          jfloat score,
                                          jlong fetch_timestamp_ms) {
  ntp_snippets::metrics::OnSuggestionShown(
      global_position, Category::FromIDValue(j_category_id),
      position_in_category, base::Time::FromJavaTime(publish_timestamp_ms),
      score, base::Time::FromJavaTime(fetch_timestamp_ms));
  if (global_position == 0) {
    content_suggestions_service_->user_classifier()->OnEvent(
        ntp_snippets::UserClassifier::Metric::SUGGESTIONS_SHOWN);
  }
}

void NTPSnippetsBridge::OnSuggestionOpened(JNIEnv* env,
                                           const JavaParamRef<jobject>& obj,
                                           jint global_position,
                                           jint j_category_id,
                                           jint category_index,
                                           jint position_in_category,
                                           jlong publish_timestamp_ms,
                                           jfloat score,
                                           int windowOpenDisposition) {
  const Category category = Category::FromIDValue(j_category_id);
  ntp_snippets::metrics::OnSuggestionOpened(
      global_position, category, category_index, position_in_category,
      base::Time::FromJavaTime(publish_timestamp_ms), score,
      static_cast<WindowOpenDisposition>(windowOpenDisposition));
  // TODO(vitaliii): Add ContentSuggestionsService::OnSuggestionOpened and
  // notify the ranker and the classifier there instead. Do not expose both of
  // them at all. See crbug.com/674080.
  content_suggestions_service_->category_ranker()->OnSuggestionOpened(category);
  content_suggestions_service_->user_classifier()->OnEvent(
      ntp_snippets::UserClassifier::Metric::SUGGESTIONS_USED);
}

void NTPSnippetsBridge::OnSuggestionMenuOpened(JNIEnv* env,
                                               const JavaParamRef<jobject>& obj,
                                               jint global_position,
                                               jint j_category_id,
                                               jint position_in_category,
                                               jlong publish_timestamp_ms,
                                               jfloat score) {
  ntp_snippets::metrics::OnSuggestionMenuOpened(
      global_position, Category::FromIDValue(j_category_id),
      position_in_category, base::Time::FromJavaTime(publish_timestamp_ms),
      score);
}

void NTPSnippetsBridge::OnMoreButtonShown(JNIEnv* env,
                                          const JavaParamRef<jobject>& obj,
                                          jint j_category_id,
                                          jint position) {
  ntp_snippets::metrics::OnMoreButtonShown(Category::FromIDValue(j_category_id),
                                           position);
}

void NTPSnippetsBridge::OnMoreButtonClicked(JNIEnv* env,
                                            const JavaParamRef<jobject>& obj,
                                            jint j_category_id,
                                            jint position) {
  ntp_snippets::metrics::OnMoreButtonClicked(
      Category::FromIDValue(j_category_id), position);
  content_suggestions_service_->user_classifier()->OnEvent(
      ntp_snippets::UserClassifier::Metric::SUGGESTIONS_USED);
}

void NTPSnippetsBridge::OnNTPInitialized(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  ntp_snippets::RemoteSuggestionsScheduler* scheduler =
      GetRemoteSuggestionsScheduler();
  // Can be null if the feature has been disabled but the scheduler has not been
  // unregistered yet. The next start should unregister it.
  if (!scheduler) {
    return;
  }

  scheduler->OnNTPOpened();
}

void NTPSnippetsBridge::OnColdStart(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  ntp_snippets::RemoteSuggestionsScheduler* scheduler =
      GetRemoteSuggestionsScheduler();
  // TODO(fhorschig): Remove guard when https://crbug.com/678556 is resolved.
  if (!scheduler) {
    return;
  }
  scheduler->OnBrowserColdStart();
}

void NTPSnippetsBridge::OnActivityWarmResumed(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  ntp_snippets::RemoteSuggestionsScheduler* scheduler =
      GetRemoteSuggestionsScheduler();
  // TODO(fhorschig): Remove guard when https://crbug.com/678556 is resolved.
  if (!scheduler) {
    return;
  }
  scheduler->OnBrowserForegrounded();
}

NTPSnippetsBridge::~NTPSnippetsBridge() {}

void NTPSnippetsBridge::OnNewSuggestions(Category category) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SnippetsBridge_onNewSuggestions(env, bridge_,
                                       static_cast<int>(category.id()));
}

void NTPSnippetsBridge::OnCategoryStatusChanged(Category category,
                                                CategoryStatus new_status) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SnippetsBridge_onCategoryStatusChanged(env, bridge_,
                                              static_cast<int>(category.id()),
                                              static_cast<int>(new_status));
}

void NTPSnippetsBridge::OnSuggestionInvalidated(
    const ContentSuggestion::ID& suggestion_id) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SnippetsBridge_onSuggestionInvalidated(
      env, bridge_.obj(), static_cast<int>(suggestion_id.category().id()),
      ConvertUTF8ToJavaString(env, suggestion_id.id_within_category()).obj());
}

void NTPSnippetsBridge::OnFullRefreshRequired() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SnippetsBridge_onFullRefreshRequired(env, bridge_.obj());
}

void NTPSnippetsBridge::ContentSuggestionsServiceShutdown() {
  bridge_.Reset();
  content_suggestions_service_observer_.Remove(content_suggestions_service_);
}

void NTPSnippetsBridge::OnImageFetched(ScopedJavaGlobalRef<jobject> callback,
                                       const gfx::Image& image) {
  ScopedJavaLocalRef<jobject> j_bitmap;
  if (!image.IsEmpty()) {
    j_bitmap = gfx::ConvertToJavaBitmap(image.ToSkBitmap());
  }
  base::android::RunCallbackAndroid(callback, j_bitmap);
}

void NTPSnippetsBridge::OnSuggestionsFetched(
    Category category,
    ntp_snippets::Status status,
    std::vector<ContentSuggestion> suggestions) {
  // TODO(fhorschig, dgn): Allow refetch or show notification acc. to status.
  JNIEnv* env = AttachCurrentThread();
  Java_SnippetsBridge_onMoreSuggestions(
      env, bridge_, category.id(),
      ToJavaSuggestionList(env, category, suggestions));
}

// static
bool NTPSnippetsBridge::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
