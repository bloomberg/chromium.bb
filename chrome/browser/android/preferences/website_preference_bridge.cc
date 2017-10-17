// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>
#include <vector>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "chrome/browser/android/search_geolocation/search_geolocation_service.h"
#include "chrome/browser/browsing_data/browsing_data_flash_lso_helper.h"
#include "chrome/browser/browsing_data/browsing_data_local_storage_helper.h"
#include "chrome/browser/browsing_data/browsing_data_quota_helper.h"
#include "chrome/browser/browsing_data/cookies_tree_model.h"
#include "chrome/browser/browsing_data/local_data_container.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/content_settings/web_site_settings_uma_util.h"
#include "chrome/browser/engagement/important_sites_util.h"
#include "chrome/browser/notifications/desktop_notification_profile_util.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker.h"
#include "chrome/browser/permissions/permission_manager.h"
#include "chrome/browser/permissions/permission_uma_util.h"
#include "chrome/browser/permissions/permission_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/storage/storage_info_fetcher.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "jni/WebsitePreferenceBridge_jni.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/common/quota/quota_status_code.h"
#include "url/origin.h"
#include "url/url_constants.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using content::BrowserThread;

namespace {
// We need to limit our size due to the algorithm in ImportantSiteUtil, but we
// want to be more on the liberal side here as we're not exposing these sites
// to the user, we're just using them for our 'clear unimportant' feature in
// ManageSpaceActivity.java.
const int kMaxImportantSites = 10;

const char* kHttpPortSuffix = ":80";
const char* kHttpsPortSuffix = ":443";

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

ScopedJavaLocalRef<jstring> ConvertOriginToJavaString(
    JNIEnv* env,
    const std::string& origin) {
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
  if (base::StartsWith(origin, url::kHttpsScheme,
                       base::CompareCase::INSENSITIVE_ASCII) &&
      base::EndsWith(origin, kHttpsPortSuffix,
                     base::CompareCase::INSENSITIVE_ASCII)) {
    return ConvertUTF8ToJavaString(
        env, origin.substr(0, origin.size() - strlen(kHttpsPortSuffix)));
  } else if (base::StartsWith(origin, url::kHttpScheme,
                              base::CompareCase::INSENSITIVE_ASCII) &&
             base::EndsWith(origin, kHttpPortSuffix,
                            base::CompareCase::INSENSITIVE_ASCII)) {
    return ConvertUTF8ToJavaString(
        env, origin.substr(0, origin.size() - strlen(kHttpPortSuffix)));
  } else {
    return ConvertUTF8ToJavaString(env, origin);
  }
}

typedef void (*InfoListInsertionFunction)(
    JNIEnv*,
    const base::android::JavaRef<jobject>&,
    const base::android::JavaRef<jstring>&,
    const base::android::JavaRef<jstring>&);

void GetOrigins(JNIEnv* env,
                ContentSettingsType content_type,
                InfoListInsertionFunction insertionFunc,
                const JavaRef<jobject>& list,
                jboolean managedOnly) {
  HostContentSettingsMap* content_settings_map =
      GetHostContentSettingsMap(false);  // is_incognito
  ContentSettingsForOneType all_settings;
  ContentSettingsForOneType embargo_settings;

  content_settings_map->GetSettingsForOneType(
      content_type, std::string(), &all_settings);
  content_settings_map->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_PERMISSION_AUTOBLOCKER_DATA, std::string(),
      &embargo_settings);
  ContentSetting default_content_setting = content_settings_map->
      GetDefaultContentSetting(content_type, NULL);

  // Use a vector since the overall number of origins should be small.
  std::vector<std::string> seen_origins;

  // Now add all origins that have a non-default setting to the list.
  for (const auto& settings_it : all_settings) {
    if (settings_it.GetContentSetting() == default_content_setting)
      continue;
    if (managedOnly &&
        HostContentSettingsMap::GetProviderTypeFromSource(settings_it.source) !=
            HostContentSettingsMap::ProviderType::POLICY_PROVIDER) {
      continue;
    }
    const std::string origin = settings_it.primary_pattern.ToString();
    const std::string embedder = settings_it.secondary_pattern.ToString();

    ScopedJavaLocalRef<jstring> jembedder;
    if (embedder != origin)
      jembedder = ConvertUTF8ToJavaString(env, embedder);

    seen_origins.push_back(origin);
    insertionFunc(env, list, ConvertOriginToJavaString(env, origin), jembedder);
  }

  // Add any origins which have a default content setting value (thus skipped
  // above), but have been automatically blocked for this permission type.
  // We use an empty embedder since embargo doesn't care about it.
  PermissionDecisionAutoBlocker* auto_blocker =
      PermissionDecisionAutoBlocker::GetForProfile(
          GetActiveUserProfile(false /* is_incognito */));
  ScopedJavaLocalRef<jstring> jembedder;

  for (const auto& settings_it : embargo_settings) {
    const std::string origin = settings_it.primary_pattern.ToString();
    if (base::ContainsValue(seen_origins, origin)) {
      // This origin has already been added to the list, so don't add it again.
      continue;
    }

    if (auto_blocker->GetEmbargoResult(GURL(origin), content_type)
            .content_setting == CONTENT_SETTING_BLOCK) {
      seen_origins.push_back(origin);
      insertionFunc(env, list, ConvertOriginToJavaString(env, origin),
                    jembedder);
    }
  }

  // Add the DSE origin if it allows geolocation.
  if (content_type == CONTENT_SETTINGS_TYPE_GEOLOCATION) {
    SearchGeolocationService* search_helper =
        SearchGeolocationService::Factory::GetForBrowserContext(
            GetActiveUserProfile(false /* is_incognito */));
    if (search_helper) {
      const url::Origin& dse_origin = search_helper->GetDSEOriginIfEnabled();
      if (!dse_origin.unique()) {
        std::string dse_origin_string = dse_origin.Serialize();
        if (!base::ContainsValue(seen_origins, dse_origin_string)) {
          seen_origins.push_back(dse_origin_string);
          insertionFunc(env, list,
                        ConvertOriginToJavaString(env, dse_origin_string),
                        jembedder);
        }
      }
    }
  }
}

ContentSetting GetSettingForOrigin(JNIEnv* env,
                                   ContentSettingsType content_type,
                                   jstring origin,
                                   jstring embedder,
                                   jboolean is_incognito) {
  GURL url(ConvertJavaStringToUTF8(env, origin));
  std::string embedder_str = ConvertJavaStringToUTF8(env, embedder);
  GURL embedder_url;
  // TODO(raymes): This check to see if '*' is the embedder is a hack that fixes
  // crbug.com/738377. In general querying the settings for patterns is broken
  // and needs to be fixed. See crbug.com/738757.
  if (embedder_str == "*")
    embedder_url = url;
  else
    embedder_url = GURL(embedder_str);
  return PermissionManager::Get(GetActiveUserProfile(is_incognito))
      ->GetPermissionStatus(content_type, url, embedder_url)
      .content_setting;
}

void SetSettingForOrigin(JNIEnv* env,
                         ContentSettingsType content_type,
                         jstring origin,
                         jstring embedder,
                         ContentSetting setting,
                         jboolean is_incognito) {
  GURL origin_url(ConvertJavaStringToUTF8(env, origin));
  GURL embedder_url =
      embedder ? GURL(ConvertJavaStringToUTF8(env, embedder)) : GURL();
  Profile* profile = GetActiveUserProfile(is_incognito);

  // The permission may have been blocked due to being under embargo, so if it
  // was changed away from BLOCK, clear embargo status if it exists.
  if (setting != CONTENT_SETTING_BLOCK) {
    PermissionDecisionAutoBlocker::GetForProfile(profile)->RemoveEmbargoByUrl(
        origin_url, content_type);
  }

  PermissionUtil::ScopedRevocationReporter scoped_revocation_reporter(
      profile, origin_url, embedder_url, content_type,
      PermissionSourceUI::SITE_SETTINGS);
  HostContentSettingsMapFactory::GetForProfile(profile)
      ->SetContentSettingDefaultScope(origin_url, embedder_url, content_type,
                                      std::string(), setting);
  WebSiteSettingsUmaUtil::LogPermissionChange(content_type, setting);
}

}  // anonymous namespace

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
  SetSettingForOrigin(env, CONTENT_SETTINGS_TYPE_GEOLOCATION, origin, embedder,
                      static_cast<ContentSetting>(value), is_incognito);
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
  SetSettingForOrigin(env, CONTENT_SETTINGS_TYPE_MIDI_SYSEX, origin, embedder,
                      static_cast<ContentSetting>(value), is_incognito);
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
  SetSettingForOrigin(env, CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER,
                      origin, embedder, static_cast<ContentSetting>(value),
                      is_incognito);
}

static void GetNotificationOrigins(JNIEnv* env,
                                   const JavaParamRef<jclass>& clazz,
                                   const JavaParamRef<jobject>& list) {
  GetOrigins(env, CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
             &Java_WebsitePreferenceBridge_insertNotificationIntoList, list,
             false);
}

static jint GetNotificationSettingForOrigin(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jstring>& origin,
    jboolean is_incognito) {
  return GetSettingForOrigin(env, CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                             origin, origin, is_incognito);
}

static void SetNotificationSettingForOrigin(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jstring>& origin,
    jint value,
    jboolean is_incognito) {
  // Note: Web Notification permission behaves differently from all other
  // permission types. See https://crbug.com/416894.
  Profile* profile = GetActiveUserProfile(is_incognito);
  GURL url = GURL(ConvertJavaStringToUTF8(env, origin));
  ContentSetting setting = static_cast<ContentSetting>(value);

  if (setting != CONTENT_SETTING_BLOCK) {
    PermissionDecisionAutoBlocker::GetForProfile(profile)->RemoveEmbargoByUrl(
        url, CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
  }

  switch (setting) {
    case CONTENT_SETTING_DEFAULT:
      DesktopNotificationProfileUtil::ClearSetting(profile, url);
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
                                          jint value,
                                          jboolean is_incognito) {
  // Here 'nullptr' indicates that microphone uses wildcard for embedder.
  SetSettingForOrigin(env, CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC, origin,
                      nullptr, static_cast<ContentSetting>(value),
                      is_incognito);
}

static void SetCameraSettingForOrigin(JNIEnv* env,
                                      const JavaParamRef<jclass>& clazz,
                                      const JavaParamRef<jstring>& origin,
                                      jint value,
                                      jboolean is_incognito) {
  // Here 'nullptr' indicates that camera uses wildcard for embedder.
  SetSettingForOrigin(env, CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA, origin,
                      nullptr, static_cast<ContentSetting>(value),
                      is_incognito);
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

static void GetUsbOrigins(JNIEnv* env,
                          const JavaParamRef<jclass>& clazz,
                          const JavaParamRef<jobject>& list) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  UsbChooserContext* context = UsbChooserContextFactory::GetForProfile(profile);
  for (const auto& object : context->GetAllGrantedObjects()) {
    // Remove the trailing slash so that origins are matched correctly in
    // SingleWebsitePreferences.mergePermissionInfoForTopLevelOrigin.
    std::string origin = object->requesting_origin.spec();
    DCHECK_EQ('/', origin.back());
    origin.pop_back();
    ScopedJavaLocalRef<jstring> jorigin = ConvertUTF8ToJavaString(env, origin);

    std::string embedder = object->embedding_origin.spec();
    DCHECK_EQ('/', embedder.back());
    embedder.pop_back();
    ScopedJavaLocalRef<jstring> jembedder;
    if (embedder != origin)
      jembedder = ConvertUTF8ToJavaString(env, embedder);

    std::string name;
    bool found = object->object.GetString("name", &name);
    DCHECK(found);
    ScopedJavaLocalRef<jstring> jname = ConvertUTF8ToJavaString(env, name);

    std::string serialized;
    bool written = base::JSONWriter::Write(object->object, &serialized);
    DCHECK(written);
    ScopedJavaLocalRef<jstring> jserialized =
        ConvertUTF8ToJavaString(env, serialized);

    Java_WebsitePreferenceBridge_insertUsbInfoIntoList(
        env, list, jorigin, jembedder, jname, jserialized);
  }
}

static void RevokeUsbPermission(JNIEnv* env,
                                const JavaParamRef<jclass>& clazz,
                                const JavaParamRef<jstring>& jorigin,
                                const JavaParamRef<jstring>& jembedder,
                                const JavaParamRef<jstring>& jobject) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  UsbChooserContext* context = UsbChooserContextFactory::GetForProfile(profile);
  GURL origin(ConvertJavaStringToUTF8(env, jorigin));
  DCHECK(origin.is_valid());
  // If embedder == origin above then a null embedder was sent to Java instead
  // of a duplicated string.
  GURL embedder(
      ConvertJavaStringToUTF8(env, jembedder.is_null() ? jorigin : jembedder));
  DCHECK(embedder.is_valid());
  std::unique_ptr<base::DictionaryValue> object = base::DictionaryValue::From(
      base::JSONReader::Read(ConvertJavaStringToUTF8(env, jobject)));
  DCHECK(object);
  context->RevokeObjectPermission(origin, embedder, *object);
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
        nullptr,
        new BrowsingDataAppCacheHelper(profile_),
        new BrowsingDataIndexedDBHelper(indexed_db_context),
        BrowsingDataFileSystemHelper::Create(file_system_context),
        BrowsingDataQuotaHelper::Create(profile_),
        BrowsingDataChannelIDHelper::Create(profile_->GetRequestContext()),
        new BrowsingDataServiceWorkerHelper(service_worker_context),
        new BrowsingDataCacheStorageHelper(cache_storage_context),
        nullptr,
        nullptr);

    cookies_tree_model_.reset(new CookiesTreeModel(
        container, profile_->GetExtensionSpecialStoragePolicy()));
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
    BrowserThread::ReleaseSoon(BrowserThread::UI, FROM_HERE, this);
  }

  void RecursivelyFindSiteAndDelete(CookieTreeNode* node) {
    CookieTreeNode::DetailedInfo info = node->GetDetailedInfo();
    for (int i = node->child_count(); i > 0; --i)
      RecursivelyFindSiteAndDelete(node->GetChild(i - 1));

    if (info.node_type == CookieTreeNode::DetailedInfo::TYPE_COOKIE &&
        info.cookie && domain_.DomainIs(info.cookie->Domain()))
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

  std::unique_ptr<CookiesTreeModel> cookies_tree_model_;

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

      Java_WebsitePreferenceBridge_insertStorageInfoIntoList(env_, list, host,
                                                             i->type, i->usage);
    }

    base::android::RunCallbackAndroid(java_callback_, list);
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

    Java_StorageInfoClearedCallback_onStorageInfoCleared(env_, java_callback_);

    delete this;
  }

 private:
  JNIEnv* env_;
  ScopedJavaGlobalRef<jobject> java_callback_;
};

class LocalStorageInfoReadyCallback {
 public:
  LocalStorageInfoReadyCallback(const JavaRef<jobject>& java_callback,
                                bool fetch_important)
      : env_(base::android::AttachCurrentThread()),
        java_callback_(java_callback),
        fetch_important_(fetch_important) {}

  void OnLocalStorageModelInfoLoaded(
      Profile* profile,
      const std::list<BrowsingDataLocalStorageHelper::LocalStorageInfo>&
          local_storage_info) {
    ScopedJavaLocalRef<jobject> map =
        Java_WebsitePreferenceBridge_createLocalStorageInfoMap(env_);

    std::vector<ImportantSitesUtil::ImportantDomainInfo> important_domains;
    if (fetch_important_) {
      important_domains = ImportantSitesUtil::GetImportantRegisterableDomains(
          profile, kMaxImportantSites);
    }

    std::list<BrowsingDataLocalStorageHelper::LocalStorageInfo>::const_iterator
        i;
    for (i = local_storage_info.begin(); i != local_storage_info.end(); ++i) {
      ScopedJavaLocalRef<jstring> full_origin =
          ConvertUTF8ToJavaString(env_, i->origin_url.spec());
      std::string origin_str = i->origin_url.GetOrigin().spec();

      bool important = false;
      if (fetch_important_) {
        std::string registerable_domain;
        if (i->origin_url.HostIsIPAddress()) {
          registerable_domain = i->origin_url.host();
        } else {
          registerable_domain =
              net::registry_controlled_domains::GetDomainAndRegistry(
                  i->origin_url,
                  net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
        }
        auto important_domain_search =
            [&registerable_domain](
                const ImportantSitesUtil::ImportantDomainInfo& item) {
              return item.registerable_domain == registerable_domain;
            };
        if (std::find_if(important_domains.begin(), important_domains.end(),
                         important_domain_search) != important_domains.end()) {
          important = true;
        }
      }
      // Remove the trailing slash so the origin is matched correctly in
      // SingleWebsitePreferences.mergePermissionInfoForTopLevelOrigin.
      DCHECK_EQ('/', origin_str.back());
      origin_str.pop_back();
      ScopedJavaLocalRef<jstring> origin =
          ConvertUTF8ToJavaString(env_, origin_str);
      Java_WebsitePreferenceBridge_insertLocalStorageInfoIntoMap(
          env_, map, origin, full_origin, i->size, important);
    }

    base::android::RunCallbackAndroid(java_callback_, map);
    delete this;
  }

 private:
  JNIEnv* env_;
  ScopedJavaGlobalRef<jobject> java_callback_;
  bool fetch_important_;
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
                                  const JavaParamRef<jobject>& java_callback,
                                  jboolean fetch_important) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  scoped_refptr<BrowsingDataLocalStorageHelper> local_storage_helper(
      new BrowsingDataLocalStorageHelper(profile));
  // local_storage_callback will delete itself when it is run.
  LocalStorageInfoReadyCallback* local_storage_callback =
      new LocalStorageInfoReadyCallback(java_callback, fetch_important);
  local_storage_helper->StartFetching(
      base::Bind(&LocalStorageInfoReadyCallback::OnLocalStorageModelInfoLoaded,
                 base::Unretained(local_storage_callback), profile));
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

static void ClearBannerData(JNIEnv* env,
                            const JavaParamRef<jclass>& clazz,
                            const JavaParamRef<jstring>& jorigin) {
  GetHostContentSettingsMap(false)->SetWebsiteSettingDefaultScope(
      GURL(ConvertJavaStringToUTF8(env, jorigin)), GURL(),
      CONTENT_SETTINGS_TYPE_APP_BANNER, std::string(), nullptr);
}

static jboolean ShouldUseDSEGeolocationSetting(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jstring>& jorigin,
    jboolean is_incognito) {
  SearchGeolocationService* search_helper =
      SearchGeolocationService::Factory::GetForBrowserContext(
          GetActiveUserProfile(is_incognito));
  return search_helper &&
         search_helper->UseDSEGeolocationSetting(
             url::Origin(GURL(ConvertJavaStringToUTF8(env, jorigin))));
}

static jboolean GetDSEGeolocationSetting(JNIEnv* env,
                                         const JavaParamRef<jclass>& clazz) {
  SearchGeolocationService* search_helper =
      SearchGeolocationService::Factory::GetForBrowserContext(
          GetActiveUserProfile(false /* is_incognito */));
  return search_helper->GetDSEGeolocationSetting();
}

static void SetDSEGeolocationSetting(JNIEnv* env,
                                     const JavaParamRef<jclass>& clazz,
                                     jboolean setting) {
  SearchGeolocationService* search_helper =
      SearchGeolocationService::Factory::GetForBrowserContext(
          GetActiveUserProfile(false /* is_incognito */));
  return search_helper->SetDSEGeolocationSetting(setting);
}

static jboolean GetAdBlockingActivated(JNIEnv* env,
                                       const JavaParamRef<jclass>& clazz,
                                       const JavaParamRef<jstring>& jorigin) {
  GURL url(ConvertJavaStringToUTF8(env, jorigin));
  return !!GetHostContentSettingsMap(false)->GetWebsiteSetting(
      url, GURL(), CONTENT_SETTINGS_TYPE_ADS_DATA, std::string(), nullptr);
}
