// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"

#include <string>

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
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/extensions/extension_info_map.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/extensions/extension_pref_store.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/browser/extensions/extension_webrequest_api.h"
#include "chrome/browser/net/pref_proxy_config_service.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/off_the_record_profile_io_data.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/transport_security_persister.h"
#include "chrome/browser/ui/browser_list.h"
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
#include "content/browser/browser_thread.h"
#include "content/browser/chrome_blob_storage_context.h"
#include "content/browser/download/download_manager.h"
#include "content/browser/file_system/browser_file_system_helper.h"
#include "content/browser/host_zoom_map.h"
#include "content/browser/in_process_webkit/webkit_context.h"
#include "content/browser/ssl/ssl_host_state.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_service.h"
#include "net/base/transport_security_state.h"
#include "webkit/database/database_tracker.h"
#include "webkit/quota/quota_manager.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/preferences.h"
#endif

using base::Time;
using base::TimeDelta;

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

////////////////////////////////////////////////////////////////////////////////
//
// OffTheRecordProfileImpl is a profile subclass that wraps an existing profile
// to make it suitable for the incognito mode.
//
////////////////////////////////////////////////////////////////////////////////
class OffTheRecordProfileImpl : public Profile,
                                public BrowserList::Observer {
 public:
  explicit OffTheRecordProfileImpl(Profile* real_profile)
      : profile_(real_profile),
        prefs_(real_profile->GetOffTheRecordPrefs()),
        ALLOW_THIS_IN_INITIALIZER_LIST(io_data_(this)),
        start_time_(Time::Now()) {
    extension_process_manager_.reset(ExtensionProcessManager::Create(this));

    BrowserList::AddObserver(this);

    ProfileDependencyManager::GetInstance()->CreateProfileServices(this, false);

    DCHECK_NE(IncognitoModePrefs::DISABLED,
              IncognitoModePrefs::GetAvailability(real_profile->GetPrefs()));

    // TODO(oshima): Remove the need to eagerly initialize the request context
    // getter. chromeos::OnlineAttempt is illegally trying to access this
    // Profile member from a thread other than the UI thread, so we need to
    // prevent a race.
#if defined(OS_CHROMEOS)
    GetRequestContext();
#endif  // defined(OS_CHROMEOS)

    // Make the chrome//extension-icon/ resource available.
    ExtensionIconSource* icon_source = new ExtensionIconSource(real_profile);
    GetChromeURLDataManager()->AddDataSource(icon_source);

    BrowserThread::PostTask(
    BrowserThread::IO, FROM_HERE,
    NewRunnableFunction(&NotifyOTRProfileCreatedOnIOThread, profile_, this));
  }

  virtual ~OffTheRecordProfileImpl() {
    NotificationService::current()->Notify(
        chrome::NOTIFICATION_PROFILE_DESTROYED, Source<Profile>(this),
        NotificationService::NoDetails());

    ProfileDependencyManager::GetInstance()->DestroyProfileServices(this);

    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        NewRunnableFunction(
            &NotifyOTRProfileDestroyedOnIOThread, profile_, this));

    // Clean up all DB files/directories
    if (db_tracker_) {
      BrowserThread::PostTask(
          BrowserThread::FILE, FROM_HERE,
          NewRunnableMethod(
              db_tracker_.get(),
              &webkit_database::DatabaseTracker::Shutdown));
    }

    BrowserList::RemoveObserver(this);

    if (host_content_settings_map_)
      host_content_settings_map_->ShutdownOnUIThread();

    if (pref_proxy_config_tracker_)
      pref_proxy_config_tracker_->DetachFromPrefService();

    ExtensionService* extension_service = GetExtensionService();
    if (extension_service) {
      ExtensionPrefs* extension_prefs = extension_service->extension_prefs();
      extension_prefs->ClearIncognitoSessionOnlyContentSettings();
    }
  }

  virtual std::string GetProfileName() {
    // Incognito profile should not return the profile name.
    return std::string();
  }

  virtual FilePath GetPath() { return profile_->GetPath(); }

  virtual bool IsOffTheRecord() {
    return true;
  }

  virtual Profile* GetOffTheRecordProfile() {
    return this;
  }

  virtual void DestroyOffTheRecordProfile() {
    // Suicide is bad!
    NOTREACHED();
  }

  virtual bool HasOffTheRecordProfile() {
    return true;
  }

  virtual Profile* GetOriginalProfile() {
    return profile_;
  }

  virtual ChromeAppCacheService* GetAppCacheService() {
    CreateQuotaManagerAndClients();
    return appcache_service_;
  }

  virtual webkit_database::DatabaseTracker* GetDatabaseTracker() {
    CreateQuotaManagerAndClients();
    return db_tracker_;
  }

  virtual VisitedLinkMaster* GetVisitedLinkMaster() {
    // We don't provide access to the VisitedLinkMaster when we're OffTheRecord
    // because we don't want to leak the sites that the user has visited before.
    return NULL;
  }

  virtual ExtensionService* GetExtensionService() {
    return GetOriginalProfile()->GetExtensionService();
  }

  virtual UserScriptMaster* GetUserScriptMaster() {
    return GetOriginalProfile()->GetUserScriptMaster();
  }

  virtual ExtensionDevToolsManager* GetExtensionDevToolsManager() {
    // TODO(mpcomplete): figure out whether we should return the original
    // profile's version.
    return NULL;
  }

  virtual ExtensionProcessManager* GetExtensionProcessManager() {
    return extension_process_manager_.get();
  }

  virtual ExtensionMessageService* GetExtensionMessageService() {
    return GetOriginalProfile()->GetExtensionMessageService();
  }

  virtual ExtensionEventRouter* GetExtensionEventRouter() {
    return GetOriginalProfile()->GetExtensionEventRouter();
  }

  virtual ExtensionSpecialStoragePolicy* GetExtensionSpecialStoragePolicy() {
    return GetOriginalProfile()->GetExtensionSpecialStoragePolicy();
  }

  virtual SSLHostState* GetSSLHostState() {
    if (!ssl_host_state_.get())
      ssl_host_state_.reset(new SSLHostState());

    DCHECK(ssl_host_state_->CalledOnValidThread());
    return ssl_host_state_.get();
  }

  virtual net::TransportSecurityState* GetTransportSecurityState() {
    if (!transport_security_state_.get()) {
      transport_security_state_ = new net::TransportSecurityState(
          CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
              switches::kHstsHosts));
      transport_security_loader_ =
          new TransportSecurityPersister(true /* readonly */);
      transport_security_loader_->Initialize(transport_security_state_.get(),
                                             GetOriginalProfile()->GetPath());
    }

    return transport_security_state_.get();
  }

  virtual HistoryService* GetHistoryService(ServiceAccessType sat) {
    if (sat == EXPLICIT_ACCESS)
      return profile_->GetHistoryService(sat);

    NOTREACHED() << "This profile is OffTheRecord";
    return NULL;
  }

  virtual HistoryService* GetHistoryServiceWithoutCreating() {
    return profile_->GetHistoryServiceWithoutCreating();
  }

  virtual FaviconService* GetFaviconService(ServiceAccessType sat) {
    if (sat == EXPLICIT_ACCESS)
      return profile_->GetFaviconService(sat);

    NOTREACHED() << "This profile is OffTheRecord";
    return NULL;
  }

  virtual AutocompleteClassifier* GetAutocompleteClassifier() {
    return profile_->GetAutocompleteClassifier();
  }

  virtual history::ShortcutsBackend* GetShortcutsBackend() {
    return NULL;
  }

  virtual WebDataService* GetWebDataService(ServiceAccessType sat) {
    if (sat == EXPLICIT_ACCESS)
      return profile_->GetWebDataService(sat);

    NOTREACHED() << "This profile is OffTheRecord";
    return NULL;
  }

  virtual WebDataService* GetWebDataServiceWithoutCreating() {
    return profile_->GetWebDataServiceWithoutCreating();
  }

  virtual PasswordStore* GetPasswordStore(ServiceAccessType sat) {
    if (sat == EXPLICIT_ACCESS)
      return profile_->GetPasswordStore(sat);

    NOTREACHED() << "This profile is OffTheRecord";
    return NULL;
  }

  virtual PrefService* GetPrefs() {
    return prefs_;
  }

  virtual PrefService* GetOffTheRecordPrefs() {
    return prefs_;
  }

  virtual TemplateURLFetcher* GetTemplateURLFetcher() {
    return profile_->GetTemplateURLFetcher();
  }

  virtual DownloadManager* GetDownloadManager() {
    if (!download_manager_.get()) {
      download_manager_delegate_ = new ChromeDownloadManagerDelegate(this);
      scoped_refptr<DownloadManager> dlm(
          new DownloadManager(download_manager_delegate_,
                              g_browser_process->download_status_updater()));
      dlm->Init(this);
      download_manager_delegate_->SetDownloadManager(dlm);
      download_manager_.swap(dlm);
    }
    return download_manager_.get();
  }

  virtual bool HasCreatedDownloadManager() const {
    return (download_manager_.get() != NULL);
  }

  virtual PersonalDataManager* GetPersonalDataManager() {
    return NULL;
  }

  virtual fileapi::FileSystemContext* GetFileSystemContext() {
    CreateQuotaManagerAndClients();
    return file_system_context_.get();
  }

  virtual net::URLRequestContextGetter* GetRequestContext() {
    return io_data_.GetMainRequestContextGetter();
  }

  virtual quota::QuotaManager* GetQuotaManager() {
    CreateQuotaManagerAndClients();
    return quota_manager_.get();
  }

  virtual net::URLRequestContextGetter* GetRequestContextForRenderProcess(
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

  virtual net::URLRequestContextGetter* GetRequestContextForMedia() {
    // In OTR mode, media request context is the same as the original one.
    return io_data_.GetMainRequestContextGetter();
  }

  virtual net::URLRequestContextGetter* GetRequestContextForExtensions() {
    return io_data_.GetExtensionsRequestContextGetter();
  }

  virtual net::URLRequestContextGetter* GetRequestContextForIsolatedApp(
      const std::string& app_id) {
    return io_data_.GetIsolatedAppRequestContextGetter(app_id);
  }

  virtual const content::ResourceContext& GetResourceContext() {
    return io_data_.GetResourceContext();
  }

  virtual net::SSLConfigService* GetSSLConfigService() {
    return profile_->GetSSLConfigService();
  }

  virtual HostContentSettingsMap* GetHostContentSettingsMap() {
    // Retrieve the host content settings map of the parent profile in order to
    // ensure the preferences have been migrated.
    profile_->GetHostContentSettingsMap();
    if (!host_content_settings_map_.get()) {
      host_content_settings_map_ = new HostContentSettingsMap(
          GetPrefs(), GetExtensionService(), true);
    }
    return host_content_settings_map_.get();
  }

  virtual HostZoomMap* GetHostZoomMap() {
    if (!host_zoom_map_)
      host_zoom_map_ = new HostZoomMap();
    return host_zoom_map_.get();
  }

  virtual GeolocationPermissionContext* GetGeolocationPermissionContext() {
    return profile_->GetGeolocationPermissionContext();
  }

  virtual UserStyleSheetWatcher* GetUserStyleSheetWatcher() {
    return profile_->GetUserStyleSheetWatcher();
  }

  virtual FindBarState* GetFindBarState() {
    if (!find_bar_state_.get())
      find_bar_state_.reset(new FindBarState());
    return find_bar_state_.get();
  }

  virtual bool HasProfileSyncService() const {
    // We never have a profile sync service.
    return false;
  }

  virtual bool DidLastSessionExitCleanly() {
    return profile_->DidLastSessionExitCleanly();
  }

  virtual BookmarkModel* GetBookmarkModel() {
    return profile_->GetBookmarkModel();
  }

  virtual ProtocolHandlerRegistry* GetProtocolHandlerRegistry() {
    return profile_->GetProtocolHandlerRegistry();
  }

  virtual TokenService* GetTokenService() {
    return NULL;
  }

  virtual ProfileSyncService* GetProfileSyncService() {
    return NULL;
  }

  virtual ProfileSyncService* GetProfileSyncService(
      const std::string& cros_user) {
    return NULL;
  }

  virtual bool IsSameProfile(Profile* profile) {
    return (profile == this) || (profile == profile_);
  }

  virtual Time GetStartTime() const {
    return start_time_;
  }

  virtual SpellCheckHost* GetSpellCheckHost() {
    return profile_->GetSpellCheckHost();
  }

  virtual void ReinitializeSpellCheckHost(bool force) {
    profile_->ReinitializeSpellCheckHost(force);
  }

  virtual WebKitContext* GetWebKitContext() {
    CreateQuotaManagerAndClients();
    return webkit_context_.get();
  }

  virtual history::TopSites* GetTopSitesWithoutCreating() {
    return NULL;
  }

  virtual history::TopSites* GetTopSites() {
    return NULL;
  }

  virtual void MarkAsCleanShutdown() {
  }

  virtual void InitExtensions(bool extensions_enabled) {
    NOTREACHED();
  }

  virtual void InitPromoResources() {
    NOTREACHED();
  }

  virtual void InitRegisteredProtocolHandlers() {
    NOTREACHED();
  }

  virtual FilePath last_selected_directory() {
    const FilePath& directory = last_selected_directory_;
    if (directory.empty()) {
      return profile_->last_selected_directory();
    }
    return directory;
  }

  virtual void set_last_selected_directory(const FilePath& path) {
    last_selected_directory_ = path;
  }

#if defined(OS_CHROMEOS)
  virtual void SetupChromeOSEnterpriseExtensionObserver() {
    profile_->SetupChromeOSEnterpriseExtensionObserver();
  }

  virtual void InitChromeOSPreferences() {
    // The incognito profile shouldn't have Chrome OS's preferences.
    // The preferences are associated with the regular user profile.
  }
#endif  // defined(OS_CHROMEOS)

  virtual void ExitedOffTheRecordMode() {
    // DownloadManager is lazily created, so check before accessing it.
    if (download_manager_.get()) {
      // Drop our download manager so we forget about all the downloads made
      // in incognito mode.
      download_manager_->Shutdown();
      download_manager_ = NULL;
    }
  }

  virtual void OnBrowserAdded(const Browser* browser) {
  }

  virtual void OnBrowserRemoved(const Browser* browser) {
    if (BrowserList::GetBrowserCount(this) == 0)
      ExitedOffTheRecordMode();
  }

  virtual ChromeBlobStorageContext* GetBlobStorageContext() {
    if (!blob_storage_context_) {
      blob_storage_context_ = new ChromeBlobStorageContext();
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          NewRunnableMethod(
              blob_storage_context_.get(),
              &ChromeBlobStorageContext::InitializeOnIOThread));
    }
    return blob_storage_context_;
  }

  virtual ExtensionInfoMap* GetExtensionInfoMap() {
    return profile_->GetExtensionInfoMap();
  }

  virtual ChromeURLDataManager* GetChromeURLDataManager() {
    if (!chrome_url_data_manager_.get())
      chrome_url_data_manager_.reset(new ChromeURLDataManager(
          io_data_.GetChromeURLDataManagerBackendGetter()));
    return chrome_url_data_manager_.get();
  }

  virtual PromoCounter* GetInstantPromoCounter() {
    return NULL;
  }

#if defined(OS_CHROMEOS)
  virtual void ChangeAppLocale(const std::string& locale, AppLocaleChangedVia) {
  }
  virtual void OnLogin() {
  }
#endif  // defined(OS_CHROMEOS)

  virtual PrefProxyConfigTracker* GetProxyConfigTracker() {
    if (!pref_proxy_config_tracker_)
      pref_proxy_config_tracker_ = new PrefProxyConfigTracker(GetPrefs());

    return pref_proxy_config_tracker_;
  }

  virtual prerender::PrerenderManager* GetPrerenderManager() {
    // We do not allow prerendering in OTR profiles at this point.
    // TODO(tburkard): Figure out if we want to support this, and how, at some
    // point in the future.
    return NULL;
  }

 private:
  void CreateQuotaManagerAndClients() {
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
        NewRunnableMethod(
            appcache_service_.get(),
            &ChromeAppCacheService::InitializeOnIOThread,
            IsOffTheRecord()
                ? FilePath() : GetPath().Append(chrome::kAppCacheDirname),
            &GetResourceContext(),
            make_scoped_refptr(GetExtensionSpecialStoragePolicy())));
  }

  NotificationRegistrar registrar_;

  // The real underlying profile.
  Profile* profile_;

  // Weak pointer owned by |profile_|.
  PrefService* prefs_;

  scoped_ptr<ExtensionProcessManager> extension_process_manager_;

  OffTheRecordProfileIOData::Handle io_data_;

  // Used so that Chrome code can influence how content module's DownloadManager
  // functions.
  scoped_refptr<ChromeDownloadManagerDelegate> download_manager_delegate_;

  // The download manager that only stores downloaded items in memory.
  scoped_refptr<DownloadManager> download_manager_;

  // We use a non-persistent content settings map for OTR.
  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;

  // Use a separate zoom map for OTR.
  scoped_refptr<HostZoomMap> host_zoom_map_;

  // Use a special WebKit context for OTR browsing.
  scoped_refptr<WebKitContext> webkit_context_;

  // We don't want SSLHostState from the OTR profile to leak back to the main
  // profile because then the main profile would learn some of the host names
  // the user visited while OTR.
  scoped_ptr<SSLHostState> ssl_host_state_;

  // Use a separate FindBarState so search terms do not leak back to the main
  // profile.
  scoped_ptr<FindBarState> find_bar_state_;

  // The TransportSecurityState that only stores enabled sites in memory.
  scoped_refptr<net::TransportSecurityState>
      transport_security_state_;

  // Time we were started.
  Time start_time_;

  scoped_refptr<ChromeAppCacheService> appcache_service_;

  // The main database tracker for this profile.
  // Should be used only on the file thread.
  scoped_refptr<webkit_database::DatabaseTracker> db_tracker_;

  FilePath last_selected_directory_;

  scoped_refptr<ChromeBlobStorageContext> blob_storage_context_;

  // The file_system context for this profile.
  scoped_refptr<fileapi::FileSystemContext> file_system_context_;

  scoped_refptr<PrefProxyConfigTracker> pref_proxy_config_tracker_;

  scoped_ptr<ChromeURLDataManager> chrome_url_data_manager_;

  scoped_refptr<quota::QuotaManager> quota_manager_;

  // Used read-only.
  scoped_refptr<TransportSecurityPersister> transport_security_loader_;

  DISALLOW_COPY_AND_ASSIGN(OffTheRecordProfileImpl);
};

#if defined(OS_CHROMEOS)
// Special case of the OffTheRecordProfileImpl which is used while Guest
// session in CrOS.
class GuestSessionProfile : public OffTheRecordProfileImpl {
 public:
  explicit GuestSessionProfile(Profile* real_profile)
      : OffTheRecordProfileImpl(real_profile) {
  }

  virtual PersonalDataManager* GetPersonalDataManager() {
    return GetOriginalProfile()->GetPersonalDataManager();
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
#if defined(OS_CHROMEOS)
  if (Profile::IsGuestSession())
    return new GuestSessionProfile(this);
#endif
  return new OffTheRecordProfileImpl(this);
}
