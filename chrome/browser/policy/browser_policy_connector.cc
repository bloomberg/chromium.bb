// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/browser_policy_connector.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/async_policy_provider.h"
#include "chrome/browser/policy/cloud/cloud_policy_client.h"
#include "chrome/browser/policy/cloud/cloud_policy_service.h"
#include "chrome/browser/policy/cloud/device_management_service.h"
#include "chrome/browser/policy/configuration_policy_provider.h"
#include "chrome/browser/policy/managed_mode_policy_provider.h"
#include "chrome/browser/policy/policy_service_impl.h"
#include "chrome/browser/policy/policy_statistics_collector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"
#include "grit/generated_resources.h"
#include "net/url_request/url_request_context_getter.h"
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
#include "chrome/browser/chromeos/policy/app_pack_updater.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_store_chromeos.h"
#include "chrome/browser/chromeos/policy/device_local_account_policy_provider.h"
#include "chrome/browser/chromeos/policy/device_local_account_policy_service.h"
#include "chrome/browser/chromeos/policy/device_status_collector.h"
#include "chrome/browser/chromeos/policy/enterprise_install_attributes.h"
#include "chrome/browser/chromeos/policy/network_configuration_updater.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_store_chromeos.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/cros_settings_provider.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/chromeos/system/statistics_provider.h"
#include "chrome/browser/chromeos/system/timezone_settings.h"
#include "chrome/browser/policy/cloud/resource_cache.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#else
#include "chrome/browser/policy/cloud/user_cloud_policy_manager.h"
#include "chrome/browser/policy/cloud/user_cloud_policy_manager_factory.h"
#endif

using content::BrowserThread;

namespace policy {

namespace {

// Subdirectory in the user's profile for storing legacy user policies.
const base::FilePath::CharType kDeviceManagementDir[] =
    FILE_PATH_LITERAL("Device Management");
// File in the above directory for storing legacy user policy dmtokens.
const base::FilePath::CharType kToken[] = FILE_PATH_LITERAL("Token");
// This constant is used to build two different paths. It can be a file inside
// kDeviceManagementDir where legacy user policy data is stored, and it can be
// a directory inside the profile directory where other resources are stored.
const base::FilePath::CharType kPolicy[] = FILE_PATH_LITERAL("Policy");
// Directory under kPolicy, in the user's profile dir, where external policy
// resources are stored.
const base::FilePath::CharType kResourceDir[] = FILE_PATH_LITERAL("Resources");

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
      local_state_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  // GetPolicyService() must be ready after the constructor is done.
  // The connector is created very early during startup, when the browser
  // threads aren't running yet; initialize components that need local_state,
  // the system request context or other threads (e.g. FILE) at Init().

  platform_provider_.reset(CreatePlatformProvider());

  device_management_service_.reset(
      new DeviceManagementService(GetDeviceManagementUrl()));

#if defined(OS_CHROMEOS)
  chromeos::CrosLibrary* cros_library = chromeos::CrosLibrary::Get();
  // |cros_library| may be NULL on unit tests. DBusThreadManager may not be
  // initialized on unit tests..
  if (cros_library && chromeos::DBusThreadManager::IsInitialized()) {
    chromeos::CryptohomeLibrary* cryptohome =
        cros_library->GetCryptohomeLibrary();
    chromeos::CryptohomeClient* cryptohome_client =
        chromeos::DBusThreadManager::Get()->GetCryptohomeClient();
    install_attributes_.reset(
        new EnterpriseInstallAttributes(cryptohome, cryptohome_client));
    base::FilePath install_attrs_file;
    CHECK(PathService::Get(chrome::FILE_INSTALL_ATTRIBUTES,
                           &install_attrs_file));
    install_attributes_->ReadCacheFile(install_attrs_file);

    scoped_ptr<DeviceCloudPolicyStoreChromeOS> device_cloud_policy_store(
        new DeviceCloudPolicyStoreChromeOS(
            chromeos::DeviceSettingsService::Get(),
            install_attributes_.get()));
    device_cloud_policy_manager_.reset(
        new DeviceCloudPolicyManagerChromeOS(
            device_cloud_policy_store.Pass(),
            install_attributes_.get()));
  }
#endif
}

BrowserPolicyConnector::~BrowserPolicyConnector() {
  if (is_initialized()) {
    // Shutdown() wasn't invoked by our owner after having called Init().
    // This usually means it's an early shutdown and
    // BrowserProcessImpl::StartTearDown() wasn't invoked.
    // Cleanup properly in those cases and avoid crashing the ToastCrasher test.
    Shutdown();
  }
}

void BrowserPolicyConnector::Init(
    PrefService* local_state,
    scoped_refptr<net::URLRequestContextGetter> request_context) {
  // Initialization of some of the providers requires the FILE thread; make
  // sure that threading is ready at this point.
  DCHECK(BrowserThread::IsWellKnownThread(BrowserThread::FILE));
  DCHECK(!is_initialized()) << "BrowserPolicyConnector::Init() called twice.";

  local_state_ = local_state;
  request_context_ = request_context;

  device_management_service_->ScheduleInitialization(
      kServiceInitializationStartupDelay);

  if (g_testing_provider)
    g_testing_provider->Init();
  if (platform_provider_)
    platform_provider_->Init();

#if defined(OS_CHROMEOS)
  global_user_cloud_policy_provider_.Init();

  if (device_cloud_policy_manager_) {
    device_cloud_policy_manager_->Init();
    scoped_ptr<CloudPolicyClient::StatusProvider> status_provider(
        new DeviceStatusCollector(
            local_state_,
            chromeos::system::StatisticsProvider::GetInstance(),
            NULL));
    device_cloud_policy_manager_->Connect(
        local_state_,
        device_management_service_.get(),
        status_provider.Pass());
  }

  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kDisableLocalAccounts)) {
    device_local_account_policy_service_.reset(
        new DeviceLocalAccountPolicyService(
            chromeos::DBusThreadManager::Get()->GetSessionManagerClient(),
            chromeos::DeviceSettingsService::Get()));
    device_local_account_policy_service_->Connect(
        device_management_service_.get());
  }

  GetAppPackUpdater();

  SetTimezoneIfPolicyAvailable();
#endif

  policy_statistics_collector_.reset(
      new policy::PolicyStatisticsCollector(
          GetPolicyService(),
          local_state_,
          MessageLoop::current()->message_loop_proxy()));
  policy_statistics_collector_->Initialize();

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

  request_context_ = NULL;
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
  return install_attributes_ && install_attributes_->IsEnterpriseDevice();
}

std::string BrowserPolicyConnector::GetEnterpriseDomain() {
  return install_attributes_ ? install_attributes_->GetDomain() : std::string();
}

DeviceMode BrowserPolicyConnector::GetDeviceMode() {
  return install_attributes_ ? install_attributes_->GetMode()
                             : DEVICE_MODE_NOT_SET;
}
#endif

void BrowserPolicyConnector::ScheduleServiceInitialization(
    int64 delay_milliseconds) {
  // Skip device initialization if the BrowserPolicyConnector was never
  // initialized (unit tests).
  if (device_management_service_)
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
  CHECK(!user_cloud_policy_manager_);

  CommandLine* command_line = CommandLine::ForCurrentProcess();

  base::FilePath profile_dir;
  CHECK(PathService::Get(chrome::DIR_USER_DATA, &profile_dir));
  profile_dir = profile_dir.Append(
      command_line->GetSwitchValuePath(switches::kLoginProfile));
  const base::FilePath legacy_dir = profile_dir.Append(kDeviceManagementDir);
  const base::FilePath policy_cache_file = legacy_dir.Append(kPolicy);
  const base::FilePath token_cache_file = legacy_dir.Append(kToken);
  const base::FilePath resource_cache_dir =
      profile_dir.Append(kPolicy).Append(kResourceDir);
  base::FilePath policy_key_dir;
  CHECK(PathService::Get(chrome::DIR_USER_POLICY_KEYS, &policy_key_dir));

  if (wait_for_policy_fetch)
    device_management_service_->ScheduleInitialization(0);
  if (is_public_account && device_local_account_policy_service_) {
    device_local_account_policy_provider_.reset(
        new DeviceLocalAccountPolicyProvider(
            user_name, device_local_account_policy_service_.get()));

    device_local_account_policy_provider_->Init();
    global_user_cloud_policy_provider_.SetDelegate(
        device_local_account_policy_provider_.get());
  } else if (!IsNonEnterpriseUser(user_name)) {
    scoped_ptr<CloudPolicyStore> store(
        new UserCloudPolicyStoreChromeOS(
            chromeos::DBusThreadManager::Get()->GetCryptohomeClient(),
            chromeos::DBusThreadManager::Get()->GetSessionManagerClient(),
            user_name, policy_key_dir, token_cache_file, policy_cache_file));
    scoped_ptr<ResourceCache> resource_cache;
    if (command_line->HasSwitch(switches::kEnableComponentCloudPolicy))
      resource_cache.reset(new ResourceCache(resource_cache_dir));
    user_cloud_policy_manager_.reset(
        new UserCloudPolicyManagerChromeOS(store.Pass(),
                                           resource_cache.Pass(),
                                           wait_for_policy_fetch));

    user_cloud_policy_manager_->Init();
    user_cloud_policy_manager_->Connect(local_state_,
                                        device_management_service_.get(),
                                        request_context_,
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
  if (install_attributes_ &&
      gaia::ExtractDomainName(gaia::CanonicalizeEmail(user_name)) ==
          install_attributes_->GetDomain()) {
    return USER_AFFILIATION_MANAGED;
  }
#endif

  return USER_AFFILIATION_NONE;
}

#if defined(OS_CHROMEOS)
AppPackUpdater* BrowserPolicyConnector::GetAppPackUpdater() {
  // request_context_ is NULL in unit tests.
  if (!app_pack_updater_ && request_context_) {
    app_pack_updater_.reset(
        new AppPackUpdater(request_context_, install_attributes_.get()));
  }
  return app_pack_updater_.get();
}
#endif

#if defined(OS_CHROMEOS)
NetworkConfigurationUpdater*
    BrowserPolicyConnector::GetNetworkConfigurationUpdater() {
  if (!network_configuration_updater_) {
    network_configuration_updater_.reset(new NetworkConfigurationUpdater(
        GetPolicyService(),
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
  if (username.empty() || username.find('@') == std::string::npos) {
    // An empty username means incognito user in case of ChromiumOS and
    // no logged-in user in case of Chromium (SigninService). Many tests use
    // nonsense email addresses (e.g. 'test') so treat those as non-enterprise
    // users.
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
void BrowserPolicyConnector::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kUserPolicyRefreshRate,
                                kDefaultPolicyRefreshRateMs);
#if defined(OS_CHROMEOS)
  registry->RegisterIntegerPref(prefs::kDevicePolicyRefreshRate,
                                kDefaultPolicyRefreshRateMs);
#endif
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
  if (chromeos::CrosSettings::Get()->GetString(chromeos::kSystemTimezonePolicy,
                                               &timezone) &&
      !timezone.empty()) {
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
    if (device_cloud_policy_manager_)
      providers.push_back(device_cloud_policy_manager_.get());
    if (!user_cloud_policy_provider)
      user_cloud_policy_provider = &global_user_cloud_policy_provider_;
#endif

    if (user_cloud_policy_provider)
      providers.push_back(user_cloud_policy_provider);
    if (managed_mode_policy_provider)
      providers.push_back(managed_mode_policy_provider);
  }

  scoped_ptr<PolicyService> service(new PolicyServiceImpl(providers));
  service->RegisterPolicyDomain(POLICY_DOMAIN_CHROME, std::set<std::string>());
  return service.Pass();
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
  base::FilePath config_dir_path;
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
