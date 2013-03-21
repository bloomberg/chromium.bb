// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class gathers state related to a single user profile.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_IMPL_H_
#define CHROME_BROWSER_PROFILES_PROFILE_IMPL_H_

#include <string>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/timer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_impl_io_data.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/host_zoom_map.h"

class NetPrefObserver;
class PrefRegistrySyncable;
class PrefService;
class PrefServiceSyncable;
class SSLConfigServiceManager;

#if defined(OS_CHROMEOS)
namespace chromeos {
class EnterpriseExtensionObserver;
class LocaleChangeGuard;
class Preferences;
}
#endif

namespace base {
class SequencedTaskRunner;
}

namespace content {
class SpeechRecognitionPreferences;
}

namespace extensions {
class ExtensionSystem;
}

namespace policy {
class UserCloudPolicyManager;
}

// The default profile implementation.
class ProfileImpl : public Profile {
 public:
  // Value written to prefs when the exit type is EXIT_NORMAL. Public for tests.
  static const char* const kPrefExitTypeNormal;

  virtual ~ProfileImpl();

  static void RegisterUserPrefs(PrefRegistrySyncable* registry);

  // content::BrowserContext implementation:
  virtual base::FilePath GetPath() OVERRIDE;
  virtual content::DownloadManagerDelegate*
      GetDownloadManagerDelegate() OVERRIDE;
  virtual net::URLRequestContextGetter* GetRequestContext() OVERRIDE;
  virtual net::URLRequestContextGetter* GetRequestContextForRenderProcess(
      int renderer_child_id) OVERRIDE;
  virtual net::URLRequestContextGetter* GetMediaRequestContext() OVERRIDE;
  virtual net::URLRequestContextGetter* GetMediaRequestContextForRenderProcess(
      int renderer_child_id) OVERRIDE;
  virtual net::URLRequestContextGetter*
      GetMediaRequestContextForStoragePartition(
          const base::FilePath& partition_path,
          bool in_memory) OVERRIDE;
  virtual content::ResourceContext* GetResourceContext() OVERRIDE;
  virtual content::GeolocationPermissionContext*
      GetGeolocationPermissionContext() OVERRIDE;
  virtual content::SpeechRecognitionPreferences*
      GetSpeechRecognitionPreferences() OVERRIDE;
  virtual quota::SpecialStoragePolicy* GetSpecialStoragePolicy() OVERRIDE;

  // Profile implementation:
  virtual scoped_refptr<base::SequencedTaskRunner> GetIOTaskRunner() OVERRIDE;
  virtual std::string GetProfileName() OVERRIDE;
  virtual bool IsOffTheRecord() const OVERRIDE;
  virtual Profile* GetOffTheRecordProfile() OVERRIDE;
  virtual void DestroyOffTheRecordProfile() OVERRIDE;
  virtual bool HasOffTheRecordProfile() OVERRIDE;
  virtual Profile* GetOriginalProfile() OVERRIDE;
  virtual history::TopSites* GetTopSites() OVERRIDE;
  virtual history::TopSites* GetTopSitesWithoutCreating() OVERRIDE;
  virtual ExtensionService* GetExtensionService() OVERRIDE;
  virtual ExtensionSpecialStoragePolicy*
      GetExtensionSpecialStoragePolicy() OVERRIDE;
  virtual policy::ManagedModePolicyProvider*
      GetManagedModePolicyProvider() OVERRIDE;
  virtual policy::PolicyService* GetPolicyService() OVERRIDE;
  virtual PrefService* GetPrefs() OVERRIDE;
  virtual PrefService* GetOffTheRecordPrefs() OVERRIDE;
  virtual net::URLRequestContextGetter*
      GetRequestContextForExtensions() OVERRIDE;
  virtual net::SSLConfigService* GetSSLConfigService() OVERRIDE;
  virtual HostContentSettingsMap* GetHostContentSettingsMap() OVERRIDE;
  virtual ProtocolHandlerRegistry* GetProtocolHandlerRegistry() OVERRIDE;
  virtual bool IsSameProfile(Profile* profile) OVERRIDE;
  virtual base::Time GetStartTime() const OVERRIDE;
  virtual net::URLRequestContextGetter* CreateRequestContext(
      content::ProtocolHandlerMap* protocol_handlers) OVERRIDE;
  virtual net::URLRequestContextGetter* CreateRequestContextForStoragePartition(
      const base::FilePath& partition_path,
      bool in_memory,
      content::ProtocolHandlerMap* protocol_handlers) OVERRIDE;
  virtual base::FilePath last_selected_directory() OVERRIDE;
  virtual void set_last_selected_directory(const base::FilePath& path) OVERRIDE;
  virtual chrome_browser_net::Predictor* GetNetworkPredictor() OVERRIDE;
  virtual void ClearNetworkingHistorySince(
      base::Time time,
      const base::Closure& completion) OVERRIDE;
  virtual GURL GetHomePage() OVERRIDE;
  virtual bool WasCreatedByVersionOrLater(const std::string& version) OVERRIDE;
  virtual void SetExitType(ExitType exit_type) OVERRIDE;
  virtual ExitType GetLastSessionExitType() OVERRIDE;

#if defined(OS_CHROMEOS)
  virtual void ChangeAppLocale(const std::string& locale,
                               AppLocaleChangedVia) OVERRIDE;
  virtual void OnLogin() OVERRIDE;
  virtual void SetupChromeOSEnterpriseExtensionObserver() OVERRIDE;
  virtual void InitChromeOSPreferences() OVERRIDE;
#endif  // defined(OS_CHROMEOS)

  virtual PrefProxyConfigTracker* GetProxyConfigTracker() OVERRIDE;

 private:
  friend class Profile;
  friend class BetterSessionRestoreCrashTest;
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorTest,
                           ProfilesLaunchedAfterCrash);
  FRIEND_TEST_ALL_PREFIXES(ProfileBrowserTest, ProfileReadmeCreated);
  FRIEND_TEST_ALL_PREFIXES(ProfileBrowserTest,
                           ProfileDeletedBeforeReadmeCreated);

  // Delay, in milliseconds, before README file is created for a new profile.
  // This is non-const for testing purposes.
  static int create_readme_delay_ms;

  ProfileImpl(const base::FilePath& path,
              Delegate* delegate,
              CreateMode create_mode,
              base::SequencedTaskRunner* sequenced_task_runner);

  // Does final initialization. Should be called after prefs were loaded.
  void DoFinalInit(bool is_new_profile);

  void InitHostZoomMap();

  void OnDefaultZoomLevelChanged();
  void OnZoomLevelChanged(
      const content::HostZoomMap::ZoomLevelChange& change);

  void OnInitializationCompleted(PrefService* pref_service,
                                 bool succeeded);

  // Does final prefs initialization and calls Init().
  void OnPrefsLoaded(bool success);

  base::FilePath GetPrefFilePath();

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
                          base::FilePath* cache_path,
                          int* max_size);

  content::HostZoomMap::ZoomLevelChangedCallback zoom_callback_;
  PrefChangeRegistrar pref_change_registrar_;

  base::FilePath path_;
  base::FilePath base_cache_path_;

  // !!! BIG HONKING WARNING !!!
  //  The order of the members below is important. Do not change it unless
  //  you know what you're doing. Also, if adding a new member here make sure
  //  that the declaration occurs AFTER things it depends on as destruction
  //  happens in reverse order of declaration.

#if defined(ENABLE_CONFIGURATION_POLICY)
  // |prefs_| depends on |policy_service_|, which depends on
  // |user_cloud_policy_manager_| and |managed_mode_policy_provider_|.
  // TODO(bauerb, mnissler): Once |prefs_| is a ProfileKeyedService, these
  // should become proper ProfileKeyedServices as well.
#if !defined(OS_CHROMEOS)
  scoped_ptr<policy::UserCloudPolicyManager> cloud_policy_manager_;
#endif
  scoped_ptr<policy::ManagedModePolicyProvider> managed_mode_policy_provider_;
#endif
  scoped_ptr<policy::PolicyService> policy_service_;

  // Keep |prefs_| on top for destruction order because |extension_prefs_|,
  // |net_pref_observer_|, |io_data_| and others store pointers to |prefs_| and
  // shall be destructed first.
  scoped_refptr<PrefRegistrySyncable> pref_registry_;
  scoped_ptr<PrefServiceSyncable> prefs_;
  scoped_ptr<PrefServiceSyncable> otr_prefs_;
  ProfileImplIOData::Handle io_data_;
  scoped_refptr<ExtensionSpecialStoragePolicy>
      extension_special_storage_policy_;
  scoped_ptr<NetPrefObserver> net_pref_observer_;
  scoped_ptr<SSLConfigServiceManager> ssl_config_service_manager_;
  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;
  scoped_refptr<history::ShortcutsBackend> shortcuts_backend_;

  // Exit type the last time the profile was opened. This is set only once from
  // prefs.
  ExitType last_session_exit_type_;

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

  DISALLOW_COPY_AND_ASSIGN(ProfileImpl);
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_IMPL_H_
