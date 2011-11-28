// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_OFF_THE_RECORD_PROFILE_IMPL_H_
#define CHROME_BROWSER_PROFILES_OFF_THE_RECORD_PROFILE_IMPL_H_
#pragma once

#include <string>

#include "chrome/browser/profiles/off_the_record_profile_io_data.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "content/public/browser/notification_observer.h"

using base::Time;
using base::TimeDelta;

////////////////////////////////////////////////////////////////////////////////
//
// OffTheRecordProfileImpl is a profile subclass that wraps an existing profile
// to make it suitable for the incognito mode.
//
// Note: This class is a leaf class and is not intended for subclassing.
// Providing this header file is for unit testing.
//
////////////////////////////////////////////////////////////////////////////////
class OffTheRecordProfileImpl : public Profile,
                                public BrowserList::Observer,
                                public content::NotificationObserver {
 public:
  explicit OffTheRecordProfileImpl(Profile* real_profile);
  virtual ~OffTheRecordProfileImpl();
  void Init();

  // Profile implementation.
  virtual std::string GetProfileName() OVERRIDE;
  virtual FilePath GetPath() OVERRIDE;
  virtual bool IsOffTheRecord() OVERRIDE;
  virtual Profile* GetOffTheRecordProfile() OVERRIDE;
  virtual void DestroyOffTheRecordProfile() OVERRIDE;
  virtual bool HasOffTheRecordProfile() OVERRIDE;
  virtual Profile* GetOriginalProfile() OVERRIDE;
  virtual ChromeAppCacheService* GetAppCacheService() OVERRIDE;
  virtual webkit_database::DatabaseTracker* GetDatabaseTracker() OVERRIDE;
  virtual VisitedLinkMaster* GetVisitedLinkMaster() OVERRIDE;
  virtual ExtensionService* GetExtensionService() OVERRIDE;
  virtual UserScriptMaster* GetUserScriptMaster() OVERRIDE;
  virtual ExtensionDevToolsManager* GetExtensionDevToolsManager() OVERRIDE;
  virtual ExtensionProcessManager* GetExtensionProcessManager() OVERRIDE;
  virtual ExtensionMessageService* GetExtensionMessageService() OVERRIDE;
  virtual ExtensionEventRouter* GetExtensionEventRouter() OVERRIDE;
  virtual ExtensionSpecialStoragePolicy*
      GetExtensionSpecialStoragePolicy() OVERRIDE;
  virtual SSLHostState* GetSSLHostState() OVERRIDE;
  virtual HistoryService* GetHistoryService(ServiceAccessType sat) OVERRIDE;
  virtual HistoryService* GetHistoryServiceWithoutCreating() OVERRIDE;
  virtual FaviconService* GetFaviconService(ServiceAccessType sat) OVERRIDE;
  virtual AutocompleteClassifier* GetAutocompleteClassifier() OVERRIDE;
  virtual history::ShortcutsBackend* GetShortcutsBackend() OVERRIDE;
  virtual WebDataService* GetWebDataService(ServiceAccessType sat) OVERRIDE;
  virtual WebDataService* GetWebDataServiceWithoutCreating() OVERRIDE;
  virtual PasswordStore* GetPasswordStore(ServiceAccessType sat) OVERRIDE;
  virtual PrefService* GetPrefs() OVERRIDE;
  virtual PrefService* GetOffTheRecordPrefs() OVERRIDE;
  virtual TemplateURLFetcher* GetTemplateURLFetcher() OVERRIDE;
  virtual DownloadManager* GetDownloadManager() OVERRIDE;
  virtual fileapi::FileSystemContext* GetFileSystemContext() OVERRIDE;
  virtual net::URLRequestContextGetter* GetRequestContext() OVERRIDE;
  virtual quota::QuotaManager* GetQuotaManager() OVERRIDE;
  virtual net::URLRequestContextGetter* GetRequestContextForRenderProcess(
      int renderer_child_id) OVERRIDE;
  virtual net::URLRequestContextGetter* GetRequestContextForMedia() OVERRIDE;
  virtual net::URLRequestContextGetter*
      GetRequestContextForExtensions() OVERRIDE;
  virtual net::URLRequestContextGetter* GetRequestContextForIsolatedApp(
      const std::string& app_id) OVERRIDE;
  virtual const content::ResourceContext& GetResourceContext() OVERRIDE;
  virtual net::SSLConfigService* GetSSLConfigService() OVERRIDE;
  virtual HostContentSettingsMap* GetHostContentSettingsMap() OVERRIDE;
  virtual HostZoomMap* GetHostZoomMap() OVERRIDE;
  virtual GeolocationPermissionContext*
      GetGeolocationPermissionContext() OVERRIDE;
  virtual SpeechInputPreferences* GetSpeechInputPreferences() OVERRIDE;
  virtual UserStyleSheetWatcher* GetUserStyleSheetWatcher() OVERRIDE;
  virtual FindBarState* GetFindBarState() OVERRIDE;
  virtual bool HasProfileSyncService() const OVERRIDE;
  virtual bool DidLastSessionExitCleanly() OVERRIDE;
  virtual BookmarkModel* GetBookmarkModel() OVERRIDE;
  virtual ProtocolHandlerRegistry* GetProtocolHandlerRegistry() OVERRIDE;
  virtual TokenService* GetTokenService() OVERRIDE;
  virtual ProfileSyncService* GetProfileSyncService() OVERRIDE;
  virtual ProfileSyncService* GetProfileSyncService(
      const std::string& cros_user) OVERRIDE;
  virtual bool IsSameProfile(Profile* profile) OVERRIDE;
  virtual Time GetStartTime() const OVERRIDE;
  virtual SpellCheckHost* GetSpellCheckHost() OVERRIDE;
  virtual void ReinitializeSpellCheckHost(bool force) OVERRIDE;
  virtual WebKitContext* GetWebKitContext() OVERRIDE;
  virtual history::TopSites* GetTopSitesWithoutCreating() OVERRIDE;
  virtual history::TopSites* GetTopSites() OVERRIDE;
  virtual void MarkAsCleanShutdown() OVERRIDE;
  virtual void InitExtensions(bool extensions_enabled) OVERRIDE;
  virtual void InitPromoResources() OVERRIDE;
  virtual void InitRegisteredProtocolHandlers() OVERRIDE;
  virtual FilePath last_selected_directory() OVERRIDE;
  virtual void set_last_selected_directory(const FilePath& path) OVERRIDE;

#if defined(OS_CHROMEOS)
  virtual void SetupChromeOSEnterpriseExtensionObserver() OVERRIDE;
  virtual void InitChromeOSPreferences() OVERRIDE;
#endif  // defined(OS_CHROMEOS)

  virtual void OnBrowserAdded(const Browser* browser) OVERRIDE;
  virtual void OnBrowserRemoved(const Browser* browser) OVERRIDE;
  virtual ChromeBlobStorageContext* GetBlobStorageContext() OVERRIDE;
  virtual ExtensionInfoMap* GetExtensionInfoMap() OVERRIDE;
  virtual ChromeURLDataManager* GetChromeURLDataManager() OVERRIDE;
  virtual PromoCounter* GetInstantPromoCounter() OVERRIDE;

#if defined(OS_CHROMEOS)
  virtual void ChangeAppLocale(const std::string& locale,
                               AppLocaleChangedVia) OVERRIDE;
  virtual void OnLogin() OVERRIDE;
#endif  // defined(OS_CHROMEOS)

  virtual PrefProxyConfigTracker* GetProxyConfigTracker() OVERRIDE;

  virtual chrome_browser_net::Predictor* GetNetworkPredictor() OVERRIDE;
  virtual void ClearNetworkingHistorySince(base::Time time) OVERRIDE;
  virtual GURL GetHomePage() OVERRIDE;
  virtual NetworkActionPredictor* GetNetworkActionPredictor() OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  void CreateQuotaManagerAndClients();

  content::NotificationRegistrar registrar_;

  // The real underlying profile.
  Profile* profile_;

  // Weak pointer owned by |profile_|.
  PrefService* prefs_;

  OffTheRecordProfileIOData::Handle io_data_;

  // Must be freed before |io_data_|. While |extension_process_manager_| still
  // lives, we handle incoming resource requests from extension processes and
  // those require access to the ResourceContext owned by |io_data_|.
  scoped_ptr<ExtensionProcessManager> extension_process_manager_;

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

  scoped_ptr<PrefProxyConfigTracker> pref_proxy_config_tracker_;

  scoped_ptr<ChromeURLDataManager> chrome_url_data_manager_;

  scoped_refptr<quota::QuotaManager> quota_manager_;

  DISALLOW_COPY_AND_ASSIGN(OffTheRecordProfileImpl);
};

#endif  // CHROME_BROWSER_PROFILES_OFF_THE_RECORD_PROFILE_IMPL_H_
