// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/off_the_record_profile_impl.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/background/background_contents_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_plugin_service_filter.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/extensions/extension_info_map.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/extensions/extension_pref_store.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/browser/extensions/extension_webrequest_api.h"
#include "chrome/browser/net/proxy_service_factory.h"
#include "chrome/browser/plugin_prefs.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/transport_security_persister.h"
#include "chrome/browser/ui/find_bar/find_bar_state.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/extension_icon_source.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/json_pref_store.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/chrome_blob_storage_context.h"
#include "content/browser/file_system/browser_file_system_helper.h"
#include "content/browser/host_zoom_map.h"
#include "content/browser/in_process_webkit/webkit_context.h"
#include "content/browser/ssl/ssl_host_state.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "net/base/transport_security_state.h"
#include "net/http/http_server_properties.h"
#include "webkit/database/database_tracker.h"
#include "webkit/quota/quota_manager.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/preferences.h"
#include "chrome/browser/chromeos/proxy_config_service_impl.h"
#endif

using content::BrowserThread;

namespace {

void NotifyOTRProfileCreatedOnIOThread(void* original_profile,
                                       void* otr_profile) {
  ExtensionWebRequestEventRouter::GetInstance()->OnOTRProfileCreated(
      original_profile, otr_profile);
}

void NotifyOTRProfileDestroyedOnIOThread(void* original_profile,
                                         void* otr_profile) {
  ExtensionWebRequestEventRouter::GetInstance()->OnOTRProfileDestroyed(
      original_profile, otr_profile);
}

}  // namespace

OffTheRecordProfileImpl::OffTheRecordProfileImpl(Profile* real_profile)
    : profile_(real_profile),
      prefs_(real_profile->GetOffTheRecordPrefs()),
      ALLOW_THIS_IN_INITIALIZER_LIST(io_data_(this)),
      start_time_(Time::Now()) {
}

void OffTheRecordProfileImpl::Init() {
  extension_process_manager_.reset(ExtensionProcessManager::Create(this));

  BrowserList::AddObserver(this);

  ProfileDependencyManager::GetInstance()->CreateProfileServices(this, false);

  DCHECK_NE(IncognitoModePrefs::DISABLED,
            IncognitoModePrefs::GetAvailability(profile_->GetPrefs()));

  // TODO(oshima): Remove the need to eagerly initialize the request context
  // getter. chromeos::OnlineAttempt is illegally trying to access this
  // Profile member from a thread other than the UI thread, so we need to
  // prevent a race.
#if defined(OS_CHROMEOS)
  GetRequestContext();
#endif  // defined(OS_CHROMEOS)

  // Make the chrome//extension-icon/ resource available.
  ExtensionIconSource* icon_source = new ExtensionIconSource(profile_);
  GetChromeURLDataManager()->AddDataSource(icon_source);

  ChromePluginServiceFilter::GetInstance()->RegisterResourceContext(
      PluginPrefs::GetForProfile(this), &GetResourceContext());

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&NotifyOTRProfileCreatedOnIOThread, profile_, this));
}

OffTheRecordProfileImpl::~OffTheRecordProfileImpl() {
  content::NotificationService::current()->Notify(
    chrome::NOTIFICATION_PROFILE_DESTROYED, content::Source<Profile>(this),
    content::NotificationService::NoDetails());

  ChromePluginServiceFilter::GetInstance()->UnregisterResourceContext(
    &GetResourceContext());

  ProfileDependencyManager::GetInstance()->DestroyProfileServices(this);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&NotifyOTRProfileDestroyedOnIOThread, profile_, this));

  // Clean up all DB files/directories
  if (db_tracker_) {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&webkit_database::DatabaseTracker::Shutdown,
                   db_tracker_.get()));
  }

  BrowserList::RemoveObserver(this);

  if (host_content_settings_map_)
    host_content_settings_map_->ShutdownOnUIThread();

  if (pref_proxy_config_tracker_.get())
    pref_proxy_config_tracker_->DetachFromPrefService();

  ExtensionService* extension_service = GetExtensionService();
  if (extension_service) {
    ExtensionPrefs* extension_prefs = extension_service->extension_prefs();
    extension_prefs->ClearIncognitoSessionOnlyContentSettings();
  }
}

std::string OffTheRecordProfileImpl::GetProfileName() {
  // Incognito profile should not return the profile name.
  return std::string();
}

FilePath OffTheRecordProfileImpl::GetPath() {
  return profile_->GetPath();
}

bool OffTheRecordProfileImpl::IsOffTheRecord() {
  return true;
}

Profile* OffTheRecordProfileImpl::GetOffTheRecordProfile() {
  return this;
}

void OffTheRecordProfileImpl::DestroyOffTheRecordProfile() {
  // Suicide is bad!
  NOTREACHED();
}

bool OffTheRecordProfileImpl::HasOffTheRecordProfile() {
  return true;
}

Profile* OffTheRecordProfileImpl::GetOriginalProfile() {
  return profile_;
}

ChromeAppCacheService* OffTheRecordProfileImpl::GetAppCacheService() {
  CreateQuotaManagerAndClients();
  return appcache_service_;
}

webkit_database::DatabaseTracker*
    OffTheRecordProfileImpl::GetDatabaseTracker() {
  CreateQuotaManagerAndClients();
  return db_tracker_;
}

VisitedLinkMaster* OffTheRecordProfileImpl::GetVisitedLinkMaster() {
  // We don't provide access to the VisitedLinkMaster when we're OffTheRecord
  // because we don't want to leak the sites that the user has visited before.
  return NULL;
}

ExtensionService* OffTheRecordProfileImpl::GetExtensionService() {
  return GetOriginalProfile()->GetExtensionService();
}

UserScriptMaster* OffTheRecordProfileImpl::GetUserScriptMaster() {
  return GetOriginalProfile()->GetUserScriptMaster();
}

ExtensionDevToolsManager*
    OffTheRecordProfileImpl::GetExtensionDevToolsManager() {
  // TODO(mpcomplete): figure out whether we should return the original
  // profile's version.
  return NULL;
}

ExtensionProcessManager*
    OffTheRecordProfileImpl::GetExtensionProcessManager() {
  return extension_process_manager_.get();
}

ExtensionMessageService*
    OffTheRecordProfileImpl::GetExtensionMessageService() {
  return GetOriginalProfile()->GetExtensionMessageService();
}

ExtensionEventRouter* OffTheRecordProfileImpl::GetExtensionEventRouter() {
  return GetOriginalProfile()->GetExtensionEventRouter();
}

ExtensionSpecialStoragePolicy*
    OffTheRecordProfileImpl::GetExtensionSpecialStoragePolicy() {
  return GetOriginalProfile()->GetExtensionSpecialStoragePolicy();
}

SSLHostState* OffTheRecordProfileImpl::GetSSLHostState() {
  if (!ssl_host_state_.get())
    ssl_host_state_.reset(new SSLHostState());

  DCHECK(ssl_host_state_->CalledOnValidThread());
  return ssl_host_state_.get();
}

GAIAInfoUpdateService* OffTheRecordProfileImpl::GetGAIAInfoUpdateService() {
  return NULL;
}

HistoryService* OffTheRecordProfileImpl::GetHistoryService(
    ServiceAccessType sat) {
  if (sat == EXPLICIT_ACCESS)
    return profile_->GetHistoryService(sat);

  NOTREACHED() << "This profile is OffTheRecord";
  return NULL;
}

HistoryService* OffTheRecordProfileImpl::GetHistoryServiceWithoutCreating() {
  return profile_->GetHistoryServiceWithoutCreating();
}

FaviconService* OffTheRecordProfileImpl::GetFaviconService(
    ServiceAccessType sat) {
  if (sat == EXPLICIT_ACCESS)
    return profile_->GetFaviconService(sat);

  NOTREACHED() << "This profile is OffTheRecord";
  return NULL;
}

AutocompleteClassifier* OffTheRecordProfileImpl::GetAutocompleteClassifier() {
  return profile_->GetAutocompleteClassifier();
}

history::ShortcutsBackend* OffTheRecordProfileImpl::GetShortcutsBackend() {
  return NULL;
}

WebDataService* OffTheRecordProfileImpl::GetWebDataService(
    ServiceAccessType sat) {
  if (sat == EXPLICIT_ACCESS)
    return profile_->GetWebDataService(sat);

  NOTREACHED() << "This profile is OffTheRecord";
  return NULL;
}

WebDataService* OffTheRecordProfileImpl::GetWebDataServiceWithoutCreating() {
  return profile_->GetWebDataServiceWithoutCreating();
}

PasswordStore* OffTheRecordProfileImpl::GetPasswordStore(
    ServiceAccessType sat) {
  if (sat == EXPLICIT_ACCESS)
    return profile_->GetPasswordStore(sat);

  NOTREACHED() << "This profile is OffTheRecord";
  return NULL;
}

PrefService* OffTheRecordProfileImpl::GetPrefs() {
  return prefs_;
}

PrefService* OffTheRecordProfileImpl::GetOffTheRecordPrefs() {
  return prefs_;
}

TemplateURLFetcher* OffTheRecordProfileImpl::GetTemplateURLFetcher() {
  return profile_->GetTemplateURLFetcher();
}

DownloadManager* OffTheRecordProfileImpl::GetDownloadManager() {
  return DownloadServiceFactory::GetForProfile(this)->GetDownloadManager();
}

fileapi::FileSystemContext* OffTheRecordProfileImpl::GetFileSystemContext() {
  CreateQuotaManagerAndClients();
  return file_system_context_.get();
}

net::URLRequestContextGetter* OffTheRecordProfileImpl::GetRequestContext() {
  return io_data_.GetMainRequestContextGetter();
}

quota::QuotaManager* OffTheRecordProfileImpl::GetQuotaManager() {
  CreateQuotaManagerAndClients();
  return quota_manager_.get();
}

net::URLRequestContextGetter*
    OffTheRecordProfileImpl::GetRequestContextForRenderProcess(
        int renderer_child_id) {
  if (GetExtensionService()) {
    const Extension* installed_app = GetExtensionService()->
        GetInstalledAppForRenderer(renderer_child_id);
    if (installed_app != NULL && installed_app->is_storage_isolated() &&
        installed_app->HasAPIPermission(
            ExtensionAPIPermission::kExperimental)) {
      return GetRequestContextForIsolatedApp(installed_app->id());
    }
  }
  return GetRequestContext();
}

net::URLRequestContextGetter*
    OffTheRecordProfileImpl::GetRequestContextForMedia() {
  // In OTR mode, media request context is the same as the original one.
  return io_data_.GetMainRequestContextGetter();
}

net::URLRequestContextGetter*
    OffTheRecordProfileImpl::GetRequestContextForExtensions() {
  return io_data_.GetExtensionsRequestContextGetter();
}

net::URLRequestContextGetter*
    OffTheRecordProfileImpl::GetRequestContextForIsolatedApp(
        const std::string& app_id) {
  return io_data_.GetIsolatedAppRequestContextGetter(app_id);
}

const content::ResourceContext&
    OffTheRecordProfileImpl::GetResourceContext() {
  return io_data_.GetResourceContext();
}

net::SSLConfigService* OffTheRecordProfileImpl::GetSSLConfigService() {
  return profile_->GetSSLConfigService();
}

HostContentSettingsMap* OffTheRecordProfileImpl::GetHostContentSettingsMap() {
  // Retrieve the host content settings map of the parent profile in order to
  // ensure the preferences have been migrated.
  profile_->GetHostContentSettingsMap();
  if (!host_content_settings_map_.get()) {
    host_content_settings_map_ = new HostContentSettingsMap(
        GetPrefs(), GetExtensionService(), true);
  }
  return host_content_settings_map_.get();
}

HostZoomMap* OffTheRecordProfileImpl::GetHostZoomMap() {
  // Create new host zoom map and copy zoom levels from parent.
  if (!host_zoom_map_) {
    host_zoom_map_ = new HostZoomMap(profile_->GetHostZoomMap());
     // Observe parent's HZM change for propagating change of parent's
     // change to this HZM.
     registrar_.Add(this, content::NOTIFICATION_ZOOM_LEVEL_CHANGED,
                    content::Source<HostZoomMap>(profile_->GetHostZoomMap()));
  }
  return host_zoom_map_.get();
}

GeolocationPermissionContext*
    OffTheRecordProfileImpl::GetGeolocationPermissionContext() {
  return profile_->GetGeolocationPermissionContext();
}

SpeechInputPreferences* OffTheRecordProfileImpl::GetSpeechInputPreferences() {
  return profile_->GetSpeechInputPreferences();
}

UserStyleSheetWatcher* OffTheRecordProfileImpl::GetUserStyleSheetWatcher() {
  return profile_->GetUserStyleSheetWatcher();
}

FindBarState* OffTheRecordProfileImpl::GetFindBarState() {
  if (!find_bar_state_.get())
    find_bar_state_.reset(new FindBarState());
  return find_bar_state_.get();
}

bool OffTheRecordProfileImpl::HasProfileSyncService() const {
  // We never have a profile sync service.
  return false;
}

bool OffTheRecordProfileImpl::DidLastSessionExitCleanly() {
  return profile_->DidLastSessionExitCleanly();
}

BookmarkModel* OffTheRecordProfileImpl::GetBookmarkModel() {
  return profile_->GetBookmarkModel();
}

ProtocolHandlerRegistry* OffTheRecordProfileImpl::GetProtocolHandlerRegistry() {
  return profile_->GetProtocolHandlerRegistry();
}

TokenService* OffTheRecordProfileImpl::GetTokenService() {
  return NULL;
}

ProfileSyncService* OffTheRecordProfileImpl::GetProfileSyncService() {
  return NULL;
}

ProfileSyncService* OffTheRecordProfileImpl::GetProfileSyncService(
    const std::string& cros_user) {
  return NULL;
}

bool OffTheRecordProfileImpl::IsSameProfile(Profile* profile) {
  return (profile == this) || (profile == profile_);
}

Time OffTheRecordProfileImpl::GetStartTime() const {
  return start_time_;
}

SpellCheckHost* OffTheRecordProfileImpl::GetSpellCheckHost() {
  return profile_->GetSpellCheckHost();
}

void OffTheRecordProfileImpl::ReinitializeSpellCheckHost(bool force) {
  profile_->ReinitializeSpellCheckHost(force);
}

WebKitContext* OffTheRecordProfileImpl::GetWebKitContext() {
  CreateQuotaManagerAndClients();
  return webkit_context_.get();
}

history::TopSites* OffTheRecordProfileImpl::GetTopSitesWithoutCreating() {
  return NULL;
}

history::TopSites* OffTheRecordProfileImpl::GetTopSites() {
  return NULL;
}

void OffTheRecordProfileImpl::MarkAsCleanShutdown() {
}

void OffTheRecordProfileImpl::InitExtensions(bool extensions_enabled) {
  NOTREACHED();
}

void OffTheRecordProfileImpl::InitPromoResources() {
  NOTREACHED();
}

void OffTheRecordProfileImpl::InitRegisteredProtocolHandlers() {
  NOTREACHED();
}

FilePath OffTheRecordProfileImpl::last_selected_directory() {
  const FilePath& directory = last_selected_directory_;
  if (directory.empty()) {
    return profile_->last_selected_directory();
  }
  return directory;
}

void OffTheRecordProfileImpl::set_last_selected_directory(
    const FilePath& path) {
  last_selected_directory_ = path;
}

#if defined(OS_CHROMEOS)
void OffTheRecordProfileImpl::SetupChromeOSEnterpriseExtensionObserver() {
  profile_->SetupChromeOSEnterpriseExtensionObserver();
}

void OffTheRecordProfileImpl::InitChromeOSPreferences() {
  // The incognito profile shouldn't have Chrome OS's preferences.
  // The preferences are associated with the regular user profile.
}
#endif  // defined(OS_CHROMEOS)

void OffTheRecordProfileImpl::OnBrowserAdded(const Browser* browser) {
}

void OffTheRecordProfileImpl::OnBrowserRemoved(const Browser* browser) {
}

ChromeBlobStorageContext* OffTheRecordProfileImpl::GetBlobStorageContext() {
  if (!blob_storage_context_) {
    blob_storage_context_ = new ChromeBlobStorageContext();
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ChromeBlobStorageContext::InitializeOnIOThread,
                   blob_storage_context_.get()));
  }
  return blob_storage_context_;
}

ExtensionInfoMap* OffTheRecordProfileImpl::GetExtensionInfoMap() {
  return profile_->GetExtensionInfoMap();
}

ChromeURLDataManager* OffTheRecordProfileImpl::GetChromeURLDataManager() {
  if (!chrome_url_data_manager_.get())
    chrome_url_data_manager_.reset(new ChromeURLDataManager(
        io_data_.GetChromeURLDataManagerBackendGetter()));
  return chrome_url_data_manager_.get();
}

PromoCounter* OffTheRecordProfileImpl::GetInstantPromoCounter() {
  return NULL;
}

#if defined(OS_CHROMEOS)
void OffTheRecordProfileImpl::ChangeAppLocale(const std::string& locale,
                                              AppLocaleChangedVia) {
}

void OffTheRecordProfileImpl::OnLogin() {
}
#endif  // defined(OS_CHROMEOS)

PrefProxyConfigTracker* OffTheRecordProfileImpl::GetProxyConfigTracker() {
  if (!pref_proxy_config_tracker_.get()) {
    pref_proxy_config_tracker_.reset(
        ProxyServiceFactory::CreatePrefProxyConfigTracker(GetPrefs()));
  }
  return pref_proxy_config_tracker_.get();
}

chrome_browser_net::Predictor* OffTheRecordProfileImpl::GetNetworkPredictor() {
  // We do not store information about websites visited in OTR profiles which
  // is necessary for a Predictor, so we do not have a Predictor at all.
  return NULL;
}

void OffTheRecordProfileImpl::ClearNetworkingHistorySince(base::Time time) {
  // No need to do anything here, our transport security state is read-only.
}

GURL OffTheRecordProfileImpl::GetHomePage() {
  return profile_->GetHomePage();
}

NetworkActionPredictor* OffTheRecordProfileImpl::GetNetworkActionPredictor() {
  return NULL;
}

void OffTheRecordProfileImpl::Observe(int type,
                     const content::NotificationSource& source,
                     const content::NotificationDetails& details) {
  if (type == content::NOTIFICATION_ZOOM_LEVEL_CHANGED) {
    const std::string& host =
        *(content::Details<const std::string>(details).ptr());
    if (!host.empty()) {
      double level = profile_->GetHostZoomMap()->GetZoomLevel(host);
      GetHostZoomMap()->SetZoomLevel(host, level);
    }
  }
}

void OffTheRecordProfileImpl::CreateQuotaManagerAndClients() {
  if (quota_manager_.get()) {
    DCHECK(file_system_context_.get());
    DCHECK(db_tracker_.get());
    DCHECK(webkit_context_.get());
    return;
  }

  // All of the clients have to be created and registered with the
  // QuotaManager prior to the QuotaManger being used. So we do them
  // all together here prior to handing out a reference to anything
  // that utlizes the QuotaManager.
  quota_manager_ = new quota::QuotaManager(
      IsOffTheRecord(),
      GetPath(),
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO),
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::DB),
      GetExtensionSpecialStoragePolicy());

  // Each consumer is responsible for registering its QuotaClient during
  // its construction.
  file_system_context_ = CreateFileSystemContext(
      GetPath(), IsOffTheRecord(),
      GetExtensionSpecialStoragePolicy(),
      quota_manager_->proxy());
  db_tracker_ = new webkit_database::DatabaseTracker(
      GetPath(), IsOffTheRecord(), false, GetExtensionSpecialStoragePolicy(),
      quota_manager_->proxy(),
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE));
  webkit_context_ = new WebKitContext(
      IsOffTheRecord(), GetPath(), GetExtensionSpecialStoragePolicy(),
      false, quota_manager_->proxy(),
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::WEBKIT));
  appcache_service_ = new ChromeAppCacheService(quota_manager_->proxy());
  BrowserThread::PostTask(
    BrowserThread::IO, FROM_HERE,
    base::Bind(&ChromeAppCacheService::InitializeOnIOThread,
               appcache_service_.get(),
               IsOffTheRecord()
                   ? FilePath() : GetPath().Append(chrome::kAppCacheDirname),
               &GetResourceContext(),
               make_scoped_refptr(GetExtensionSpecialStoragePolicy())));
}

#if defined(OS_CHROMEOS)
// Special case of the OffTheRecordProfileImpl which is used while Guest
// session in CrOS.
class GuestSessionProfile : public OffTheRecordProfileImpl {
 public:
  explicit GuestSessionProfile(Profile* real_profile)
      : OffTheRecordProfileImpl(real_profile) {
  }

  virtual void InitChromeOSPreferences() {
    chromeos_preferences_.reset(new chromeos::Preferences());
    chromeos_preferences_->Init(GetPrefs());
  }

 private:
  // The guest user should be able to customize Chrome OS preferences.
  scoped_ptr<chromeos::Preferences> chromeos_preferences_;
};
#endif

Profile* Profile::CreateOffTheRecordProfile() {
  OffTheRecordProfileImpl* profile = NULL;
#if defined(OS_CHROMEOS)
  if (Profile::IsGuestSession())
    profile = new GuestSessionProfile(this);
#endif
  if (!profile)
    profile = new OffTheRecordProfileImpl(this);
  profile->Init();
  return profile;
}
