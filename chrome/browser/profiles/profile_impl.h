// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class gathers state related to a single user profile.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_IMPL_H_
#define CHROME_BROWSER_PROFILES_PROFILE_IMPL_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_impl_io_data.h"
#include "chrome/browser/ui/zoom/chrome_zoom_level_prefs.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/host_zoom_map.h"

class NetPrefObserver;
class PrefService;

class TrackedPreferenceValidationDelegate;

#if defined(OS_CHROMEOS)
namespace chromeos {
class KioskTest;
class LocaleChangeGuard;
class Preferences;
class SupervisedUserTestBase;
}
#endif

namespace base {
class SequencedTaskRunner;
}

namespace domain_reliability {
class DomainReliabilityMonitor;
}

namespace extensions {
class ExtensionSystem;
}

namespace policy {
class CloudPolicyManager;
class ProfilePolicyConnector;
class SchemaRegistryService;
}

namespace ssl_config {
class SSLConfigServiceManager;
}

namespace syncable_prefs {
class PrefServiceSyncable;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// The default profile implementation.
class ProfileImpl : public Profile {
 public:
  // Value written to prefs when the exit type is EXIT_NORMAL. Public for tests.
  static const char kPrefExitTypeNormal[];

  ~ProfileImpl() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // content::BrowserContext implementation:
  std::unique_ptr<content::ZoomLevelDelegate> CreateZoomLevelDelegate(
      const base::FilePath& partition_path) override;
  base::FilePath GetPath() const override;
  content::DownloadManagerDelegate* GetDownloadManagerDelegate() override;
  content::ResourceContext* GetResourceContext() override;
  content::BrowserPluginGuestManager* GetGuestManager() override;
  storage::SpecialStoragePolicy* GetSpecialStoragePolicy() override;
  content::PushMessagingService* GetPushMessagingService() override;
  content::SSLHostStateDelegate* GetSSLHostStateDelegate() override;
  content::PermissionManager* GetPermissionManager() override;
  content::BackgroundSyncController* GetBackgroundSyncController() override;
  net::URLRequestContextGetter* CreateRequestContext(
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors) override;
  net::URLRequestContextGetter* CreateRequestContextForStoragePartition(
      const base::FilePath& partition_path,
      bool in_memory,
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors) override;
  net::URLRequestContextGetter* CreateMediaRequestContext() override;
  net::URLRequestContextGetter* CreateMediaRequestContextForStoragePartition(
      const base::FilePath& partition_path,
      bool in_memory) override;

  // Profile implementation:
  scoped_refptr<base::SequencedTaskRunner> GetIOTaskRunner() override;
  // Note that this implementation returns the Google-services username, if any,
  // not the Chrome user's display name.
  std::string GetProfileUserName() const override;
  ProfileType GetProfileType() const override;
  bool IsOffTheRecord() const override;
  Profile* GetOffTheRecordProfile() override;
  void DestroyOffTheRecordProfile() override;
  bool HasOffTheRecordProfile() override;
  Profile* GetOriginalProfile() override;
  bool IsSupervised() const override;
  bool IsChild() const override;
  bool IsLegacySupervised() const override;
  ExtensionSpecialStoragePolicy* GetExtensionSpecialStoragePolicy() override;
  PrefService* GetPrefs() override;
  const PrefService* GetPrefs() const override;
  ChromeZoomLevelPrefs* GetZoomLevelPrefs() override;
  PrefService* GetOffTheRecordPrefs() override;
  net::URLRequestContextGetter* GetRequestContext() override;
  net::URLRequestContextGetter* GetRequestContextForExtensions() override;
  net::SSLConfigService* GetSSLConfigService() override;
  bool IsSameProfile(Profile* profile) override;
  base::Time GetStartTime() const override;
  base::FilePath last_selected_directory() override;
  void set_last_selected_directory(const base::FilePath& path) override;
  chrome_browser_net::Predictor* GetNetworkPredictor() override;
  DevToolsNetworkControllerHandle* GetDevToolsNetworkControllerHandle()
      override;
  void ClearNetworkingHistorySince(base::Time time,
                                   const base::Closure& completion) override;
  GURL GetHomePage() override;
  bool WasCreatedByVersionOrLater(const std::string& version) override;
  void SetExitType(ExitType exit_type) override;
  ExitType GetLastSessionExitType() override;

#if defined(OS_CHROMEOS)
  void ChangeAppLocale(const std::string& locale, AppLocaleChangedVia) override;
  void OnLogin() override;
  void InitChromeOSPreferences() override;
#endif  // defined(OS_CHROMEOS)

  PrefProxyConfigTracker* GetProxyConfigTracker() override;

 private:
#if defined(OS_CHROMEOS)
  friend class chromeos::KioskTest;
  friend class chromeos::SupervisedUserTestBase;
#endif
  friend class Profile;
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorTest,
                           ProfilesLaunchedAfterCrash);
  FRIEND_TEST_ALL_PREFIXES(ProfileBrowserTest, DISABLED_ProfileReadmeCreated);
  FRIEND_TEST_ALL_PREFIXES(ProfileBrowserTest,
                           ProfileDeletedBeforeReadmeCreated);

  ProfileImpl(const base::FilePath& path,
              Delegate* delegate,
              CreateMode create_mode,
              base::SequencedTaskRunner* sequenced_task_runner);

  // Does final initialization. Should be called after prefs were loaded.
  void DoFinalInit();

  // Switch locale (when possible) and proceed to OnLocaleReady().
  void OnPrefsLoaded(CreateMode create_mode, bool success);

  // Does final prefs initialization and calls Init().
  void OnLocaleReady();

#if defined(ENABLE_SESSION_SERVICE)
  void StopCreateSessionServiceTimer();

  void EnsureSessionServiceCreated();
#endif

  // Updates the ProfileInfoCache with data from this profile.
  void UpdateProfileSupervisedUserIdCache();
  void UpdateProfileNameCache();
  void UpdateProfileAvatarCache();
  void UpdateProfileIsEphemeralCache();

  void GetCacheParameters(bool is_media_context,
                          base::FilePath* cache_path,
                          int* max_size);

  PrefProxyConfigTracker* CreateProxyConfigTracker();

  std::unique_ptr<domain_reliability::DomainReliabilityMonitor>
  CreateDomainReliabilityMonitor(PrefService* local_state);

  PrefChangeRegistrar pref_change_registrar_;

  base::FilePath path_;
  base::FilePath base_cache_path_;

  // !!! BIG HONKING WARNING !!!
  //  The order of the members below is important. Do not change it unless
  //  you know what you're doing. Also, if adding a new member here make sure
  //  that the declaration occurs AFTER things it depends on as destruction
  //  happens in reverse order of declaration.

  // TODO(mnissler, joaodasilva): The |profile_policy_connector_| provides the
  // PolicyService that the |prefs_| depend on, and must outlive |prefs_|.
// This can be removed once |prefs_| becomes a KeyedService too.
// |profile_policy_connector_| in turn depends on |cloud_policy_manager_|,
// which depends on |schema_registry_service_|.
  std::unique_ptr<policy::SchemaRegistryService> schema_registry_service_;
  std::unique_ptr<policy::CloudPolicyManager> cloud_policy_manager_;
  std::unique_ptr<policy::ProfilePolicyConnector> profile_policy_connector_;

  // Keep |pref_validation_delegate_| above |prefs_| so that the former outlives
  // the latter.
  std::unique_ptr<TrackedPreferenceValidationDelegate>
      pref_validation_delegate_;

  // Keep |prefs_| on top for destruction order because |extension_prefs_|,
  // |net_pref_observer_|, |io_data_| and others store pointers to |prefs_| and
  // shall be destructed first.
  scoped_refptr<user_prefs::PrefRegistrySyncable> pref_registry_;
  std::unique_ptr<syncable_prefs::PrefServiceSyncable> prefs_;
  std::unique_ptr<syncable_prefs::PrefServiceSyncable> otr_prefs_;
  ProfileImplIOData::Handle io_data_;
#if defined(ENABLE_EXTENSIONS)
  scoped_refptr<ExtensionSpecialStoragePolicy>
      extension_special_storage_policy_;
#endif
  std::unique_ptr<NetPrefObserver> net_pref_observer_;
  std::unique_ptr<ssl_config::SSLConfigServiceManager>
      ssl_config_service_manager_;

  // Exit type the last time the profile was opened. This is set only once from
  // prefs.
  ExitType last_session_exit_type_;

#if defined(ENABLE_SESSION_SERVICE)
  base::OneShotTimer create_session_service_timer_;
#endif

  std::unique_ptr<Profile> off_the_record_profile_;

  // See GetStartTime for details.
  base::Time start_time_;

#if defined(OS_CHROMEOS)
  std::unique_ptr<chromeos::Preferences> chromeos_preferences_;

  std::unique_ptr<chromeos::LocaleChangeGuard> locale_change_guard_;
#endif

  std::unique_ptr<PrefProxyConfigTracker> pref_proxy_config_tracker_;

  // STOP!!!! DO NOT ADD ANY MORE ITEMS HERE!!!!
  //
  // Instead, make your Service/Manager/whatever object you're hanging off the
  // Profile use our new BrowserContextKeyedServiceFactory system instead.
  // You can find the design document here:
  //
  //   https://sites.google.com/a/chromium.org/dev/developers/design-documents/profile-architecture
  //
  // and you can read the raw headers here:
  //
  // components/keyed_service/content/browser_context_dependency_manager.*
  // components/keyed_service/core/keyed_service.h
  // components/keyed_service/content/browser_context_keyed_service_factory.*

  Profile::Delegate* delegate_;

  chrome_browser_net::Predictor* predictor_;

  DISALLOW_COPY_AND_ASSIGN(ProfileImpl);
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_IMPL_H_
