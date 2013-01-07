// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/browser_policy_connector.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/async_policy_provider.h"
#include "chrome/browser/policy/cloud_policy_client.h"
#include "chrome/browser/policy/cloud_policy_provider.h"
#include "chrome/browser/policy/cloud_policy_service.h"
#include "chrome/browser/policy/cloud_policy_subsystem.h"
#include "chrome/browser/policy/configuration_policy_provider.h"
#include "chrome/browser/policy/device_management_service.h"
#include "chrome/browser/policy/managed_mode_policy_provider.h"
#include "chrome/browser/policy/policy_service_impl.h"
#include "chrome/browser/policy/policy_statistics_collector.h"
#include "chrome/browser/policy/user_policy_cache.h"
#include "chrome/browser/policy/user_policy_token_cache.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"
#include "grit/generated_resources.h"
#include "policy/policy_constants.h"
#include "third_party/icu/public/i18n/unicode/regex.h"

#if defined(OS_WIN)
#include "chrome/browser/policy/policy_loader_win.h"
#elif defined(OS_MACOSX)
#include "chrome/browser/policy/policy_loader_mac.h"
#include "chrome/browser/preferences_mac.h"
#elif defined(OS_POSIX)
#include "chrome/browser/policy/config_dir_policy_loader.h"
#endif

#if defined(OS_CHROMEOS)
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/cros_settings_provider.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/chromeos/system/statistics_provider.h"
#include "chrome/browser/chromeos/system/timezone_settings.h"
#include "chrome/browser/policy/app_pack_updater.h"
#include "chrome/browser/policy/cros_user_policy_cache.h"
#include "chrome/browser/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/policy/device_cloud_policy_store_chromeos.h"
#include "chrome/browser/policy/device_local_account_policy_provider.h"
#include "chrome/browser/policy/device_local_account_policy_service.h"
#include "chrome/browser/policy/device_policy_cache.h"
#include "chrome/browser/policy/network_configuration_updater.h"
#include "chrome/browser/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/policy/user_cloud_policy_store_chromeos.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#else
#include "chrome/browser/policy/user_cloud_policy_manager.h"
#include "chrome/browser/policy/user_cloud_policy_manager_factory.h"
#endif

using content::BrowserThread;

namespace policy {

namespace {

// Subdirectory in the user's profile for storing user policies.
const FilePath::CharType kPolicyDir[] = FILE_PATH_LITERAL("Device Management");
// File in the above directory for stroing user policy dmtokens.
const FilePath::CharType kTokenCacheFile[] = FILE_PATH_LITERAL("Token");
// File in the above directory for storing user policy data.
const FilePath::CharType kPolicyCacheFile[] = FILE_PATH_LITERAL("Policy");

// The following constants define delays applied before the initial policy fetch
// on startup. (So that displaying Chrome's GUI does not get delayed.)
// Delay in milliseconds from startup.
const int64 kServiceInitializationStartupDelay = 5000;

// The URL for the device management server.
const char kDefaultDeviceManagementServerUrl[] =
    "https://m.google.com/devicemanagement/data/api";

// Used in BrowserPolicyConnector::SetPolicyProviderForTesting.
ConfigurationPolicyProvider* g_testing_provider = NULL;

}  // namespace

BrowserPolicyConnector::BrowserPolicyConnector()
    : is_initialized_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {}

BrowserPolicyConnector::~BrowserPolicyConnector() {
  if (is_initialized()) {
    // Shutdown() wasn't invoked by our owner after having called Init().
    // This usually means it's an early shutdown and
    // BrowserProcessImpl::StartTearDown() wasn't invoked.
    // Cleanup properly in those cases and avoid crashing the ToastCrasher test.
    Shutdown();
  }
}

void BrowserPolicyConnector::Init() {
  DCHECK(!is_initialized()) << "BrowserPolicyConnector::Init() called twice.";
  platform_provider_.reset(CreatePlatformProvider());

  if (!device_management_service_.get()) {
    device_management_service_.reset(
        new DeviceManagementService(GetDeviceManagementUrl()));
  }

#if defined(OS_CHROMEOS)
  chromeos::CryptohomeLibrary* cryptohome =
      chromeos::CrosLibrary::Get()->GetCryptohomeLibrary();
  install_attributes_.reset(new EnterpriseInstallAttributes(cryptohome));

  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kDisableCloudPolicyService)) {
    scoped_ptr<DeviceCloudPolicyStoreChromeOS> device_cloud_policy_store(
        new DeviceCloudPolicyStoreChromeOS(
            chromeos::DeviceSettingsService::Get(),
            install_attributes_.get()));
    device_cloud_policy_manager_.reset(
        new DeviceCloudPolicyManagerChromeOS(
            device_cloud_policy_store.Pass(),
            install_attributes_.get()));
    if (!command_line->HasSwitch(switches::kDisableLocalAccounts)) {
      device_local_account_policy_service_.reset(
          new DeviceLocalAccountPolicyService(
              chromeos::DBusThreadManager::Get()->GetSessionManagerClient(),
              chromeos::DeviceSettingsService::Get()));
    }
  } else {
    cloud_provider_.reset(new CloudPolicyProvider(this));
  }

  InitializeDevicePolicy();
#endif

  // Complete the initialization once the message loops are spinning.
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&BrowserPolicyConnector::CompleteInitialization,
                 weak_ptr_factory_.GetWeakPtr()));

  is_initialized_ = true;
}

void BrowserPolicyConnector::Shutdown() {
  is_initialized_ = false;

  if (g_testing_provider)
    g_testing_provider->Shutdown();
  // Drop g_testing_provider so that tests executed with --single_process can
  // call SetPolicyProviderForTesting() again. It is still owned by the test.
  g_testing_provider = NULL;
  if (platform_provider_)
    platform_provider_->Shutdown();
  // The |cloud_provider_| must be shut down before destroying the cloud
  // policy subsystems, which own the caches that |cloud_provider_| uses.
  if (cloud_provider_)
    cloud_provider_->Shutdown();

#if defined(OS_CHROMEOS)
  // Shutdown device cloud policy.
  if (device_cloud_policy_subsystem_)
    device_cloud_policy_subsystem_->Shutdown();
  // The AppPackUpdater may be observing the |device_cloud_policy_subsystem_|.
  // Delete it first.
  app_pack_updater_.reset();
  device_cloud_policy_subsystem_.reset();
  device_data_store_.reset();

  if (device_cloud_policy_manager_)
    device_cloud_policy_manager_->Shutdown();
  if (device_local_account_policy_provider_)
    device_local_account_policy_provider_->Shutdown();
  if (device_local_account_policy_service_)
    device_local_account_policy_service_->Disconnect();
  if (user_cloud_policy_manager_)
    user_cloud_policy_manager_->Shutdown();
  global_user_cloud_policy_provider_.Shutdown();
#endif

  // Shutdown user cloud policy.
  if (user_cloud_policy_subsystem_)
    user_cloud_policy_subsystem_->Shutdown();
  user_cloud_policy_subsystem_.reset();
  user_policy_token_cache_.reset();
  user_data_store_.reset();

  device_management_service_.reset();
}

scoped_ptr<PolicyService> BrowserPolicyConnector::CreatePolicyService(
    Profile* profile) {
  DCHECK(profile);
  ConfigurationPolicyProvider* user_cloud_policy_provider = NULL;
#if defined(OS_CHROMEOS)
  user_cloud_policy_provider = user_cloud_policy_manager_.get();
#else
  user_cloud_policy_provider =
      UserCloudPolicyManagerFactory::GetForProfile(profile);
#endif
  return CreatePolicyServiceWithProviders(
      user_cloud_policy_provider,
      profile->GetManagedModePolicyProvider());
}

PolicyService* BrowserPolicyConnector::GetPolicyService() {
  if (!policy_service_)
    policy_service_ = CreatePolicyServiceWithProviders(NULL, NULL);
  return policy_service_.get();
}

void BrowserPolicyConnector::RegisterForDevicePolicy(
    const std::string& owner_email,
    const std::string& token,
    bool known_machine_id,
    bool reregister) {
#if defined(OS_CHROMEOS)
  if (device_data_store_.get()) {
    if (!device_data_store_->device_token().empty()) {
      LOG(ERROR) << "Device policy data store already has a DMToken; "
                 << "RegisterForDevicePolicy won't trigger a new registration.";
    }

    device_data_store_->set_user_name(owner_email);
    device_data_store_->set_known_machine_id(known_machine_id);
    if (reregister) {
      device_data_store_->set_device_id(install_attributes_->GetDeviceId());
      device_data_store_->set_reregister(true);
    }
    device_data_store_->set_policy_fetching_enabled(false);
    device_data_store_->SetOAuthToken(token);
  }
#endif
}

bool BrowserPolicyConnector::IsEnterpriseManaged() {
#if defined(OS_CHROMEOS)
  return install_attributes_.get() && install_attributes_->IsEnterpriseDevice();
#else
  return false;
#endif
}

EnterpriseInstallAttributes::LockResult
    BrowserPolicyConnector::LockDevice(const std::string& user) {
#if defined(OS_CHROMEOS)
  if (install_attributes_.get()) {
    return install_attributes_->LockDevice(user,
                                           device_data_store_->device_mode(),
                                           device_data_store_->device_id());
  }
#endif

  return EnterpriseInstallAttributes::LOCK_BACKEND_ERROR;
}

std::string BrowserPolicyConnector::GetEnterpriseDomain() {
#if defined(OS_CHROMEOS)
  if (install_attributes_.get())
    return install_attributes_->GetDomain();
#endif

  return std::string();
}

DeviceMode BrowserPolicyConnector::GetDeviceMode() {
#if defined(OS_CHROMEOS)
  if (install_attributes_.get())
    return install_attributes_->GetMode();
  else
    return DEVICE_MODE_NOT_SET;
#endif

  // We only have the notion of "enterprise" device on ChromeOS for now.
  return DEVICE_MODE_CONSUMER;
}

void BrowserPolicyConnector::ResetDevicePolicy() {
#if defined(OS_CHROMEOS)
  if (device_cloud_policy_subsystem_.get())
    device_cloud_policy_subsystem_->Reset();
#endif
}

void BrowserPolicyConnector::FetchCloudPolicy() {
#if defined(OS_CHROMEOS)
  if (device_cloud_policy_subsystem_.get())
    device_cloud_policy_subsystem_->RefreshPolicies(false);
  if (user_cloud_policy_subsystem_.get())
    user_cloud_policy_subsystem_->RefreshPolicies(true);  // wait_for_auth_token
#endif
}

void BrowserPolicyConnector::ScheduleServiceInitialization(
    int64 delay_milliseconds) {
  if (device_management_service_.get())
    device_management_service_->ScheduleInitialization(delay_milliseconds);
  if (user_cloud_policy_subsystem_.get()) {
    user_cloud_policy_subsystem_->
        ScheduleServiceInitialization(delay_milliseconds);
  }
#if defined(OS_CHROMEOS)
  if (device_cloud_policy_subsystem_.get()) {
    device_cloud_policy_subsystem_->
        ScheduleServiceInitialization(delay_milliseconds);
  }
#endif
}

void BrowserPolicyConnector::InitializeUserPolicy(
    const std::string& user_name,
    bool is_public_account,
    bool wait_for_policy_fetch) {
#if defined(OS_CHROMEOS)
  // If the user is managed then importing certificates from ONC policy is
  // allowed, otherwise it's not. Update this flag once the user has signed in,
  // and before user policy is loaded.
  GetNetworkConfigurationUpdater()->set_allow_web_trust(
      GetUserAffiliation(user_name) == USER_AFFILIATION_MANAGED);

  // Re-initializing user policy is disallowed for two reasons:
  // (a) Existing profiles may hold pointers to |user_cloud_policy_manager_|.
  // (b) Implementing UserCloudPolicyManager::IsInitializationComplete()
  //     correctly is impossible for re-initialization.
  CHECK(!user_cloud_policy_manager_.get());
#endif

  // Throw away the old backend.
  user_cloud_policy_subsystem_.reset();
  user_policy_token_cache_.reset();
  user_data_store_.reset();
  token_service_ = NULL;
  registrar_.RemoveAll();

  CommandLine* command_line = CommandLine::ForCurrentProcess();

  int64 startup_delay =
      wait_for_policy_fetch ? 0 : kServiceInitializationStartupDelay;

  FilePath profile_dir;
  PathService::Get(chrome::DIR_USER_DATA, &profile_dir);
#if defined(OS_CHROMEOS)
  profile_dir = profile_dir.Append(
      command_line->GetSwitchValuePath(switches::kLoginProfile));
#endif
  const FilePath policy_dir = profile_dir.Append(kPolicyDir);
  const FilePath policy_cache_file = policy_dir.Append(kPolicyCacheFile);
  const FilePath token_cache_file = policy_dir.Append(kTokenCacheFile);

  if (!command_line->HasSwitch(switches::kDisableCloudPolicyService)) {
#if defined(OS_CHROMEOS)
    device_management_service_->ScheduleInitialization(startup_delay);
    if (is_public_account && device_local_account_policy_service_.get()) {
      device_local_account_policy_provider_.reset(
          new DeviceLocalAccountPolicyProvider(
              user_name, device_local_account_policy_service_.get()));

      device_local_account_policy_provider_->Init();
      global_user_cloud_policy_provider_.SetDelegate(
          device_local_account_policy_provider_.get());
    } else if (!IsNonEnterpriseUser(user_name)) {
      scoped_ptr<CloudPolicyStore> store(
          new UserCloudPolicyStoreChromeOS(
              chromeos::DBusThreadManager::Get()->GetSessionManagerClient(),
              user_name, policy_cache_file, token_cache_file));
      user_cloud_policy_manager_.reset(
          new UserCloudPolicyManagerChromeOS(store.Pass(),
                                             wait_for_policy_fetch));

      user_cloud_policy_manager_->Init();
      user_cloud_policy_manager_->Connect(g_browser_process->local_state(),
                                          device_management_service_.get(),
                                          GetUserAffiliation(user_name));
      global_user_cloud_policy_provider_.SetDelegate(
          user_cloud_policy_manager_.get());
    }
#endif
  } else {
    CloudPolicyCacheBase* user_policy_cache = NULL;

    user_data_store_.reset(CloudPolicyDataStore::CreateForUserPolicies());
#if defined(OS_CHROMEOS)
    user_policy_cache =
        new CrosUserPolicyCache(
            chromeos::DBusThreadManager::Get()->GetSessionManagerClient(),
            user_data_store_.get(),
            wait_for_policy_fetch,
            token_cache_file,
            policy_cache_file);
#else
    user_policy_cache = new UserPolicyCache(policy_cache_file,
                                            wait_for_policy_fetch);
    user_policy_token_cache_.reset(
        new UserPolicyTokenCache(user_data_store_.get(), token_cache_file));

    // Initiate the DM-Token load.
    user_policy_token_cache_->Load();
#endif

    user_cloud_policy_subsystem_.reset(new CloudPolicySubsystem(
        user_data_store_.get(),
        user_policy_cache,
        GetDeviceManagementUrl()));

    user_data_store_->set_user_name(user_name);
    user_data_store_->set_user_affiliation(GetUserAffiliation(user_name));

    user_cloud_policy_subsystem_->CompleteInitialization(
        prefs::kUserPolicyRefreshRate,
        startup_delay);

    cloud_provider_->SetUserPolicyCache(user_policy_cache);
  }
}

void BrowserPolicyConnector::SetUserPolicyTokenService(
    TokenService* token_service) {
  token_service_ = token_service;
  registrar_.Add(this,
                 chrome::NOTIFICATION_TOKEN_AVAILABLE,
                 content::Source<TokenService>(token_service_));

  if (token_service_->HasTokenForService(
          GaiaConstants::kDeviceManagementService)) {
    user_data_store_->SetGaiaToken(token_service_->GetTokenForService(
        GaiaConstants::kDeviceManagementService));
  }
}

void BrowserPolicyConnector::RegisterForUserPolicy(
    const std::string& oauth_token) {
  if (oauth_token.empty()) {
    // An attempt to fetch the dm service oauth token has failed. Notify
    // the user policy cache of this, so that a potential blocked login
    // proceeds without waiting for user policy.
    if (user_cloud_policy_subsystem_.get()) {
      user_cloud_policy_subsystem_->GetCloudPolicyCacheBase()->
          SetFetchingDone();
    }
  } else {
    if (user_data_store_.get())
      user_data_store_->SetOAuthToken(oauth_token);
  }
}

CloudPolicyDataStore* BrowserPolicyConnector::GetDeviceCloudPolicyDataStore() {
#if defined(OS_CHROMEOS)
  return device_data_store_.get();
#else
  return NULL;
#endif
}

CloudPolicyDataStore* BrowserPolicyConnector::GetUserCloudPolicyDataStore() {
  return user_data_store_.get();
}

const ConfigurationPolicyHandlerList*
    BrowserPolicyConnector::GetHandlerList() const {
  return &handler_list_;
}

UserAffiliation BrowserPolicyConnector::GetUserAffiliation(
    const std::string& user_name) {
#if defined(OS_CHROMEOS)
  if (install_attributes_.get() &&
      gaia::ExtractDomainName(gaia::CanonicalizeEmail(user_name)) ==
          install_attributes_->GetDomain()) {
    return USER_AFFILIATION_MANAGED;
  }
#endif

  return USER_AFFILIATION_NONE;
}

AppPackUpdater* BrowserPolicyConnector::GetAppPackUpdater() {
#if defined(OS_CHROMEOS)
  if (!app_pack_updater_.get()) {
    // system_request_context() is NULL in unit tests.
    net::URLRequestContextGetter* request_context =
        g_browser_process->system_request_context();
    if (request_context) {
      app_pack_updater_.reset(
          new AppPackUpdater(request_context, install_attributes_.get()));
    }
  }
  return app_pack_updater_.get();
#else
  return NULL;
#endif
}

NetworkConfigurationUpdater*
    BrowserPolicyConnector::GetNetworkConfigurationUpdater() {
#if defined(OS_CHROMEOS)
  if (!network_configuration_updater_.get()) {
    network_configuration_updater_.reset(new NetworkConfigurationUpdater(
        g_browser_process->policy_service(),
        chromeos::CrosLibrary::Get()->GetNetworkLibrary()));
  }
  return network_configuration_updater_.get();
#else
  return NULL;
#endif
}

void BrowserPolicyConnector::SetDeviceManagementServiceForTesting(
    scoped_ptr<DeviceManagementService> service) {
  device_management_service_ = service.Pass();
}

// static
void BrowserPolicyConnector::SetPolicyProviderForTesting(
    ConfigurationPolicyProvider* provider) {
  CHECK(!g_browser_process) << "Must be invoked before the browser is created";
  DCHECK(!g_testing_provider);
  g_testing_provider = provider;
}

// static
std::string BrowserPolicyConnector::GetDeviceManagementUrl() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDeviceManagementUrl))
    return command_line->GetSwitchValueASCII(switches::kDeviceManagementUrl);
  else
    return kDefaultDeviceManagementServerUrl;
}

namespace {

// Returns true if |domain| matches the regex |pattern|.
bool MatchDomain(const string16& domain, const string16& pattern) {
  UErrorCode status = U_ZERO_ERROR;
  const icu::UnicodeString icu_pattern(pattern.data(), pattern.length());
  icu::RegexMatcher matcher(icu_pattern, UREGEX_CASE_INSENSITIVE, status);
  DCHECK(U_SUCCESS(status)) << "Invalid domain pattern: " << pattern;
  icu::UnicodeString icu_input(domain.data(), domain.length());
  matcher.reset(icu_input);
  status = U_ZERO_ERROR;
  UBool match = matcher.matches(status);
  DCHECK(U_SUCCESS(status));
  return !!match;  // !! == convert from UBool to bool
}

}  // namespace

// static
bool BrowserPolicyConnector::IsNonEnterpriseUser(const std::string& username) {
  if (username.empty()) {
    // This means incognito user in case of ChromiumOS and
    // no logged-in user in case of Chromium (SigninService).
    return true;
  }

  // Exclude many of the larger public email providers as we know these users
  // are not from hosted enterprise domains.
  static const wchar_t* kNonManagedDomainPatterns[] = {
    L"aol\\.com",
    L"googlemail\\.com",
    L"gmail\\.com",
    L"hotmail(\\.co|\\.com|)\\.[^.]+", // hotmail.com, hotmail.it, hotmail.co.uk
    L"live\\.com",
    L"mail\\.ru",
    L"msn\\.com",
    L"qq\\.com",
    L"yahoo(\\.co|\\.com|)\\.[^.]+", // yahoo.com, yahoo.co.uk, yahoo.com.tw
    L"yandex\\.ru",
  };
  const string16 domain =
      UTF8ToUTF16(gaia::ExtractDomainName(gaia::CanonicalizeEmail(username)));
  for (size_t i = 0; i < arraysize(kNonManagedDomainPatterns); i++) {
    string16 pattern = WideToUTF16(kNonManagedDomainPatterns[i]);
    if (MatchDomain(domain, pattern))
      return true;
  }
  return false;
}

void BrowserPolicyConnector::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (type == chrome::NOTIFICATION_TOKEN_AVAILABLE) {
    const TokenService* token_source =
        content::Source<const TokenService>(source).ptr();
    DCHECK_EQ(token_service_, token_source);
    const TokenService::TokenAvailableDetails* token_details =
        content::Details<const TokenService::TokenAvailableDetails>(details).
            ptr();
    if (token_details->service() == GaiaConstants::kDeviceManagementService) {
      if (user_data_store_.get()) {
        user_data_store_->SetGaiaToken(token_details->token());
      }
    }
  } else {
    NOTREACHED();
  }
}

void BrowserPolicyConnector::InitializeDevicePolicy() {
#if defined(OS_CHROMEOS)
  // Throw away the old backend.
  device_cloud_policy_subsystem_.reset();
  device_data_store_.reset();

  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisableCloudPolicyService)) {
    device_data_store_.reset(CloudPolicyDataStore::CreateForDevicePolicies());
    DevicePolicyCache* device_policy_cache =
        new DevicePolicyCache(device_data_store_.get(),
                              install_attributes_.get());

    cloud_provider_->SetDevicePolicyCache(device_policy_cache);

    device_cloud_policy_subsystem_.reset(new CloudPolicySubsystem(
        device_data_store_.get(),
        device_policy_cache,
        GetDeviceManagementUrl()));
  }
#endif
}

void BrowserPolicyConnector::CompleteInitialization() {
  if (g_testing_provider)
    g_testing_provider->Init();
  if (platform_provider_)
    platform_provider_->Init();
  if (cloud_provider_)
    cloud_provider_->Init();

#if defined(OS_CHROMEOS)
  global_user_cloud_policy_provider_.Init();

  // Create the AppPackUpdater to start updating the cache. It requires the
  // system request context, which isn't available in Init(); therefore it is
  // created only once the loops are running.
  GetAppPackUpdater();

  if (device_cloud_policy_subsystem_.get()) {
    // Read serial number and machine model. This must be done before we call
    // CompleteInitialization() below such that the serial number is available
    // for re-submission in case we're doing serial number recovery.
    if (device_data_store_->machine_id().empty() ||
        device_data_store_->machine_model().empty()) {
      device_data_store_->set_machine_id(
          DeviceCloudPolicyManagerChromeOS::GetMachineID());
      device_data_store_->set_machine_model(
          DeviceCloudPolicyManagerChromeOS::GetMachineModel());
    }

    device_cloud_policy_subsystem_->CompleteInitialization(
        prefs::kDevicePolicyRefreshRate,
        kServiceInitializationStartupDelay);
  }

  if (device_data_store_.get()) {
    device_data_store_->set_device_status_collector(
        new DeviceStatusCollector(
            g_browser_process->local_state(),
            chromeos::system::StatisticsProvider::GetInstance(),
            NULL));
  }

  if (device_cloud_policy_manager_.get()) {
    device_cloud_policy_manager_->Init();
    scoped_ptr<CloudPolicyClient::StatusProvider> status_provider(
        new DeviceStatusCollector(g_browser_process->local_state(),
            chromeos::system::StatisticsProvider::GetInstance(),
            NULL));
    device_cloud_policy_manager_->Connect(
        g_browser_process->local_state(),
        device_management_service_.get(),
        status_provider.Pass());
  }

  if (device_local_account_policy_service_.get()) {
    device_local_account_policy_service_->Connect(
        device_management_service_.get());
  }

  SetTimezoneIfPolicyAvailable();
#endif

  // TODO: Do not use g_browser_process once policy service is moved to
  // BrowserPolicyConnector (http://crbug.com/128999).
  policy_statistics_collector_.reset(
      new policy::PolicyStatisticsCollector(
          g_browser_process->policy_service(),
          g_browser_process->local_state(),
          MessageLoop::current()->message_loop_proxy()));
  policy_statistics_collector_->Initialize();
}

void BrowserPolicyConnector::SetTimezoneIfPolicyAvailable() {
#if defined(OS_CHROMEOS)
  typedef chromeos::CrosSettingsProvider Provider;
  Provider::TrustedStatus result =
      chromeos::CrosSettings::Get()->PrepareTrustedValues(
          base::Bind(&BrowserPolicyConnector::SetTimezoneIfPolicyAvailable,
                     weak_ptr_factory_.GetWeakPtr()));

  if (result != Provider::TRUSTED)
    return;

  std::string timezone;
  if (chromeos::CrosSettings::Get()->GetString(
          chromeos::kSystemTimezonePolicy, &timezone)) {
    chromeos::system::TimezoneSettings::GetInstance()->SetTimezoneFromID(
        UTF8ToUTF16(timezone));
  }
#endif
}

scoped_ptr<PolicyService>
    BrowserPolicyConnector::CreatePolicyServiceWithProviders(
        ConfigurationPolicyProvider* user_cloud_policy_provider,
        ConfigurationPolicyProvider* managed_mode_policy_provider) {
  PolicyServiceImpl::Providers providers;
  if (g_testing_provider) {
    providers.push_back(g_testing_provider);
  } else {
    // |providers| in decreasing order of priority.
    if (platform_provider_)
      providers.push_back(platform_provider_.get());
    if (cloud_provider_)
      providers.push_back(cloud_provider_.get());

#if defined(OS_CHROMEOS)
    if (device_cloud_policy_manager_.get())
      providers.push_back(device_cloud_policy_manager_.get());
    if (!user_cloud_policy_provider)
      user_cloud_policy_provider = &global_user_cloud_policy_provider_;
#endif

    if (user_cloud_policy_provider)
      providers.push_back(user_cloud_policy_provider);
    if (managed_mode_policy_provider)
      providers.push_back(managed_mode_policy_provider);
  }

  return scoped_ptr<PolicyService>(new PolicyServiceImpl(providers));
}

// static
ConfigurationPolicyProvider* BrowserPolicyConnector::CreatePlatformProvider() {
#if defined(OS_WIN)
  const PolicyDefinitionList* policy_list = GetChromePolicyDefinitionList();
  scoped_ptr<AsyncPolicyLoader> loader(new PolicyLoaderWin(policy_list));
  return new AsyncPolicyProvider(loader.Pass());
#elif defined(OS_MACOSX)
  const PolicyDefinitionList* policy_list = GetChromePolicyDefinitionList();
  scoped_ptr<AsyncPolicyLoader> loader(
      new PolicyLoaderMac(policy_list, new MacPreferences()));
  return new AsyncPolicyProvider(loader.Pass());
#elif defined(OS_POSIX)
  FilePath config_dir_path;
  if (PathService::Get(chrome::DIR_POLICY_FILES, &config_dir_path)) {
    scoped_ptr<AsyncPolicyLoader> loader(
        new ConfigDirPolicyLoader(config_dir_path, POLICY_SCOPE_MACHINE));
    return new AsyncPolicyProvider(loader.Pass());
  } else {
    return NULL;
  }
#else
  return NULL;
#endif
}

}  // namespace policy
