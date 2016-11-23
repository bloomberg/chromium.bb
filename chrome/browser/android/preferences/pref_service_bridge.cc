// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/preferences/pref_service_bridge.h"

#include <jni.h>
#include <stddef.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/jni_weak_ref.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/scoped_observer.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/browsing_data_filter_builder.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/browsing_data_remover.h"
#include "chrome/browser/browsing_data/browsing_data_remover_factory.h"
#include "chrome/browser/browsing_data/registrable_domain_filter_builder.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/engagement/important_sites_util.h"
#include "chrome/browser/history/web_history_service_factory.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/android/android_about_app_info.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/locale_settings.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/browsing_data/core/history_notice_utils.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing_db/safe_browsing_prefs.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "components/strings/grit/components_locale_settings.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/translate/core/common/translate_pref_names.h"
#include "components/version_info/version_info.h"
#include "components/web_resource/web_resource_pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "jni/PrefServiceBridge_jni.h"
#include "third_party/icu/source/common/unicode/uloc.h"
#include "ui/base/l10n/l10n_util.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using base::android::ScopedJavaGlobalRef;
using content::BrowserThread;

namespace {

const size_t kMaxImportantSites = 5;

Profile* GetOriginalProfile() {
  return ProfileManager::GetActiveUserProfile()->GetOriginalProfile();
}

bool GetBooleanForContentSetting(ContentSettingsType type) {
  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(GetOriginalProfile());
  switch (content_settings->GetDefaultContentSetting(type, NULL)) {
    case CONTENT_SETTING_BLOCK:
      return false;
    case CONTENT_SETTING_ALLOW:
    case CONTENT_SETTING_ASK:
    default:
      return true;
  }
}

bool IsContentSettingManaged(ContentSettingsType content_settings_type) {
  std::string source;
  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(GetOriginalProfile());
  content_settings->GetDefaultContentSetting(content_settings_type, &source);
  HostContentSettingsMap::ProviderType provider =
      content_settings->GetProviderTypeFromSource(source);
  return provider == HostContentSettingsMap::POLICY_PROVIDER;
}

bool IsContentSettingManagedByCustodian(
    ContentSettingsType content_settings_type) {
  std::string source;
  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(GetOriginalProfile());
  content_settings->GetDefaultContentSetting(content_settings_type, &source);
  HostContentSettingsMap::ProviderType provider =
      content_settings->GetProviderTypeFromSource(source);
  return provider == HostContentSettingsMap::SUPERVISED_PROVIDER;
}

bool IsContentSettingUserModifiable(ContentSettingsType content_settings_type) {
  std::string source;
  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(GetOriginalProfile());
  content_settings->GetDefaultContentSetting(content_settings_type, &source);
  HostContentSettingsMap::ProviderType provider =
      content_settings->GetProviderTypeFromSource(source);
  return provider >= HostContentSettingsMap::PREF_PROVIDER;
}

PrefService* GetPrefService() {
  return GetOriginalProfile()->GetPrefs();
}

}  // namespace

// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------

static jboolean IsContentSettingManaged(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj,
                                        int content_settings_type) {
  return IsContentSettingManaged(
      static_cast<ContentSettingsType>(content_settings_type));
}

static jboolean IsContentSettingEnabled(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj,
                                        int content_settings_type) {
  // Before we migrate functions over to this central function, we must verify
  // that the functionality provided below is correct.
  DCHECK(content_settings_type == CONTENT_SETTINGS_TYPE_JAVASCRIPT ||
         content_settings_type == CONTENT_SETTINGS_TYPE_POPUPS);
  ContentSettingsType type =
      static_cast<ContentSettingsType>(content_settings_type);
  return GetBooleanForContentSetting(type);
}

static void SetContentSettingEnabled(JNIEnv* env,
                                     const JavaParamRef<jobject>& obj,
                                     int content_settings_type,
                                     jboolean allow) {
  // Before we migrate functions over to this central function, we must verify
  // that the new category supports ALLOW/BLOCK pairs and, if not, handle them.
  DCHECK(content_settings_type == CONTENT_SETTINGS_TYPE_JAVASCRIPT ||
         content_settings_type == CONTENT_SETTINGS_TYPE_POPUPS);

  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(GetOriginalProfile());
  host_content_settings_map->SetDefaultContentSetting(
      static_cast<ContentSettingsType>(content_settings_type),
      allow ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK);
}

static void SetContentSettingForPattern(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj,
                                        int content_settings_type,
                                        const JavaParamRef<jstring>& pattern,
                                        int setting) {
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(GetOriginalProfile());
  host_content_settings_map->SetContentSettingCustomScope(
      ContentSettingsPattern::FromString(ConvertJavaStringToUTF8(env, pattern)),
      ContentSettingsPattern::Wildcard(),
      static_cast<ContentSettingsType>(content_settings_type), std::string(),
      static_cast<ContentSetting>(setting));
}

static void GetContentSettingsExceptions(JNIEnv* env,
                                         const JavaParamRef<jobject>& obj,
                                         int content_settings_type,
                                         const JavaParamRef<jobject>& list) {
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(GetOriginalProfile());
  ContentSettingsForOneType entries;
  host_content_settings_map->GetSettingsForOneType(
      static_cast<ContentSettingsType>(content_settings_type), "", &entries);
  for (size_t i = 0; i < entries.size(); ++i) {
    Java_PrefServiceBridge_addContentSettingExceptionToList(
        env, list, content_settings_type,
        ConvertUTF8ToJavaString(env, entries[i].primary_pattern.ToString()),
        entries[i].setting, ConvertUTF8ToJavaString(env, entries[i].source));
  }
}

static jboolean GetAcceptCookiesEnabled(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj) {
  return GetBooleanForContentSetting(CONTENT_SETTINGS_TYPE_COOKIES);
}

static jboolean GetAcceptCookiesManaged(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj) {
  return IsContentSettingManaged(CONTENT_SETTINGS_TYPE_COOKIES);
}

static jboolean GetAutoplayEnabled(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj) {
  return GetBooleanForContentSetting(CONTENT_SETTINGS_TYPE_AUTOPLAY);
}

static jboolean GetBackgroundSyncEnabled(JNIEnv* env,
                                         const JavaParamRef<jobject>& obj) {
  return GetBooleanForContentSetting(CONTENT_SETTINGS_TYPE_BACKGROUND_SYNC);
}

static jboolean GetBlockThirdPartyCookiesEnabled(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return GetPrefService()->GetBoolean(prefs::kBlockThirdPartyCookies);
}

static jboolean GetBlockThirdPartyCookiesManaged(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return GetPrefService()->IsManagedPreference(prefs::kBlockThirdPartyCookies);
}

static jboolean GetRememberPasswordsEnabled(JNIEnv* env,
                                            const JavaParamRef<jobject>& obj) {
  return GetPrefService()->GetBoolean(
      password_manager::prefs::kPasswordManagerSavingEnabled);
}

static jboolean GetPasswordManagerAutoSigninEnabled(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return GetPrefService()->GetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin);
}

static jboolean GetRememberPasswordsManaged(JNIEnv* env,
                                            const JavaParamRef<jobject>& obj) {
  return GetPrefService()->IsManagedPreference(
      password_manager::prefs::kPasswordManagerSavingEnabled);
}

static jboolean GetPasswordManagerAutoSigninManaged(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return GetPrefService()->IsManagedPreference(
      password_manager::prefs::kCredentialsEnableAutosignin);
}

static jboolean GetDoNotTrackEnabled(JNIEnv* env,
                                     const JavaParamRef<jobject>& obj) {
  return GetPrefService()->GetBoolean(prefs::kEnableDoNotTrack);
}

static jboolean GetNetworkPredictionEnabled(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj) {
  return GetPrefService()->GetInteger(prefs::kNetworkPredictionOptions)
      != chrome_browser_net::NETWORK_PREDICTION_NEVER;
}

static jboolean GetNetworkPredictionManaged(JNIEnv* env,
                                            const JavaParamRef<jobject>& obj) {
  return GetPrefService()->IsManagedPreference(
      prefs::kNetworkPredictionOptions);
}

static jboolean GetPasswordEchoEnabled(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj) {
  return GetPrefService()->GetBoolean(prefs::kWebKitPasswordEchoEnabled);
}

static jboolean GetPrintingEnabled(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj) {
  return GetPrefService()->GetBoolean(prefs::kPrintingEnabled);
}

static jboolean GetPrintingManaged(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj) {
  return GetPrefService()->IsManagedPreference(prefs::kPrintingEnabled);
}

static jboolean GetTranslateEnabled(JNIEnv* env,
                                    const JavaParamRef<jobject>& obj) {
  return GetPrefService()->GetBoolean(prefs::kEnableTranslate);
}

static jboolean GetTranslateManaged(JNIEnv* env,
                                    const JavaParamRef<jobject>& obj) {
  return GetPrefService()->IsManagedPreference(prefs::kEnableTranslate);
}

static jboolean GetSearchSuggestEnabled(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj) {
  return GetPrefService()->GetBoolean(prefs::kSearchSuggestEnabled);
}

static jboolean GetSearchSuggestManaged(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj) {
  return GetPrefService()->IsManagedPreference(prefs::kSearchSuggestEnabled);
}

static jboolean GetSafeBrowsingExtendedReportingEnabled(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return safe_browsing::IsExtendedReportingEnabled(*GetPrefService());
}

static void SetSafeBrowsingExtendedReportingEnabled(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean enabled) {
  safe_browsing::SetExtendedReportingPref(GetPrefService(), enabled);
}

static jboolean GetSafeBrowsingExtendedReportingManaged(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  PrefService* pref_service = GetPrefService();
  return pref_service->IsManagedPreference(
      safe_browsing::GetExtendedReportingPrefName(*pref_service));
}

static jboolean GetSafeBrowsingEnabled(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj) {
  return GetPrefService()->GetBoolean(prefs::kSafeBrowsingEnabled);
}

static void SetSafeBrowsingEnabled(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj,
                                   jboolean enabled) {
  GetPrefService()->SetBoolean(prefs::kSafeBrowsingEnabled, enabled);
}

static jboolean GetSafeBrowsingManaged(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj) {
  return GetPrefService()->IsManagedPreference(prefs::kSafeBrowsingEnabled);
}

static jboolean GetProtectedMediaIdentifierEnabled(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return GetBooleanForContentSetting(
      CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER);
}

static jboolean GetNotificationsEnabled(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj) {
  return GetBooleanForContentSetting(CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
}

static jboolean GetNotificationsVibrateEnabled(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return GetPrefService()->GetBoolean(prefs::kNotificationsVibrateEnabled);
}

static jboolean GetAllowLocationEnabled(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj) {
  return GetBooleanForContentSetting(CONTENT_SETTINGS_TYPE_GEOLOCATION);
}

static jboolean GetLocationAllowedByPolicy(JNIEnv* env,
                                           const JavaParamRef<jobject>& obj) {
  if (!IsContentSettingManaged(CONTENT_SETTINGS_TYPE_GEOLOCATION))
    return false;
  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(GetOriginalProfile());
  return content_settings->GetDefaultContentSetting(
             CONTENT_SETTINGS_TYPE_GEOLOCATION, nullptr) ==
         CONTENT_SETTING_ALLOW;
}

static jboolean GetAllowLocationUserModifiable(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return IsContentSettingUserModifiable(CONTENT_SETTINGS_TYPE_GEOLOCATION);
}

static jboolean GetAllowLocationManagedByCustodian(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return IsContentSettingManagedByCustodian(CONTENT_SETTINGS_TYPE_GEOLOCATION);
}

static jboolean GetResolveNavigationErrorEnabled(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return GetPrefService()->GetBoolean(prefs::kAlternateErrorPagesEnabled);
}

static jboolean GetResolveNavigationErrorManaged(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return GetPrefService()->IsManagedPreference(
      prefs::kAlternateErrorPagesEnabled);
}

static jboolean GetSupervisedUserSafeSitesEnabled(JNIEnv* env,
                                         const JavaParamRef<jobject>& obj) {
  return GetPrefService()->GetBoolean(prefs::kSupervisedUserSafeSites);
}

static jint GetDefaultSupervisedUserFilteringBehavior(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return GetPrefService()->GetInteger(
      prefs::kDefaultSupervisedUserFilteringBehavior);
}

static jboolean GetIncognitoModeEnabled(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj) {
  PrefService* prefs = GetPrefService();
  IncognitoModePrefs::Availability incognito_pref =
      IncognitoModePrefs::GetAvailability(prefs);
  DCHECK(incognito_pref == IncognitoModePrefs::ENABLED ||
         incognito_pref == IncognitoModePrefs::DISABLED) <<
             "Unsupported incognito mode preference: " << incognito_pref;
  return incognito_pref != IncognitoModePrefs::DISABLED;
}

static jboolean GetIncognitoModeManaged(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj) {
  return GetPrefService()->IsManagedPreference(
      prefs::kIncognitoModeAvailability);
}

static jboolean IsMetricsReportingEnabled(JNIEnv* env,
                                           const JavaParamRef<jobject>& obj) {
  PrefService* local_state = g_browser_process->local_state();
  return local_state->GetBoolean(metrics::prefs::kMetricsReportingEnabled);
}

static void SetMetricsReportingEnabled(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj,
                                       jboolean enabled) {
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetBoolean(metrics::prefs::kMetricsReportingEnabled, enabled);
}

static jboolean IsMetricsReportingManaged(JNIEnv* env,
                                           const JavaParamRef<jobject>& obj) {
  return GetPrefService()->IsManagedPreference(
      metrics::prefs::kMetricsReportingEnabled);
}

static void SetClickedUpdateMenuItem(JNIEnv* env,
                                     const JavaParamRef<jobject>& obj,
                                     jboolean clicked) {
  GetPrefService()->SetBoolean(prefs::kClickedUpdateMenuItem, clicked);
}

static jboolean GetClickedUpdateMenuItem(JNIEnv* env,
                                         const JavaParamRef<jobject>& obj) {
  return GetPrefService()->GetBoolean(prefs::kClickedUpdateMenuItem);
}

static void SetLatestVersionWhenClickedUpdateMenuItem(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& version) {
  GetPrefService()->SetString(
      prefs::kLatestVersionWhenClickedUpdateMenuItem,
      ConvertJavaStringToUTF8(env, version));
}

static ScopedJavaLocalRef<jstring> GetLatestVersionWhenClickedUpdateMenuItem(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return ConvertUTF8ToJavaString(
      env, GetPrefService()->GetString(
          prefs::kLatestVersionWhenClickedUpdateMenuItem));
}

namespace {

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

    Java_PrefServiceBridge_browsingDataCleared(
        env, weak_chrome_native_preferences_.get(env));
  }

 private:
  int task_count_;
  JavaObjectWeakGlobalRef weak_chrome_native_preferences_;
  ScopedObserver<BrowsingDataRemover, BrowsingDataRemover::Observer> observer_;
};

}  // namespace

static jboolean GetBrowsingDataDeletionPreference(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint data_type) {
  DCHECK_GE(data_type, 0);
  DCHECK_LT(data_type, browsing_data::NUM_TYPES);

  // If there is no corresponding preference for this |data_type|, pretend
  // that it's set to false.
  // TODO(msramek): Consider defining native-side preferences for all Java UI
  // data types for consistency.
  std::string pref;
  if (!browsing_data::GetDeletionPreferenceFromDataType(
          static_cast<browsing_data::BrowsingDataType>(data_type), &pref)) {
    return false;
  }

  return GetOriginalProfile()->GetPrefs()->GetBoolean(pref);
}

static void SetBrowsingDataDeletionPreference(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint data_type,
    jboolean value) {
  DCHECK_GE(data_type, 0);
  DCHECK_LT(data_type, browsing_data::NUM_TYPES);

  std::string pref;
  if (!browsing_data::GetDeletionPreferenceFromDataType(
          static_cast<browsing_data::BrowsingDataType>(data_type), &pref)) {
    return;
  }

  GetOriginalProfile()->GetPrefs()->SetBoolean(pref, value);
}

static jint GetBrowsingDataDeletionTimePeriod(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return GetPrefService()->GetInteger(browsing_data::prefs::kDeleteTimePeriod);
}

static void SetBrowsingDataDeletionTimePeriod(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint time_period) {
  DCHECK_GE(time_period, 0);
  DCHECK_LE(time_period, browsing_data::TIME_PERIOD_LAST);
  GetPrefService()->SetInteger(browsing_data::prefs::kDeleteTimePeriod,
                               time_period);
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
      BrowsingDataRemoverFactory::GetForBrowserContext(GetOriginalProfile());

  std::vector<int> data_types_vector;
  base::android::JavaIntArrayToIntVector(env, data_types, &data_types_vector);

  int remove_mask = 0;
  for (const int data_type : data_types_vector) {
    switch (static_cast<browsing_data::BrowsingDataType>(data_type)) {
      case browsing_data::HISTORY:
        remove_mask |= BrowsingDataRemover::REMOVE_HISTORY;
        break;
      case browsing_data::CACHE:
        remove_mask |= BrowsingDataRemover::REMOVE_CACHE;
        break;
      case browsing_data::COOKIES:
        remove_mask |= BrowsingDataRemover::REMOVE_COOKIES;
        remove_mask |= BrowsingDataRemover::REMOVE_SITE_DATA;
        break;
      case browsing_data::PASSWORDS:
        remove_mask |= BrowsingDataRemover::REMOVE_PASSWORDS;
        break;
      case browsing_data::FORM_DATA:
        remove_mask |= BrowsingDataRemover::REMOVE_FORM_DATA;
        break;
      case browsing_data::BOOKMARKS:
        // Bookmarks are deleted separately on the Java side.
        NOTREACHED();
        break;
      case browsing_data::NUM_TYPES:
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
  std::unique_ptr<RegistrableDomainFilterBuilder> filter_builder(
      new RegistrableDomainFilterBuilder(BrowsingDataFilterBuilder::BLACKLIST));
  for (const std::string& domain : excluding_domains) {
    filter_builder->AddRegisterableDomain(domain);
  }

  if (!excluding_domains.empty()) {
    ImportantSitesUtil::RecordBlacklistedAndIgnoredImportantSites(
        GetOriginalProfile(), excluding_domains, excluding_domain_reasons,
        ignoring_domains, ignoring_domain_reasons);
  }

  // Delete the types protected by Important Sites with a filter,
  // and the rest completely.
  int filterable_mask =
      remove_mask & BrowsingDataRemover::IMPORTANT_SITES_DATATYPES;
  int nonfilterable_mask = remove_mask &
      ~BrowsingDataRemover::IMPORTANT_SITES_DATATYPES;

  // ClearBrowsingDataObserver deletes itself when |browsing_data_remover| is
  // done with both removal tasks.
  ClearBrowsingDataObserver* observer = new ClearBrowsingDataObserver(
      env, obj, browsing_data_remover, 2 /* tasks_count */);

  if (filterable_mask) {
    browsing_data_remover->RemoveWithFilterAndReply(
        BrowsingDataRemover::Period(
            static_cast<browsing_data::TimePeriod>(time_period)),
        filterable_mask, BrowsingDataHelper::UNPROTECTED_WEB,
        std::move(filter_builder), observer);
  } else {
    // Make sure |observer| doesn't wait for the filtered task.
    observer->OnBrowsingDataRemoverDone();
  }

  if (nonfilterable_mask) {
    browsing_data_remover->RemoveAndReply(
        BrowsingDataRemover::Period(
            static_cast<browsing_data::TimePeriod>(time_period)),
        nonfilterable_mask, BrowsingDataHelper::UNPROTECTED_WEB, observer);
  } else {
    // Make sure |observer| doesn't wait for the non-filtered task.
    observer->OnBrowsingDataRemoverDone();
  }
}

static jboolean CanDeleteBrowsingHistory(JNIEnv* env,
                                         const JavaParamRef<jobject>& obj) {
  return GetPrefService()->GetBoolean(prefs::kAllowDeletingBrowserHistory);
}

static void FetchImportantSites(JNIEnv* env,
                                const JavaParamRef<jclass>& clazz,
                                const JavaParamRef<jobject>& java_callback) {
  std::vector<ImportantSitesUtil::ImportantDomainInfo> important_sites =
      ImportantSitesUtil::GetImportantRegisterableDomains(GetOriginalProfile(),
                                                          kMaxImportantSites);

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
      java_reasons.obj());
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

static void ShowNoticeAboutOtherFormsOfBrowsingHistory(
    ScopedJavaGlobalRef<jobject>* listener,
    bool show) {
  JNIEnv* env = AttachCurrentThread();
  UMA_HISTOGRAM_BOOLEAN(
      "History.ClearBrowsingData.HistoryNoticeShownInFooterWhenUpdated", show);
  if (!show)
    return;
  Java_OtherFormsOfBrowsingHistoryListener_showNoticeAboutOtherFormsOfBrowsingHistory(
      env, listener->obj());
}

static void EnableDialogAboutOtherFormsOfBrowsingHistory(
    ScopedJavaGlobalRef<jobject>* listener,
    bool enabled) {
  JNIEnv* env = AttachCurrentThread();
  if (!enabled)
    return;
  Java_OtherFormsOfBrowsingHistoryListener_enableDialogAboutOtherFormsOfBrowsingHistory(
      env, listener->obj());
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
                 base::Owned(new ScopedJavaGlobalRef<jobject>(env, listener))));

  // The one-time notice in the dialog.
  browsing_data::ShouldPopupDialogAboutOtherFormsOfBrowsingHistory(
      ProfileSyncServiceFactory::GetForProfile(GetOriginalProfile()),
      WebHistoryServiceFactory::GetForProfile(GetOriginalProfile()),
      chrome::GetChannel(),
      base::Bind(&EnableDialogAboutOtherFormsOfBrowsingHistory,
                 base::Owned(new ScopedJavaGlobalRef<jobject>(env, listener))));
}

static void SetAutoplayEnabled(JNIEnv* env,
                               const JavaParamRef<jobject>& obj,
                               jboolean allow) {
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(GetOriginalProfile());
  host_content_settings_map->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_AUTOPLAY,
      allow ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK);
}

static void SetAllowCookiesEnabled(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj,
                                   jboolean allow) {
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(GetOriginalProfile());
  host_content_settings_map->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_COOKIES,
      allow ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK);
}

static void SetBackgroundSyncEnabled(JNIEnv* env,
                                     const JavaParamRef<jobject>& obj,
                                     jboolean allow) {
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(GetOriginalProfile());
  host_content_settings_map->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_BACKGROUND_SYNC,
      allow ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK);
}

static void SetBlockThirdPartyCookiesEnabled(JNIEnv* env,
                                             const JavaParamRef<jobject>& obj,
                                             jboolean enabled) {
  GetPrefService()->SetBoolean(prefs::kBlockThirdPartyCookies, enabled);
}

static void SetRememberPasswordsEnabled(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj,
                                        jboolean allow) {
  GetPrefService()->SetBoolean(
      password_manager::prefs::kPasswordManagerSavingEnabled, allow);
}

static void SetPasswordManagerAutoSigninEnabled(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean enabled) {
  GetPrefService()->SetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin, enabled);
}

static void SetProtectedMediaIdentifierEnabled(JNIEnv* env,
                                               const JavaParamRef<jobject>& obj,
                                               jboolean is_enabled) {
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(GetOriginalProfile());
  host_content_settings_map->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER,
      is_enabled ? CONTENT_SETTING_ASK : CONTENT_SETTING_BLOCK);
}

static void SetAllowLocationEnabled(JNIEnv* env,
                                    const JavaParamRef<jobject>& obj,
                                    jboolean is_enabled) {
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(GetOriginalProfile());
  host_content_settings_map->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_GEOLOCATION,
      is_enabled ? CONTENT_SETTING_ASK : CONTENT_SETTING_BLOCK);
}

static void SetCameraEnabled(JNIEnv* env,
                             const JavaParamRef<jobject>& obj,
                             jboolean allow) {
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(GetOriginalProfile());
  host_content_settings_map->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA,
      allow ? CONTENT_SETTING_ASK : CONTENT_SETTING_BLOCK);
}

static void SetMicEnabled(JNIEnv* env,
                          const JavaParamRef<jobject>& obj,
                          jboolean allow) {
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(GetOriginalProfile());
  host_content_settings_map->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
      allow ? CONTENT_SETTING_ASK : CONTENT_SETTING_BLOCK);
}

static void SetNotificationsEnabled(JNIEnv* env,
                                    const JavaParamRef<jobject>& obj,
                                    jboolean allow) {
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(GetOriginalProfile());
  host_content_settings_map->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
      allow ? CONTENT_SETTING_ASK : CONTENT_SETTING_BLOCK);
}

static void SetNotificationsVibrateEnabled(JNIEnv* env,
                                           const JavaParamRef<jobject>& obj,
                                           jboolean enabled) {
  GetPrefService()->SetBoolean(prefs::kNotificationsVibrateEnabled, enabled);
}

static jboolean CanPrefetchAndPrerender(JNIEnv* env,
                                         const JavaParamRef<jobject>& obj) {
  return chrome_browser_net::CanPrefetchAndPrerenderUI(GetPrefService()) ==
      chrome_browser_net::NetworkPredictionStatus::ENABLED;
}

static void SetDoNotTrackEnabled(JNIEnv* env,
                                 const JavaParamRef<jobject>& obj,
                                 jboolean allow) {
  GetPrefService()->SetBoolean(prefs::kEnableDoNotTrack, allow);
}

static ScopedJavaLocalRef<jstring> GetSyncLastAccountId(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return ConvertUTF8ToJavaString(
      env, GetPrefService()->GetString(prefs::kGoogleServicesLastAccountId));
}

static ScopedJavaLocalRef<jstring> GetSyncLastAccountName(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return ConvertUTF8ToJavaString(
      env, GetPrefService()->GetString(prefs::kGoogleServicesLastUsername));
}

static void SetTranslateEnabled(JNIEnv* env,
                                const JavaParamRef<jobject>& obj,
                                jboolean enabled) {
  GetPrefService()->SetBoolean(prefs::kEnableTranslate, enabled);
}

static void ResetTranslateDefaults(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj) {
  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      ChromeTranslateClient::CreateTranslatePrefs(GetPrefService());
  translate_prefs->ResetToDefaults();
}

static void MigrateJavascriptPreference(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj) {
  const PrefService::Preference* javascript_pref =
      GetPrefService()->FindPreference(prefs::kWebKitJavascriptEnabled);
  DCHECK(javascript_pref);

  if (!javascript_pref->HasUserSetting())
    return;

  bool javascript_enabled = false;
  bool retval = javascript_pref->GetValue()->GetAsBoolean(&javascript_enabled);
  DCHECK(retval);
  SetContentSettingEnabled(env, obj,
      CONTENT_SETTINGS_TYPE_JAVASCRIPT, javascript_enabled);
  GetPrefService()->ClearPref(prefs::kWebKitJavascriptEnabled);
}

static void SetPasswordEchoEnabled(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj,
                                   jboolean passwordEchoEnabled) {
  GetPrefService()->SetBoolean(prefs::kWebKitPasswordEchoEnabled,
                               passwordEchoEnabled);
}

static jboolean GetCameraEnabled(JNIEnv* env,
                                 const JavaParamRef<jobject>& obj) {
  return GetBooleanForContentSetting(CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA);
}

static jboolean GetCameraUserModifiable(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj) {
  return IsContentSettingUserModifiable(
             CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA);
}

static jboolean GetCameraManagedByCustodian(JNIEnv* env,
                                            const JavaParamRef<jobject>& obj) {
  return IsContentSettingManagedByCustodian(
             CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA);
}

static jboolean GetMicEnabled(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  return GetBooleanForContentSetting(CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC);
}

static jboolean GetMicUserModifiable(JNIEnv* env,
                                     const JavaParamRef<jobject>& obj) {
  return IsContentSettingUserModifiable(
             CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC);
}

static jboolean GetMicManagedByCustodian(JNIEnv* env,
                                         const JavaParamRef<jobject>& obj) {
  return IsContentSettingManagedByCustodian(
             CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC);
}

static void SetSearchSuggestEnabled(JNIEnv* env,
                                    const JavaParamRef<jobject>& obj,
                                    jboolean enabled) {
  GetPrefService()->SetBoolean(prefs::kSearchSuggestEnabled, enabled);
}

static ScopedJavaLocalRef<jstring> GetContextualSearchPreference(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return ConvertUTF8ToJavaString(
      env, GetPrefService()->GetString(prefs::kContextualSearchEnabled));
}

static jboolean GetContextualSearchPreferenceIsManaged(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return GetPrefService()->IsManagedPreference(prefs::kContextualSearchEnabled);
}

static void SetContextualSearchPreference(JNIEnv* env,
                                          const JavaParamRef<jobject>& obj,
                                          const JavaParamRef<jstring>& pref) {
  GetPrefService()->SetString(prefs::kContextualSearchEnabled,
      ConvertJavaStringToUTF8(env, pref));
}

static void SetNetworkPredictionEnabled(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj,
                                        jboolean enabled) {
  GetPrefService()->SetInteger(
      prefs::kNetworkPredictionOptions,
      enabled ? chrome_browser_net::NETWORK_PREDICTION_WIFI_ONLY
              : chrome_browser_net::NETWORK_PREDICTION_NEVER);
}

static jboolean ObsoleteNetworkPredictionOptionsHasUserSetting(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return GetPrefService()->GetUserPrefValue(
      prefs::kNetworkPredictionOptions) != NULL;
}

static void SetResolveNavigationErrorEnabled(JNIEnv* env,
                                             const JavaParamRef<jobject>& obj,
                                             jboolean enabled) {
  GetPrefService()->SetBoolean(prefs::kAlternateErrorPagesEnabled, enabled);
}

static jboolean GetFirstRunEulaAccepted(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj) {
  return g_browser_process->local_state()->GetBoolean(prefs::kEulaAccepted);
}

static void SetEulaAccepted(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  g_browser_process->local_state()->SetBoolean(prefs::kEulaAccepted, true);
}

static void ResetAcceptLanguages(JNIEnv* env,
                                 const JavaParamRef<jobject>& obj,
                                 const JavaParamRef<jstring>& default_locale) {
  std::string accept_languages(l10n_util::GetStringUTF8(IDS_ACCEPT_LANGUAGES));
  std::string locale_string(ConvertJavaStringToUTF8(env, default_locale));

  PrefServiceBridge::PrependToAcceptLanguagesIfNecessary(locale_string,
                                                         &accept_languages);
  GetPrefService()->SetString(prefs::kAcceptLanguages, accept_languages);
}

// Sends all information about the different versions to Java.
// From browser_about_handler.cc
static ScopedJavaLocalRef<jobject> GetAboutVersionStrings(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  std::string os_version = version_info::GetOSType();
  os_version += " " + AndroidAboutAppInfo::GetOsInfo();

  base::android::BuildInfo* android_build_info =
        base::android::BuildInfo::GetInstance();
  std::string application(android_build_info->package_label());
  application.append(" ");
  application.append(version_info::GetVersionNumber());

  return Java_PrefServiceBridge_createAboutVersionStrings(
      env, ConvertUTF8ToJavaString(env, application),
      ConvertUTF8ToJavaString(env, os_version));
}

static ScopedJavaLocalRef<jstring> GetSupervisedUserCustodianName(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return ConvertUTF8ToJavaString(
      env, GetPrefService()->GetString(prefs::kSupervisedUserCustodianName));
}

static ScopedJavaLocalRef<jstring> GetSupervisedUserCustodianEmail(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return ConvertUTF8ToJavaString(
      env, GetPrefService()->GetString(prefs::kSupervisedUserCustodianEmail));
}

static ScopedJavaLocalRef<jstring> GetSupervisedUserCustodianProfileImageURL(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return ConvertUTF8ToJavaString(
      env, GetPrefService()->GetString(
               prefs::kSupervisedUserCustodianProfileImageURL));
}

static ScopedJavaLocalRef<jstring> GetSupervisedUserSecondCustodianName(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return ConvertUTF8ToJavaString(
      env,
      GetPrefService()->GetString(prefs::kSupervisedUserSecondCustodianName));
}

static ScopedJavaLocalRef<jstring> GetSupervisedUserSecondCustodianEmail(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return ConvertUTF8ToJavaString(
      env,
      GetPrefService()->GetString(prefs::kSupervisedUserSecondCustodianEmail));
}

static ScopedJavaLocalRef<jstring>
GetSupervisedUserSecondCustodianProfileImageURL(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return ConvertUTF8ToJavaString(
      env, GetPrefService()->GetString(
               prefs::kSupervisedUserSecondCustodianProfileImageURL));
}

// static
bool PrefServiceBridge::RegisterPrefServiceBridge(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static
// This logic should be kept in sync with prependToAcceptLanguagesIfNecessary in
// chrome/android/java/src/org/chromium/chrome/browser/
//     physicalweb/PwsClientImpl.java
// Input |locales| is a comma separated locale representation that consists of
// language tags (BCP47 compliant format). Each language tag contains a language
// code and a country code or a language code only.
void PrefServiceBridge::PrependToAcceptLanguagesIfNecessary(
    const std::string& locales,
    std::string* accept_languages) {
  std::vector<std::string> locale_list =
      base::SplitString(locales + "," + *accept_languages, ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  std::set<std::string> seen_tags;
  std::vector<std::pair<std::string, std::string>> unique_locale_list;
  for (const std::string& locale_str : locale_list) {
    char locale_ID[ULOC_FULLNAME_CAPACITY] = {};
    char language_code_buffer[ULOC_LANG_CAPACITY] = {};
    char country_code_buffer[ULOC_COUNTRY_CAPACITY] = {};

    UErrorCode error = U_ZERO_ERROR;
    uloc_forLanguageTag(locale_str.c_str(), locale_ID, ULOC_FULLNAME_CAPACITY,
                        nullptr, &error);
    if (U_FAILURE(error)) {
      LOG(ERROR) << "Ignoring invalid locale representation " << locale_str;
      continue;
    }

    error = U_ZERO_ERROR;
    uloc_getLanguage(locale_ID, language_code_buffer, ULOC_LANG_CAPACITY,
                     &error);
    if (U_FAILURE(error)) {
      LOG(ERROR) << "Ignoring invalid locale representation " << locale_str;
      continue;
    }

    error = U_ZERO_ERROR;
    uloc_getCountry(locale_ID, country_code_buffer, ULOC_COUNTRY_CAPACITY,
                    &error);
    if (U_FAILURE(error)) {
      LOG(ERROR) << "Ignoring invalid locale representation " << locale_str;
      continue;
    }

    std::string language_code(language_code_buffer);
    std::string country_code(country_code_buffer);
    std::string language_tag(language_code + "-" + country_code);

    if (seen_tags.find(language_tag) != seen_tags.end())
      continue;

    seen_tags.insert(language_tag);
    unique_locale_list.push_back(std::make_pair(language_code, country_code));
  }

  // If language is not in the accept languages list, also add language
  // code. A language code should only be inserted after the last
  // languageTag that contains that language.
  // This will work with the IDS_ACCEPT_LANGUAGE localized strings bundled
  // with Chrome but may fail on arbitrary lists of language tags due to
  // differences in case and whitespace.
  std::set<std::string> seen_languages;
  std::vector<std::string> output_list;
  for (auto it = unique_locale_list.rbegin(); it != unique_locale_list.rend();
       ++it) {
    if (seen_languages.find(it->first) == seen_languages.end()) {
      output_list.push_back(it->first);
      seen_languages.insert(it->first);
    }
    if (!it->second.empty())
      output_list.push_back(it->first + "-" + it->second);
  }

  std::reverse(output_list.begin(), output_list.end());
  *accept_languages = base::JoinString(output_list, ",");
}

// static
std::string PrefServiceBridge::GetAndroidPermissionForContentSetting(
    ContentSettingsType content_type) {
  JNIEnv* env = AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> android_permission =
      Java_PrefServiceBridge_getAndroidPermissionForContentSetting(
          env, content_type);
  if (android_permission.is_null())
    return std::string();

  return ConvertJavaStringToUTF8(android_permission);
}

static void SetSupervisedUserId(JNIEnv* env,
                                const JavaParamRef<jobject>& obj,
                                const JavaParamRef<jstring>& pref) {
  GetPrefService()->SetString(prefs::kSupervisedUserId,
                              ConvertJavaStringToUTF8(env, pref));
}
