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
#include "chrome/browser/policy/cloud_policy_service.h"
#include "chrome/browser/policy/configuration_policy_provider.h"
#include "chrome/browser/policy/device_management_service.h"
#include "chrome/browser/policy/managed_mode_policy_provider.h"
#include "chrome/browser/policy/policy_service_impl.h"
#include "chrome/browser/policy/policy_statistics_collector.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/pref_service_simple.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"
#include "grit/generated_resources.h"
#include "policy/policy_constants.h"
#include "third_party/icu/public/i18n/unicode/regex.h"

#if defined(OS_WIN)
#include "chrome/browser/policy/policy_loader_win.h"
#elif defined(OS_MACOSX)
#include "chrome/browser/policy/policy_loader_mac.h"
#include "chrome/browser/policy/preferences_mac.h"
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
#include "chrome/browser/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/policy/device_cloud_policy_store_chromeos.h"
#include "chrome/browser/policy/device_local_account_policy_provider.h"
#include "chrome/browser/policy/device_local_account_policy_service.h"
#include "chrome/browser/policy/device_status_collector.h"
#include "chrome/browser/policy/enterprise_install_attributes.h"
#include "chrome/browser/policy/network_configuration_updater.h"
#include "chrome/browser/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/policy/user_cloud_policy_store_chromeos.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#else
#include "chrome/browser/policy/user_cloud_policy_manager.h"
#include "chrome/browser/policy/user_cloud_policy_manager_factory.h"
#endif

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

// Default policy refresh rate.
const int64 kDefaultPolicyRefreshRateMs = 3 * 60 * 60 * 1000;  // 3 hours.

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
  install_attributes_->ReadCacheFile(
      FilePath(policy::EnterpriseInstallAttributes::kCacheFilePath));

  scoped_ptr<DeviceCloudPolicyStoreChromeOS> device_cloud_policy_store(
      new DeviceCloudPolicyStoreChromeOS(
          chromeos::DeviceSettingsService::Get(),
          install_attributes_.get()));
  device_cloud_policy_manager_.reset(
      new DeviceCloudPolicyManagerChromeOS(
          device_cloud_policy_store.Pass(),
          install_attributes_.get()));

  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kDisableLocalAccounts)) {
    device_local_account_policy_service_.reset(
        new DeviceLocalAccountPolicyService(
            chromeos::DBusThreadManager::Get()->GetSessionManagerClient(),
            chromeos::DeviceSettingsService::Get()));
  }
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

#if defined(OS_CHROMEOS)
  // The AppPackUpdater may be observing the |device_cloud_policy_subsystem_|.
  // Delete it first.
  app_pack_updater_.reset();

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

#if defined(OS_CHROMEOS)
bool BrowserPolicyConnector::IsEnterpriseManaged() {
  return install_attributes_.get() && install_attributes_->IsEnterpriseDevice();
}

std::string BrowserPolicyConnector::GetEnterpriseDomain() {
  return install_attributes_.get() ? install_attributes_->GetDomain()
                                   : std::string();
}

DeviceMode BrowserPolicyConnector::GetDeviceMode() {
  return install_attributes_.get() ? install_attributes_->GetMode()
                                   : DEVICE_MODE_NOT_SET;
}
#endif

void BrowserPolicyConnector::ScheduleServiceInitialization(
    int64 delay_milliseconds) {
  device_management_service_->ScheduleInitialization(delay_milliseconds);
}

#if defined(OS_CHROMEOS)
void BrowserPolicyConnector::InitializeUserPolicy(
    const std::string& user_name,
    bool is_public_account,
    bool wait_for_policy_fetch) {
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

  CommandLine* command_line = CommandLine::ForCurrentProcess();

  int64 startup_delay =
      wait_for_policy_fetch ? 0 : kServiceInitializationStartupDelay;

  FilePath profile_dir;
  PathService::Get(chrome::DIR_USER_DATA, &profile_dir);
  profile_dir = profile_dir.Append(
      command_line->GetSwitchValuePath(switches::kLoginProfile));
  const FilePath policy_dir = profile_dir.Append(kPolicyDir);
  const FilePath policy_cache_file = policy_dir.Append(kPolicyCacheFile);
  const FilePath token_cache_file = policy_dir.Append(kTokenCacheFile);

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
}
#endif

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

#if defined(OS_CHROMEOS)
AppPackUpdater* BrowserPolicyConnector::GetAppPackUpdater() {
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
}
#endif

#if defined(OS_CHROMEOS)
NetworkConfigurationUpdater*
    BrowserPolicyConnector::GetNetworkConfigurationUpdater() {
  if (!network_configuration_updater_.get()) {
    network_configuration_updater_.reset(new NetworkConfigurationUpdater(
        g_browser_process->policy_service(),
        chromeos::CrosLibrary::Get()->GetNetworkLibrary()));
  }
  return network_configuration_updater_.get();
}
#endif

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

// static
void BrowserPolicyConnector::RegisterPrefs(PrefServiceSimple* local_state) {
  local_state->RegisterIntegerPref(prefs::kUserPolicyRefreshRate,
                                   kDefaultPolicyRefreshRateMs);
#if defined(OS_CHROMEOS)
  local_state->RegisterIntegerPref(prefs::kDevicePolicyRefreshRate,
                                   kDefaultPolicyRefreshRateMs);
#endif
}

void BrowserPolicyConnector::CompleteInitialization() {
  if (g_testing_provider)
    g_testing_provider->Init();
  if (platform_provider_)
    platform_provider_->Init();

#if defined(OS_CHROMEOS)
  global_user_cloud_policy_provider_.Init();

  // Create the AppPackUpdater to start updating the cache. It requires the
  // system request context, which isn't available in Init(); therefore it is
  // created only once the loops are running.
  GetAppPackUpdater();

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
