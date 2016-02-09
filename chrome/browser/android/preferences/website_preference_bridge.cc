// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/preferences/website_preference_bridge.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/browsing_data_local_storage_helper.h"
#include "chrome/browser/browsing_data/cookies_tree_model.h"
#include "chrome/browser/browsing_data/local_data_container.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/content_settings/web_site_settings_uma_util.h"
#include "chrome/browser/notifications/desktop_notification_profile_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/storage/storage_info_fetcher.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "jni/WebsitePreferenceBridge_jni.h"
#include "storage/browser/quota/quota_client.h"
#include "storage/browser/quota/quota_manager.h"
#include "url/url_constants.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using content::BrowserThread;

namespace {

Profile* GetActiveUserProfile(bool is_incognito) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (is_incognito)
    profile = profile->GetOffTheRecordProfile();
  return profile;
}

HostContentSettingsMap* GetHostContentSettingsMap(bool is_incognito) {
  return HostContentSettingsMapFactory::GetForProfile(
      GetActiveUserProfile(is_incognito));
}

typedef void (*InfoListInsertionFunction)(JNIEnv*, jobject, jstring, jstring);

void GetOrigins(JNIEnv* env,
                ContentSettingsType content_type,
                InfoListInsertionFunction insertionFunc,
                jobject list,
                jboolean managedOnly) {
  ContentSettingsForOneType all_settings;
  HostContentSettingsMap* content_settings_map =
      GetHostContentSettingsMap(false);
  content_settings_map->GetSettingsForOneType(
      content_type, std::string(), &all_settings);
  ContentSetting default_content_setting = content_settings_map->
      GetDefaultContentSetting(content_type, NULL);
  // Now add all origins that have a non-default setting to the list.
  for (const auto& settings_it : all_settings) {
    if (settings_it.setting == default_content_setting)
      continue;
    if (managedOnly &&
        HostContentSettingsMap::GetProviderTypeFromSource(settings_it.source) !=
            HostContentSettingsMap::ProviderType::POLICY_PROVIDER) {
      continue;
    }
    const std::string origin = settings_it.primary_pattern.ToString();
    const std::string embedder = settings_it.secondary_pattern.ToString();

    // The string |jorigin| is used to group permissions together in the Site
    // Settings list. In order to group sites with the same origin, remove any
    // standard port from the end of the URL if it's present (i.e. remove :443
    // for HTTPS sites and :80 for HTTP sites).
    // TODO(sashab,lgarron): Find out which settings are being saved with the
    // port and omit it if it's the standard port.
    // TODO(mvanouwerkerk): Remove all this logic and take two passes through
    // HostContentSettingsMap: once to get all the 'interesting' hosts, and once
    // (on SingleWebsitePreferences) to find permission patterns which match
    // each of these hosts.
    const char* kHttpPortSuffix = ":80";
    const char* kHttpsPortSuffix = ":443";
    ScopedJavaLocalRef<jstring> jorigin;
    if (base::StartsWith(origin, url::kHttpsScheme,
                         base::CompareCase::INSENSITIVE_ASCII) &&
        base::EndsWith(origin, kHttpsPortSuffix,
                       base::CompareCase::INSENSITIVE_ASCII)) {
      jorigin = ConvertUTF8ToJavaString(
          env, origin.substr(0, origin.size() - strlen(kHttpsPortSuffix)));
    } else if (base::StartsWith(origin, url::kHttpScheme,
                                base::CompareCase::INSENSITIVE_ASCII) &&
               base::EndsWith(origin, kHttpPortSuffix,
                              base::CompareCase::INSENSITIVE_ASCII)) {
      jorigin = ConvertUTF8ToJavaString(
          env, origin.substr(0, origin.size() - strlen(kHttpPortSuffix)));
    } else {
      jorigin = ConvertUTF8ToJavaString(env, origin);
    }

    ScopedJavaLocalRef<jstring> jembedder;
    if (embedder != origin)
      jembedder = ConvertUTF8ToJavaString(env, embedder);

    insertionFunc(env, list, jorigin.obj(), jembedder.obj());
  }
}

ContentSetting GetSettingForOrigin(JNIEnv* env,
                                   ContentSettingsType content_type,
                                   jstring origin,
                                   jstring embedder,
                                   jboolean is_incognito) {
  GURL url(ConvertJavaStringToUTF8(env, origin));
  GURL embedder_url(ConvertJavaStringToUTF8(env, embedder));
  ContentSetting setting =
      GetHostContentSettingsMap(is_incognito)
          ->GetContentSetting(url, embedder_url, content_type, std::string());
  return setting;
}

void SetSettingForOrigin(JNIEnv* env,
                         ContentSettingsType content_type,
                         jstring origin,
                         ContentSettingsPattern secondary_pattern,
                         ContentSetting setting,
                         jboolean is_incognito) {
  GURL url(ConvertJavaStringToUTF8(env, origin));
  GetHostContentSettingsMap(is_incognito)
      ->SetContentSetting(ContentSettingsPattern::FromURLNoWildcard(url),
                          secondary_pattern, content_type, std::string(),
                          setting);
  WebSiteSettingsUmaUtil::LogPermissionChange(content_type, setting);
}

}  // anonymous namespace

static void GetFullscreenOrigins(JNIEnv* env,
                                 const JavaParamRef<jclass>& clazz,
                                 const JavaParamRef<jobject>& list,
                                 jboolean managedOnly) {
  GetOrigins(env, CONTENT_SETTINGS_TYPE_FULLSCREEN,
             &Java_WebsitePreferenceBridge_insertFullscreenInfoIntoList, list,
             managedOnly);
}

static jint GetFullscreenSettingForOrigin(JNIEnv* env,
                                          const JavaParamRef<jclass>& clazz,
                                          const JavaParamRef<jstring>& origin,
                                          const JavaParamRef<jstring>& embedder,
                                          jboolean is_incognito) {
  return GetSettingForOrigin(env, CONTENT_SETTINGS_TYPE_FULLSCREEN, origin,
                             embedder, is_incognito);
}

static void SetFullscreenSettingForOrigin(JNIEnv* env,
                                          const JavaParamRef<jclass>& clazz,
                                          const JavaParamRef<jstring>& origin,
                                          const JavaParamRef<jstring>& embedder,
                                          jint value,
                                          jboolean is_incognito) {
  GURL embedder_url(ConvertJavaStringToUTF8(env, embedder));
  SetSettingForOrigin(env, CONTENT_SETTINGS_TYPE_FULLSCREEN, origin,
                      ContentSettingsPattern::FromURLNoWildcard(embedder_url),
                      (ContentSetting) value, is_incognito);
}

static void GetGeolocationOrigins(JNIEnv* env,
                                  const JavaParamRef<jclass>& clazz,
                                  const JavaParamRef<jobject>& list,
                                  jboolean managedOnly) {
  GetOrigins(env, CONTENT_SETTINGS_TYPE_GEOLOCATION,
             &Java_WebsitePreferenceBridge_insertGeolocationInfoIntoList, list,
             managedOnly);
}

static jint GetGeolocationSettingForOrigin(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jstring>& origin,
    const JavaParamRef<jstring>& embedder,
    jboolean is_incognito) {
  return GetSettingForOrigin(env, CONTENT_SETTINGS_TYPE_GEOLOCATION, origin,
                             embedder, is_incognito);
}

static void SetGeolocationSettingForOrigin(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jstring>& origin,
    const JavaParamRef<jstring>& embedder,
    jint value,
    jboolean is_incognito) {
  GURL embedder_url(ConvertJavaStringToUTF8(env, embedder));
  SetSettingForOrigin(env, CONTENT_SETTINGS_TYPE_GEOLOCATION, origin,
                      ContentSettingsPattern::FromURLNoWildcard(embedder_url),
                      (ContentSetting) value, is_incognito);
}

static void GetKeygenOrigins(JNIEnv* env,
                             const JavaParamRef<jclass>& clazz,
                             const JavaParamRef<jobject>& list) {
  GetOrigins(env, CONTENT_SETTINGS_TYPE_KEYGEN,
             &Java_WebsitePreferenceBridge_insertKeygenInfoIntoList, list,
             false);
}

static jint GetKeygenSettingForOrigin(JNIEnv* env,
                                      const JavaParamRef<jclass>& clazz,
                                      const JavaParamRef<jstring>& origin,
                                      const JavaParamRef<jstring>& embedder,
                                      jboolean is_incognito) {
  return GetSettingForOrigin(env, CONTENT_SETTINGS_TYPE_KEYGEN, origin,
                             embedder, is_incognito);
}

static void SetKeygenSettingForOrigin(JNIEnv* env,
                                      const JavaParamRef<jclass>& clazz,
                                      const JavaParamRef<jstring>& origin,
                                      const JavaParamRef<jstring>& embedder,
                                      jint value,
                                      jboolean is_incognito) {
  GURL embedder_url(ConvertJavaStringToUTF8(env, embedder));
  SetSettingForOrigin(env, CONTENT_SETTINGS_TYPE_KEYGEN, origin,
                      ContentSettingsPattern::FromURLNoWildcard(embedder_url),
                      (ContentSetting) value, is_incognito);
}

static jboolean GetKeygenBlocked(JNIEnv* env,
                             const JavaParamRef<jclass>& clazz,
                             const JavaParamRef<jobject>& java_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);
  return TabSpecificContentSettings::FromWebContents(
      web_contents)->IsContentBlocked(CONTENT_SETTINGS_TYPE_KEYGEN);
}

static void GetMidiOrigins(JNIEnv* env,
                           const JavaParamRef<jclass>& clazz,
                           const JavaParamRef<jobject>& list) {
  GetOrigins(env, CONTENT_SETTINGS_TYPE_MIDI_SYSEX,
             &Java_WebsitePreferenceBridge_insertMidiInfoIntoList, list, false);
}

static jint GetMidiSettingForOrigin(JNIEnv* env,
                                    const JavaParamRef<jclass>& clazz,
                                    const JavaParamRef<jstring>& origin,
                                    const JavaParamRef<jstring>& embedder,
                                    jboolean is_incognito) {
  return GetSettingForOrigin(env, CONTENT_SETTINGS_TYPE_MIDI_SYSEX, origin,
                             embedder, is_incognito);
}

static void SetMidiSettingForOrigin(JNIEnv* env,
                                    const JavaParamRef<jclass>& clazz,
                                    const JavaParamRef<jstring>& origin,
                                    const JavaParamRef<jstring>& embedder,
                                    jint value,
                                    jboolean is_incognito) {
  GURL embedder_url(ConvertJavaStringToUTF8(env, embedder));
  SetSettingForOrigin(env, CONTENT_SETTINGS_TYPE_MIDI_SYSEX, origin,
                      ContentSettingsPattern::FromURLNoWildcard(embedder_url),
                      (ContentSetting) value, is_incognito);
}

static void GetProtectedMediaIdentifierOrigins(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jobject>& list) {
  GetOrigins(
      env, CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER,
      &Java_WebsitePreferenceBridge_insertProtectedMediaIdentifierInfoIntoList,
      list, false);
}

static jint GetProtectedMediaIdentifierSettingForOrigin(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jstring>& origin,
    const JavaParamRef<jstring>& embedder,
    jboolean is_incognito) {
  return GetSettingForOrigin(env,
                             CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER,
                             origin, embedder, is_incognito);
}

static void SetProtectedMediaIdentifierSettingForOrigin(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jstring>& origin,
    const JavaParamRef<jstring>& embedder,
    jint value,
    jboolean is_incognito) {
  GURL embedder_url(ConvertJavaStringToUTF8(env, embedder));
  SetSettingForOrigin(env, CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER,
                      origin,
                      ContentSettingsPattern::FromURLNoWildcard(embedder_url),
                      (ContentSetting) value, is_incognito);
}

static void GetPushNotificationOrigins(JNIEnv* env,
                                       const JavaParamRef<jclass>& clazz,
                                       const JavaParamRef<jobject>& list) {
  GetOrigins(env, CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
             &Java_WebsitePreferenceBridge_insertPushNotificationIntoList, list,
             false);
}

static jint GetPushNotificationSettingForOrigin(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jstring>& origin,
    const JavaParamRef<jstring>& embedder,
    jboolean is_incognito) {
  return DesktopNotificationProfileUtil::GetContentSetting(
      GetActiveUserProfile(is_incognito),
      GURL(ConvertJavaStringToUTF8(env, origin)));
}

static void SetPushNotificationSettingForOrigin(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jstring>& origin,
    const JavaParamRef<jstring>& embedder,
    jint value,
    jboolean is_incognito) {
  // TODO(peter): Web Notification permission behaves differently from all other
  // permission types. See https://crbug.com/416894.
  Profile* profile = GetActiveUserProfile(is_incognito);
  GURL url = GURL(ConvertJavaStringToUTF8(env, origin));
  ContentSetting setting = (ContentSetting) value;
  switch (setting) {
    case CONTENT_SETTING_DEFAULT:
      DesktopNotificationProfileUtil::ClearSetting(
          profile, ContentSettingsPattern::FromURLNoWildcard(url));
      break;
    case CONTENT_SETTING_ALLOW:
      DesktopNotificationProfileUtil::GrantPermission(profile, url);
      break;
    case CONTENT_SETTING_BLOCK:
      DesktopNotificationProfileUtil::DenyPermission(profile, url);
      break;
    default:
      NOTREACHED();
  }
  WebSiteSettingsUmaUtil::LogPermissionChange(
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS, setting);
}

static void GetCameraOrigins(JNIEnv* env,
                             const JavaParamRef<jclass>& clazz,
                             const JavaParamRef<jobject>& list,
                             jboolean managedOnly) {
  GetOrigins(env, CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA,
             &Java_WebsitePreferenceBridge_insertCameraInfoIntoList, list,
             managedOnly);
}

static void GetMicrophoneOrigins(JNIEnv* env,
                                 const JavaParamRef<jclass>& clazz,
                                 const JavaParamRef<jobject>& list,
                                 jboolean managedOnly) {
  GetOrigins(env, CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
             &Java_WebsitePreferenceBridge_insertMicrophoneInfoIntoList, list,
             managedOnly);
}

static jint GetMicrophoneSettingForOrigin(JNIEnv* env,
                                          const JavaParamRef<jclass>& clazz,
                                          const JavaParamRef<jstring>& origin,
                                          const JavaParamRef<jstring>& embedder,
                                          jboolean is_incognito) {
  return GetSettingForOrigin(env, CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC, origin,
                             embedder, is_incognito);
}

static jint GetCameraSettingForOrigin(JNIEnv* env,
                                      const JavaParamRef<jclass>& clazz,
                                      const JavaParamRef<jstring>& origin,
                                      const JavaParamRef<jstring>& embedder,
                                      jboolean is_incognito) {
  return GetSettingForOrigin(env, CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA,
                             origin, embedder, is_incognito);
}

static void SetMicrophoneSettingForOrigin(JNIEnv* env,
                                          const JavaParamRef<jclass>& clazz,
                                          const JavaParamRef<jstring>& origin,
                                          const JavaParamRef<jstring>& embedder,
                                          jint value,
                                          jboolean is_incognito) {
  SetSettingForOrigin(env, CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC, origin,
                      ContentSettingsPattern::Wildcard(),
                      (ContentSetting) value, is_incognito);
}

static void SetCameraSettingForOrigin(JNIEnv* env,
                                      const JavaParamRef<jclass>& clazz,
                                      const JavaParamRef<jstring>& origin,
                                      const JavaParamRef<jstring>& embedder,
                                      jint value,
                                      jboolean is_incognito) {
  SetSettingForOrigin(env, CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA, origin,
                      ContentSettingsPattern::Wildcard(),
                      (ContentSetting) value, is_incognito);
}

static scoped_refptr<content_settings::CookieSettings> GetCookieSettings() {
  // A single cookie setting applies to both incognito and non-incognito.
  Profile* profile = ProfileManager::GetActiveUserProfile();
  return CookieSettingsFactory::GetForProfile(profile);
}

static void GetCookieOrigins(JNIEnv* env,
                             const JavaParamRef<jclass>& clazz,
                             const JavaParamRef<jobject>& list,
                             jboolean managedOnly) {
  ContentSettingsForOneType all_settings;
  GetCookieSettings()->GetCookieSettings(&all_settings);
  const ContentSetting default_setting =
      GetCookieSettings()->GetDefaultCookieSetting(nullptr);
  for (const auto& settings_it : all_settings) {
    if (settings_it.setting == default_setting)
      continue;
    if (managedOnly &&
        HostContentSettingsMap::GetProviderTypeFromSource(settings_it.source) !=
            HostContentSettingsMap::ProviderType::POLICY_PROVIDER) {
      continue;
    }
    const std::string& origin = settings_it.primary_pattern.ToString();
    const std::string& embedder = settings_it.secondary_pattern.ToString();
    ScopedJavaLocalRef<jstring> jorigin = ConvertUTF8ToJavaString(env, origin);
    ScopedJavaLocalRef<jstring> jembedder;
    if (embedder != origin)
      jembedder = ConvertUTF8ToJavaString(env, embedder);
    Java_WebsitePreferenceBridge_insertCookieInfoIntoList(env, list,
        jorigin.obj(), jembedder.obj());
  }
}

static jint GetCookieSettingForOrigin(JNIEnv* env,
                                      const JavaParamRef<jclass>& clazz,
                                      const JavaParamRef<jstring>& origin,
                                      const JavaParamRef<jstring>& embedder,
                                      jboolean is_incognito) {
  return GetSettingForOrigin(env, CONTENT_SETTINGS_TYPE_COOKIES, origin,
                             embedder, false);
}

static void SetCookieSettingForOrigin(JNIEnv* env,
                                      const JavaParamRef<jclass>& clazz,
                                      const JavaParamRef<jstring>& origin,
                                      const JavaParamRef<jstring>& embedder,
                                      jint value,
                                      jboolean is_incognito) {
  GURL url(ConvertJavaStringToUTF8(env, origin));
  ContentSettingsPattern primary_pattern(
      ContentSettingsPattern::FromURLNoWildcard(url));
  ContentSettingsPattern secondary_pattern(ContentSettingsPattern::Wildcard());
  ContentSetting setting = CONTENT_SETTING_DEFAULT;
  if (value == -1) {
    GetCookieSettings()->ResetCookieSetting(primary_pattern, secondary_pattern);
  } else {
    setting = value ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK;
    GetCookieSettings()->SetCookieSetting(primary_pattern, secondary_pattern,
                                          setting);
  }
  WebSiteSettingsUmaUtil::LogPermissionChange(
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS, setting);
}

static jboolean IsContentSettingsPatternValid(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jstring>& pattern) {
  return ContentSettingsPattern::FromString(
      ConvertJavaStringToUTF8(env, pattern)).IsValid();
}

static jboolean UrlMatchesContentSettingsPattern(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jstring>& jurl,
    const JavaParamRef<jstring>& jpattern) {
  ContentSettingsPattern pattern = ContentSettingsPattern::FromString(
      ConvertJavaStringToUTF8(env, jpattern));
  return pattern.Matches(GURL(ConvertJavaStringToUTF8(env, jurl)));
}

namespace {

class SiteDataDeleteHelper :
      public base::RefCountedThreadSafe<SiteDataDeleteHelper>,
      public CookiesTreeModel::Observer {
 public:
  SiteDataDeleteHelper(Profile* profile, const GURL& domain)
      : profile_(profile), domain_(domain), ending_batch_processing_(false) {
  }

  void Run() {
    AddRef();  // Balanced in TreeModelEndBatch.

    content::StoragePartition* storage_partition =
        content::BrowserContext::GetDefaultStoragePartition(profile_);
    content::IndexedDBContext* indexed_db_context =
        storage_partition->GetIndexedDBContext();
    content::ServiceWorkerContext* service_worker_context =
        storage_partition->GetServiceWorkerContext();
    content::CacheStorageContext* cache_storage_context =
        storage_partition->GetCacheStorageContext();
    storage::FileSystemContext* file_system_context =
        storage_partition->GetFileSystemContext();
    LocalDataContainer* container = new LocalDataContainer(
        new BrowsingDataCookieHelper(profile_->GetRequestContext()),
        new BrowsingDataDatabaseHelper(profile_),
        new BrowsingDataLocalStorageHelper(profile_),
        NULL,
        new BrowsingDataAppCacheHelper(profile_),
        new BrowsingDataIndexedDBHelper(indexed_db_context),
        BrowsingDataFileSystemHelper::Create(file_system_context),
        BrowsingDataQuotaHelper::Create(profile_),
        BrowsingDataChannelIDHelper::Create(profile_->GetRequestContext()),
        new BrowsingDataServiceWorkerHelper(service_worker_context),
        new BrowsingDataCacheStorageHelper(cache_storage_context),
        NULL);

    cookies_tree_model_.reset(new CookiesTreeModel(
        container, profile_->GetExtensionSpecialStoragePolicy(), false));
    cookies_tree_model_->AddCookiesTreeObserver(this);
  }

  // TreeModelObserver:
  void TreeNodesAdded(ui::TreeModel* model,
                              ui::TreeModelNode* parent,
                              int start,
                              int count) override {}
  void TreeNodesRemoved(ui::TreeModel* model,
                                ui::TreeModelNode* parent,
                                int start,
                                int count) override {}

  // CookiesTreeModel::Observer:
  void TreeNodeChanged(ui::TreeModel* model, ui::TreeModelNode* node) override {
  }

  void TreeModelBeginBatch(CookiesTreeModel* model) override {
    DCHECK(!ending_batch_processing_);  // Extra batch-start sent.
  }

  void TreeModelEndBatch(CookiesTreeModel* model) override {
    DCHECK(!ending_batch_processing_);  // Already in end-stage.
    ending_batch_processing_ = true;

    RecursivelyFindSiteAndDelete(cookies_tree_model_->GetRoot());

    // This will result in this class getting deleted.
    Release();
  }

  void RecursivelyFindSiteAndDelete(CookieTreeNode* node) {
    CookieTreeNode::DetailedInfo info = node->GetDetailedInfo();
    for (int i = node->child_count(); i > 0; --i)
      RecursivelyFindSiteAndDelete(node->GetChild(i - 1));

    if (info.node_type == CookieTreeNode::DetailedInfo::TYPE_COOKIE &&
        info.cookie &&
        domain_.DomainIs(info.cookie->Domain().c_str()))
      cookies_tree_model_->DeleteCookieNode(node);
  }

 private:
  friend class base::RefCountedThreadSafe<SiteDataDeleteHelper>;

  ~SiteDataDeleteHelper() override {}

  Profile* profile_;

  // The domain we want to delete data for.
  GURL domain_;

  // Keeps track of when we're ready to close batch processing.
  bool ending_batch_processing_;

  scoped_ptr<CookiesTreeModel> cookies_tree_model_;

  DISALLOW_COPY_AND_ASSIGN(SiteDataDeleteHelper);
};

class StorageInfoReadyCallback {
 public:
  explicit StorageInfoReadyCallback(const JavaRef<jobject>& java_callback)
      : env_(base::android::AttachCurrentThread()),
        java_callback_(java_callback) {
  }

  void OnStorageInfoReady(const storage::UsageInfoEntries& entries) {
    ScopedJavaLocalRef<jobject> list =
        Java_WebsitePreferenceBridge_createStorageInfoList(env_);

    storage::UsageInfoEntries::const_iterator i;
    for (i = entries.begin(); i != entries.end(); ++i) {
      if (i->usage <= 0) continue;
      ScopedJavaLocalRef<jstring> host =
          ConvertUTF8ToJavaString(env_, i->host);

      Java_WebsitePreferenceBridge_insertStorageInfoIntoList(
          env_, list.obj(), host.obj(), i->type, i->usage);
    }

    Java_StorageInfoReadyCallback_onStorageInfoReady(
        env_, java_callback_.obj(), list.obj());

    delete this;
  }

 private:
  JNIEnv* env_;
  ScopedJavaGlobalRef<jobject> java_callback_;
};

class StorageInfoClearedCallback {
 public:
  explicit StorageInfoClearedCallback(const JavaRef<jobject>& java_callback)
      : env_(base::android::AttachCurrentThread()),
        java_callback_(java_callback) {
  }

  void OnStorageInfoCleared(storage::QuotaStatusCode code) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    Java_StorageInfoClearedCallback_onStorageInfoCleared(
        env_, java_callback_.obj());

    delete this;
  }

 private:
  JNIEnv* env_;
  ScopedJavaGlobalRef<jobject> java_callback_;
};

class LocalStorageInfoReadyCallback {
 public:
  explicit LocalStorageInfoReadyCallback(const JavaRef<jobject>& java_callback)
      : env_(base::android::AttachCurrentThread()),
        java_callback_(java_callback) {
  }

  void OnLocalStorageModelInfoLoaded(
      const std::list<BrowsingDataLocalStorageHelper::LocalStorageInfo>&
          local_storage_info) {
    ScopedJavaLocalRef<jobject> map =
        Java_WebsitePreferenceBridge_createLocalStorageInfoMap(env_);

    std::list<BrowsingDataLocalStorageHelper::LocalStorageInfo>::const_iterator
        i;
    for (i = local_storage_info.begin(); i != local_storage_info.end(); ++i) {
      ScopedJavaLocalRef<jstring> full_origin =
          ConvertUTF8ToJavaString(env_, i->origin_url.spec());
      // Remove the trailing backslash so the origin is matched correctly in
      // SingleWebsitePreferences.mergePermissionInfoForTopLevelOrigin.
      std::string origin_str = i->origin_url.GetOrigin().spec();
      DCHECK(origin_str[origin_str.size() - 1] == '/');
      origin_str = origin_str.substr(0, origin_str.size() - 1);
      ScopedJavaLocalRef<jstring> origin =
          ConvertUTF8ToJavaString(env_, origin_str);
      Java_WebsitePreferenceBridge_insertLocalStorageInfoIntoMap(
          env_, map.obj(), origin.obj(), full_origin.obj(), i->size);
    }

    Java_LocalStorageInfoReadyCallback_onLocalStorageInfoReady(
        env_, java_callback_.obj(), map.obj());
    delete this;
  }

 private:
  JNIEnv* env_;
  ScopedJavaGlobalRef<jobject> java_callback_;
};

}  // anonymous namespace

// TODO(jknotten): These methods should not be static. Instead we should
// expose a class to Java so that the fetch requests can be cancelled,
// and manage the lifetimes of the callback (and indirectly the helper
// by having a reference to it).

// The helper methods (StartFetching, DeleteLocalStorageFile, DeleteDatabase)
// are asynchronous. A "use after free" error is not possible because the
// helpers keep a reference to themselves for the duration of their tasks,
// which includes callback invocation.

static void FetchLocalStorageInfo(JNIEnv* env,
                                  const JavaParamRef<jclass>& clazz,
                                  const JavaParamRef<jobject>& java_callback) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  scoped_refptr<BrowsingDataLocalStorageHelper> local_storage_helper(
      new BrowsingDataLocalStorageHelper(profile));
  // local_storage_callback will delete itself when it is run.
  LocalStorageInfoReadyCallback* local_storage_callback =
      new LocalStorageInfoReadyCallback(java_callback);
  local_storage_helper->StartFetching(
      base::Bind(&LocalStorageInfoReadyCallback::OnLocalStorageModelInfoLoaded,
                 base::Unretained(local_storage_callback)));
}

static void FetchStorageInfo(JNIEnv* env,
                             const JavaParamRef<jclass>& clazz,
                             const JavaParamRef<jobject>& java_callback) {
  Profile* profile = ProfileManager::GetActiveUserProfile();

  // storage_info_ready_callback will delete itself when it is run.
  StorageInfoReadyCallback* storage_info_ready_callback =
      new StorageInfoReadyCallback(java_callback);
  scoped_refptr<StorageInfoFetcher> storage_info_fetcher =
      new StorageInfoFetcher(profile);
  storage_info_fetcher->FetchStorageInfo(
      base::Bind(&StorageInfoReadyCallback::OnStorageInfoReady,
          base::Unretained(storage_info_ready_callback)));
}

static void ClearLocalStorageData(JNIEnv* env,
                                  const JavaParamRef<jclass>& clazz,
                                  const JavaParamRef<jstring>& jorigin) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  scoped_refptr<BrowsingDataLocalStorageHelper> local_storage_helper =
      new BrowsingDataLocalStorageHelper(profile);
  GURL origin_url = GURL(ConvertJavaStringToUTF8(env, jorigin));
  local_storage_helper->DeleteOrigin(origin_url);
}

static void ClearStorageData(JNIEnv* env,
                             const JavaParamRef<jclass>& clazz,
                             const JavaParamRef<jstring>& jhost,
                             jint type,
                             const JavaParamRef<jobject>& java_callback) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::string host = ConvertJavaStringToUTF8(env, jhost);

  // storage_info_cleared_callback will delete itself when it is run.
  StorageInfoClearedCallback* storage_info_cleared_callback =
      new StorageInfoClearedCallback(java_callback);
  scoped_refptr<StorageInfoFetcher> storage_info_fetcher =
      new StorageInfoFetcher(profile);
  storage_info_fetcher->ClearStorage(
      host,
      static_cast<storage::StorageType>(type),
      base::Bind(&StorageInfoClearedCallback::OnStorageInfoCleared,
          base::Unretained(storage_info_cleared_callback)));
}

static void ClearCookieData(JNIEnv* env,
                            const JavaParamRef<jclass>& clazz,
                            const JavaParamRef<jstring>& jorigin) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  GURL url(ConvertJavaStringToUTF8(env, jorigin));
  scoped_refptr<SiteDataDeleteHelper> site_data_deleter(
      new SiteDataDeleteHelper(profile, url));
  site_data_deleter->Run();
}

// Register native methods
bool RegisterWebsitePreferenceBridge(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
