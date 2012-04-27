// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class gathers state related to a single user profile.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_IMPL_H_
#define CHROME_BROWSER_PROFILES_PROFILE_IMPL_H_
#pragma once

#include <string>

#include "base/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_impl_io_data.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class ExtensionNavigationObserver;
class ExtensionSystem;
class NetPrefObserver;
class PrefService;
class SSLConfigServiceManager;
class VisitedLinkEventListener;

#if defined(ENABLE_PROMO_RESOURCE_SERVICE)
class PromoResourceService;
#endif

#if defined(OS_CHROMEOS)
namespace chromeos {
class EnterpriseExtensionObserver;
class LocaleChangeGuard;
class Preferences;
}
#endif

namespace content {
class SpeechRecognitionPreferences;
}

// The default profile implementation.
class ProfileImpl : public Profile,
                    public content::NotificationObserver {
 public:
  virtual ~ProfileImpl();

  static void RegisterUserPrefs(PrefService* prefs);

  // content::BrowserContext implementation:
  virtual FilePath GetPath() OVERRIDE;
  virtual content::DownloadManager* GetDownloadManager() OVERRIDE;
  virtual net::URLRequestContextGetter* GetRequestContext() OVERRIDE;
  virtual net::URLRequestContextGetter* GetRequestContextForRenderProcess(
      int renderer_child_id) OVERRIDE;
  virtual net::URLRequestContextGetter* GetRequestContextForMedia() OVERRIDE;
  virtual content::ResourceContext* GetResourceContext() OVERRIDE;
  virtual content::GeolocationPermissionContext*
      GetGeolocationPermissionContext() OVERRIDE;
  virtual content::SpeechRecognitionPreferences*
      GetSpeechRecognitionPreferences() OVERRIDE;
  virtual bool DidLastSessionExitCleanly() OVERRIDE;
  virtual quota::SpecialStoragePolicy* GetSpecialStoragePolicy() OVERRIDE;

  // Profile implementation:
  virtual std::string GetProfileName() OVERRIDE;
  virtual bool IsOffTheRecord() const OVERRIDE;
  virtual Profile* GetOffTheRecordProfile() OVERRIDE;
  virtual void DestroyOffTheRecordProfile() OVERRIDE;
  virtual bool HasOffTheRecordProfile() OVERRIDE;
  virtual Profile* GetOriginalProfile() OVERRIDE;
  virtual history::TopSites* GetTopSites() OVERRIDE;
  virtual history::TopSites* GetTopSitesWithoutCreating() OVERRIDE;
  virtual VisitedLinkMaster* GetVisitedLinkMaster() OVERRIDE;
  virtual ExtensionService* GetExtensionService() OVERRIDE;
  virtual UserScriptMaster* GetUserScriptMaster() OVERRIDE;
  virtual ExtensionProcessManager* GetExtensionProcessManager() OVERRIDE;
  virtual ExtensionEventRouter* GetExtensionEventRouter() OVERRIDE;
  virtual ExtensionSpecialStoragePolicy*
      GetExtensionSpecialStoragePolicy() OVERRIDE;
  virtual FaviconService* GetFaviconService(ServiceAccessType sat) OVERRIDE;
  virtual GAIAInfoUpdateService* GetGAIAInfoUpdateService() OVERRIDE;
  virtual HistoryService* GetHistoryService(ServiceAccessType sat) OVERRIDE;
  virtual HistoryService* GetHistoryServiceWithoutCreating() OVERRIDE;
  virtual AutocompleteClassifier* GetAutocompleteClassifier() OVERRIDE;
  virtual history::ShortcutsBackend* GetShortcutsBackend() OVERRIDE;
  virtual WebDataService* GetWebDataService(ServiceAccessType sat) OVERRIDE;
  virtual WebDataService* GetWebDataServiceWithoutCreating() OVERRIDE;
  virtual PrefService* GetPrefs() OVERRIDE;
  virtual PrefService* GetOffTheRecordPrefs() OVERRIDE;
  virtual net::URLRequestContextGetter*
      GetRequestContextForExtensions() OVERRIDE;
  virtual net::URLRequestContextGetter* GetRequestContextForIsolatedApp(
      const std::string& app_id) OVERRIDE;
  virtual net::SSLConfigService* GetSSLConfigService() OVERRIDE;
  virtual HostContentSettingsMap* GetHostContentSettingsMap() OVERRIDE;
  virtual BookmarkModel* GetBookmarkModel() OVERRIDE;
  virtual ProtocolHandlerRegistry* GetProtocolHandlerRegistry() OVERRIDE;
  virtual bool IsSameProfile(Profile* profile) OVERRIDE;
  virtual base::Time GetStartTime() const OVERRIDE;
  virtual void MarkAsCleanShutdown() OVERRIDE;
  virtual void InitPromoResources() OVERRIDE;
  virtual void InitRegisteredProtocolHandlers() OVERRIDE;
  virtual FilePath last_selected_directory() OVERRIDE;
  virtual void set_last_selected_directory(const FilePath& path) OVERRIDE;
  virtual chrome_browser_net::Predictor* GetNetworkPredictor() OVERRIDE;
  virtual void ClearNetworkingHistorySince(base::Time time) OVERRIDE;
  virtual GURL GetHomePage() OVERRIDE;
  virtual bool WasCreatedByVersionOrLater(const std::string& version) OVERRIDE;

#if defined(OS_CHROMEOS)
  virtual void ChangeAppLocale(const std::string& locale,
                               AppLocaleChangedVia) OVERRIDE;
  virtual void OnLogin() OVERRIDE;
  virtual void SetupChromeOSEnterpriseExtensionObserver() OVERRIDE;
  virtual void InitChromeOSPreferences() OVERRIDE;
#endif  // defined(OS_CHROMEOS)

  virtual PrefProxyConfigTracker* GetProxyConfigTracker() OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  friend class Profile;
  FRIEND_TEST_ALL_PREFIXES(BrowserInitTest, ProfilesLaunchedAfterCrash);
  FRIEND_TEST_ALL_PREFIXES(ProfileBrowserTest, ProfileReadmeCreated);
  FRIEND_TEST_ALL_PREFIXES(ProfileBrowserTest,
                           ProfileDeletedBeforeReadmeCreated);

  // Delay, in milliseconds, before README file is created for a new profile.
  // This is non-const for testing purposes.
  static int create_readme_delay_ms;

  ProfileImpl(const FilePath& path,
              Delegate* delegate,
              CreateMode create_mode);

  // Does final initialization. Should be called after prefs were loaded.
  void DoFinalInit(bool is_new_profile);

  void InitHostZoomMap();

  // The installation of any pre-defined protocol handlers.
  void InstallDefaultProtocolHandlers();

  // Does final prefs initialization and calls Init().
  void OnPrefsLoaded(bool success);

  void CreateWebDataService();
  FilePath GetPrefFilePath();

#if defined(ENABLE_SESSION_SERVICE)
  void StopCreateSessionServiceTimer();

  void EnsureSessionServiceCreated();
#endif


  void EnsureRequestContextCreated() {
    GetRequestContext();
  }

  void UpdateProfileUserNameCache();

  // Updates the ProfileInfoCache with data from this profile.
  void UpdateProfileNameCache();
  void UpdateProfileAvatarCache();

  void GetCacheParameters(bool is_media_context,
                          FilePath* cache_path,
                          int* max_size);

  virtual base::Callback<ChromeURLDataManagerBackend*(void)>
      GetChromeURLDataManagerBackendGetter() const OVERRIDE;

  content::NotificationRegistrar registrar_;
  PrefChangeRegistrar pref_change_registrar_;

  FilePath path_;
  FilePath base_cache_path_;

  // !!! BIG HONKING WARNING !!!
  //  The order of the members below is important. Do not change it unless
  //  you know what you're doing. Also, if adding a new member here make sure
  //  that the declaration occurs AFTER things it depends on as destruction
  //  happens in reverse order of declaration.

  // Keep |prefs_| on top for destruction order because |extension_prefs_|,
  // |net_pref_observer_|, |web_resource_service_|, and |io_data_| store
  // pointers to |prefs_| and shall be destructed first.
  scoped_ptr<PrefService> prefs_;
  scoped_ptr<PrefService> otr_prefs_;
  scoped_ptr<VisitedLinkEventListener> visited_link_event_listener_;
  scoped_ptr<VisitedLinkMaster> visited_link_master_;
  ProfileImplIOData::Handle io_data_;
  scoped_refptr<ExtensionSpecialStoragePolicy>
      extension_special_storage_policy_;
  scoped_ptr<NetPrefObserver> net_pref_observer_;
  scoped_ptr<BookmarkModel> bookmark_bar_model_;

#if defined(ENABLE_PROMO_RESOURCE_SERVICE)
  scoped_refptr<PromoResourceService> promo_resource_service_;
#endif

  scoped_refptr<ProtocolHandlerRegistry> protocol_handler_registry_;

  scoped_ptr<SSLConfigServiceManager> ssl_config_service_manager_;

  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;
  scoped_refptr<content::GeolocationPermissionContext>
      geolocation_permission_context_;
  scoped_refptr<content::SpeechRecognitionPreferences>
      speech_recognition_preferences_;
  scoped_ptr<GAIAInfoUpdateService> gaia_info_update_service_;
  scoped_refptr<HistoryService> history_service_;
  scoped_ptr<FaviconService> favicon_service_;
  scoped_ptr<AutocompleteClassifier> autocomplete_classifier_;
  scoped_refptr<history::ShortcutsBackend> shortcuts_backend_;
  scoped_refptr<WebDataService> web_data_service_;
  bool history_service_created_;
  bool favicon_service_created_;
  bool created_web_data_service_;
  bool clear_local_state_on_exit_;

  // Whether or not the last session exited cleanly. This is set only once.
  bool last_session_exited_cleanly_;

#if defined(ENABLE_SESSION_SERVICE)
  base::OneShotTimer<ProfileImpl> create_session_service_timer_;
#endif

  scoped_ptr<Profile> off_the_record_profile_;

  // See GetStartTime for details.
  base::Time start_time_;

  scoped_refptr<history::TopSites> top_sites_;  // For history and thumbnails.

#if defined(OS_CHROMEOS)
  scoped_ptr<chromeos::Preferences> chromeos_preferences_;

  scoped_ptr<chromeos::EnterpriseExtensionObserver>
      chromeos_enterprise_extension_observer_;

  scoped_ptr<chromeos::LocaleChangeGuard> locale_change_guard_;
#endif

  scoped_ptr<PrefProxyConfigTracker> pref_proxy_config_tracker_;

  // STOP!!!! DO NOT ADD ANY MORE ITEMS HERE!!!!
  //
  // Instead, make your Service/Manager/whatever object you're hanging off the
  // Profile use our new ProfileKeyedServiceFactory system instead. You can
  // find the design document here:
  //
  //   https://sites.google.com/a/chromium.org/dev/developers/design-documents/profile-architecture
  //
  // and you can read the raw headers here:
  //
  //   chrome/browser/profile/profile_keyed_service.h
  //   chrome/browser/profile/profile_keyed_service_factory.{h,cc}
  //   chrome/browser/profile/profile_keyed_dependency_manager.{h,cc}

  Profile::Delegate* delegate_;

  chrome_browser_net::Predictor* predictor_;

  bool session_restore_enabled_;

  DISALLOW_COPY_AND_ASSIGN(ProfileImpl);
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_IMPL_H_
