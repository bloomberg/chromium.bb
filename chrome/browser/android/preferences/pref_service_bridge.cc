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
#include "base/scoped_observer.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/android/preferences/prefs.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/android/android_about_app_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/omnibox/browser/omnibox_pref_names.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "components/strings/grit/components_locale_settings.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/version_info/version_info.h"
#include "components/web_resource/web_resource_pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "jni/PrefServiceBridge_jni.h"
#include "third_party/icu/source/common/unicode/uloc.h"
#include "ui/base/l10n/l10n_util.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace {

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

browsing_data::ClearBrowsingDataTab ToTabEnum(jint clear_browsing_data_tab) {
  DCHECK_GE(clear_browsing_data_tab, 0);
  DCHECK_LT(clear_browsing_data_tab,
            static_cast<int>(browsing_data::ClearBrowsingDataTab::NUM_TYPES));

  return static_cast<browsing_data::ClearBrowsingDataTab>(
      clear_browsing_data_tab);
}

}  // namespace

// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------

static jboolean GetBoolean(JNIEnv* env,
                           const JavaParamRef<jobject>& obj,
                           const jint j_pref_index) {
  return GetPrefService()->GetBoolean(
      PrefServiceBridge::GetPrefNameExposedToJava(j_pref_index));
}

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
         content_settings_type == CONTENT_SETTINGS_TYPE_POPUPS ||
         content_settings_type == CONTENT_SETTINGS_TYPE_ADS);
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
         content_settings_type == CONTENT_SETTINGS_TYPE_POPUPS ||
         content_settings_type == CONTENT_SETTINGS_TYPE_ADS);

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
        entries[i].GetContentSetting(),
        ConvertUTF8ToJavaString(env, entries[i].source));
  }
}

static jboolean GetAcceptCookiesEnabled(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj) {
  return GetBooleanForContentSetting(CONTENT_SETTINGS_TYPE_COOKIES);
}

static jboolean GetAcceptCookiesUserModifiable(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return IsContentSettingUserModifiable(CONTENT_SETTINGS_TYPE_COOKIES);
}

static jboolean GetAcceptCookiesManagedByCustodian(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return IsContentSettingManagedByCustodian(CONTENT_SETTINGS_TYPE_COOKIES);
}

static jboolean GetAutoplayEnabled(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj) {
  return GetBooleanForContentSetting(CONTENT_SETTINGS_TYPE_AUTOPLAY);
}

static jboolean GetSoundEnabled(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  return GetBooleanForContentSetting(CONTENT_SETTINGS_TYPE_SOUND);
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
      password_manager::prefs::kCredentialsEnableService);
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
      password_manager::prefs::kCredentialsEnableService);
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

static jboolean IsScoutExtendedReportingActive(
    JNIEnv* env, const JavaParamRef<jobject>& obj) {
  return safe_browsing::IsScout(*GetPrefService());
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
  safe_browsing::SetExtendedReportingPrefAndMetric(
      GetPrefService(), enabled,
      safe_browsing::SBER_OPTIN_SITE_ANDROID_SETTINGS);
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

static jboolean GetBrowsingDataDeletionPreference(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint data_type,
    jint clear_browsing_data_tab) {
  DCHECK_GE(data_type, 0);
  DCHECK_LT(data_type,
            static_cast<int>(browsing_data::BrowsingDataType::NUM_TYPES));

  // If there is no corresponding preference for this |data_type|, pretend
  // that it's set to false.
  // TODO(msramek): Consider defining native-side preferences for all Java UI
  // data types for consistency.
  std::string pref;
  if (!browsing_data::GetDeletionPreferenceFromDataType(
          static_cast<browsing_data::BrowsingDataType>(data_type),
          ToTabEnum(clear_browsing_data_tab), &pref)) {
    return false;
  }

  return GetOriginalProfile()->GetPrefs()->GetBoolean(pref);
}

static void SetBrowsingDataDeletionPreference(JNIEnv* env,
                                              const JavaParamRef<jobject>& obj,
                                              jint data_type,
                                              jint clear_browsing_data_tab,
                                              jboolean value) {
  DCHECK_GE(data_type, 0);
  DCHECK_LT(data_type,
            static_cast<int>(browsing_data::BrowsingDataType::NUM_TYPES));

  std::string pref;
  if (!browsing_data::GetDeletionPreferenceFromDataType(
          static_cast<browsing_data::BrowsingDataType>(data_type),
          ToTabEnum(clear_browsing_data_tab), &pref)) {
    return;
  }

  GetOriginalProfile()->GetPrefs()->SetBoolean(pref, value);
}

static jint GetBrowsingDataDeletionTimePeriod(JNIEnv* env,
                                              const JavaParamRef<jobject>& obj,
                                              jint clear_browsing_data_tab) {
  return GetPrefService()->GetInteger(
      browsing_data::GetTimePeriodPreferenceName(
          ToTabEnum(clear_browsing_data_tab)));
}

static void SetBrowsingDataDeletionTimePeriod(JNIEnv* env,
                                              const JavaParamRef<jobject>& obj,
                                              jint clear_browsing_data_tab,
                                              jint time_period) {
  DCHECK_GE(time_period, 0);
  DCHECK_LE(time_period,
            static_cast<int>(browsing_data::TimePeriod::TIME_PERIOD_LAST));
  const char* pref_name = browsing_data::GetTimePeriodPreferenceName(
      ToTabEnum(clear_browsing_data_tab));
  int previous_value = GetPrefService()->GetInteger(pref_name);
  if (time_period != previous_value) {
    browsing_data::RecordTimePeriodChange(
        static_cast<browsing_data::TimePeriod>(time_period));
    GetPrefService()->SetInteger(pref_name, time_period);
  }
}

static jint GetLastClearBrowsingDataTab(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj) {
  return GetPrefService()->GetInteger(
      browsing_data::prefs::kLastClearBrowsingDataTab);
}

static void SetLastClearBrowsingDataTab(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj,
                                        jint tab_index) {
  DCHECK_GE(tab_index, 0);
  DCHECK_LT(tab_index, 2);
  GetPrefService()->SetInteger(browsing_data::prefs::kLastClearBrowsingDataTab,
                               tab_index);
}

static void MigrateBrowsingDataPreferences(JNIEnv* env,
                                           const JavaParamRef<jobject>& obj) {
  browsing_data::MigratePreferencesToBasic(GetOriginalProfile()->GetPrefs());
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

static void SetSoundEnabled(JNIEnv* env,
                            const JavaParamRef<jobject>& obj,
                            jboolean allow) {
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(GetOriginalProfile());
  host_content_settings_map->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_SOUND,
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
      password_manager::prefs::kCredentialsEnableService, allow);
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
void PrefServiceBridge::GetAndroidPermissionsForContentSetting(
    ContentSettingsType content_type,
    std::vector<std::string>* out) {
  JNIEnv* env = AttachCurrentThread();
  base::android::AppendJavaStringArrayToStringVector(
      env, Java_PrefServiceBridge_getAndroidPermissionsForContentSetting(
               env, content_type)
               .obj(),
      out);
}

static void SetSupervisedUserId(JNIEnv* env,
                                const JavaParamRef<jobject>& obj,
                                const JavaParamRef<jstring>& pref) {
  GetPrefService()->SetString(prefs::kSupervisedUserId,
                              ConvertJavaStringToUTF8(env, pref));
}

static void SetChromeHomePersonalizedOmniboxSuggestionsEnabled(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean is_enabled) {
  GetPrefService()->SetBoolean(omnibox::kZeroSuggestChromeHomePersonalized,
                               is_enabled);
}

const char* PrefServiceBridge::GetPrefNameExposedToJava(int pref_index) {
  DCHECK_GE(pref_index, 0);
  DCHECK_LT(pref_index, Pref::PREF_NUM_PREFS);
  return kPrefsExposedToJava[pref_index];
}
