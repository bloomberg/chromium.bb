// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/browser_policy_connector.h"

#include <algorithm>
#include <iterator>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/path_service.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/async_policy_provider.h"
#include "chrome/browser/policy/cloud/cloud_policy_client.h"
#include "chrome/browser/policy/cloud/cloud_policy_refresh_scheduler.h"
#include "chrome/browser/policy/cloud/cloud_policy_service.h"
#include "chrome/browser/policy/cloud/device_management_service.h"
#include "chrome/browser/policy/configuration_policy_provider.h"
#include "chrome/browser/policy/policy_service_impl.h"
#include "chrome/browser/policy/policy_statistics_collector.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/schema.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_client.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"
#include "grit/generated_resources.h"
#include "net/url_request/url_request_context_getter.h"
#include "policy/policy_constants.h"
#include "third_party/icu/source/i18n/unicode/regex.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "chrome/browser/policy/policy_loader_win.h"
#elif defined(OS_MACOSX) && !defined(OS_IOS)
#include <CoreFoundation/CoreFoundation.h>
#include "chrome/browser/policy/policy_loader_mac.h"
#include "chrome/browser/policy/preferences_mac.h"
#elif defined(OS_POSIX) && !defined(OS_ANDROID)
#include "chrome/browser/policy/config_dir_policy_loader.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/policy/app_pack_updater.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_store_chromeos.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/policy/device_local_account_policy_service.h"
#include "chrome/browser/chromeos/policy/device_status_collector.h"
#include "chrome/browser/chromeos/policy/enterprise_install_attributes.h"
#include "chrome/browser/chromeos/policy/network_configuration_updater.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chromeos/chromeos_paths.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/onc/onc_certificate_importer_impl.h"
#include "chromeos/settings/cros_settings_provider.h"
#include "chromeos/settings/timezone_settings.h"
#include "chromeos/system/statistics_provider.h"
#endif

using content::BrowserThread;

namespace policy {

namespace {

// The following constants define delays applied before the initial policy fetch
// on startup. (So that displaying Chrome's GUI does not get delayed.)
// Delay in milliseconds from startup.
const int64 kServiceInitializationStartupDelay = 5000;

// The URL for the device management server.
const char kDefaultDeviceManagementServerUrl[] =
    "https://m.google.com/devicemanagement/data/api";

#if defined(OS_CHROMEOS)
// Install attributes for tests.
EnterpriseInstallAttributes* g_testing_install_attributes = NULL;
#endif  // defined(OS_CHROMEOS)

// Used in BrowserPolicyConnector::SetPolicyProviderForTesting.
ConfigurationPolicyProvider* g_testing_provider = NULL;

#if defined(OS_CHROMEOS)
// Helper that returns a new SequencedTaskRunner backed by the blocking pool.
// Each SequencedTaskRunner returned is independent from the others.
scoped_refptr<base::SequencedTaskRunner> GetBackgroundTaskRunner() {
  base::SequencedWorkerPool* pool = BrowserThread::GetBlockingPool();
  CHECK(pool);
  return pool->GetSequencedTaskRunnerWithShutdownBehavior(
      pool->GetSequenceToken(), base::SequencedWorkerPool::SKIP_ON_SHUTDOWN);
}
#endif  // defined(OS_CHROMEOS)

#if defined(OS_MACOSX) && !defined(OS_IOS)
base::FilePath GetManagedPolicyPath() {
  // This constructs the path to the plist file in which Mac OS X stores the
  // managed preference for the application. This is undocumented and therefore
  // fragile, but if it doesn't work out, AsyncPolicyLoader has a task that
  // polls periodically in order to reload managed preferences later even if we
  // missed the change.
  base::FilePath path;
  if (!PathService::Get(chrome::DIR_MANAGED_PREFS, &path))
    return base::FilePath();

  CFBundleRef bundle(CFBundleGetMainBundle());
  if (!bundle)
    return base::FilePath();

  CFStringRef bundle_id = CFBundleGetIdentifier(bundle);
  if (!bundle_id)
    return base::FilePath();

  return path.Append(base::SysCFStringRefToUTF8(bundle_id) + ".plist");
}
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)

class DeviceManagementServiceConfiguration
    : public DeviceManagementService::Configuration {
 public:
  DeviceManagementServiceConfiguration() {}
  virtual ~DeviceManagementServiceConfiguration() {}

  virtual std::string GetServerUrl() OVERRIDE {
    CommandLine* command_line = CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(switches::kDeviceManagementUrl))
      return command_line->GetSwitchValueASCII(switches::kDeviceManagementUrl);
    else
      return kDefaultDeviceManagementServerUrl;
  }

  virtual std::string GetUserAgent() OVERRIDE {
    return content::GetUserAgent(GURL(GetServerUrl()));
  }

  virtual std::string GetAgentParameter() OVERRIDE {
    chrome::VersionInfo version_info;
    return base::StringPrintf("%s %s(%s)",
                              version_info.Name().c_str(),
                              version_info.Version().c_str(),
                              version_info.LastChange().c_str());
  }

  virtual std::string GetPlatformParameter() OVERRIDE {
    std::string os_name = base::SysInfo::OperatingSystemName();
    std::string os_hardware = base::SysInfo::OperatingSystemArchitecture();

#if defined(OS_CHROMEOS)
    chromeos::system::StatisticsProvider* provider =
        chromeos::system::StatisticsProvider::GetInstance();

    std::string hwclass;
    if (!provider->GetMachineStatistic(chromeos::system::kHardwareClassKey,
                                       &hwclass)) {
      LOG(ERROR) << "Failed to get machine information";
    }
    os_name += ",CrOS," + base::SysInfo::GetLsbReleaseBoard();
    os_hardware += "," + hwclass;
#endif

    std::string os_version("-");
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
    int32 os_major_version = 0;
    int32 os_minor_version = 0;
    int32 os_bugfix_version = 0;
    base::SysInfo::OperatingSystemVersionNumbers(&os_major_version,
                                                 &os_minor_version,
                                                 &os_bugfix_version);
    os_version = base::StringPrintf("%d.%d.%d",
                                    os_major_version,
                                    os_minor_version,
                                    os_bugfix_version);
#endif

    return base::StringPrintf(
        "%s|%s|%s", os_name.c_str(), os_hardware.c_str(), os_version.c_str());
  }
};

}  // namespace

BrowserPolicyConnector::BrowserPolicyConnector()
    : is_initialized_(false),
      local_state_(NULL),
      handler_list_(BuildHandlerList().Pass()),
      weak_ptr_factory_(this) {
  // GetPolicyService() must be ready after the constructor is done.
  // The connector is created very early during startup, when the browser
  // threads aren't running yet; initialize components that need local_state,
  // the system request context or other threads (e.g. FILE) at Init().

  // Initialize the SchemaRegistry with the Chrome schema before creating any
  // of the policy providers.
  chrome_schema_ = Schema::Wrap(GetChromeSchemaData());
  schema_registry_.RegisterComponent(PolicyNamespace(POLICY_DOMAIN_CHROME, ""),
                                     chrome_schema_);

  platform_provider_.reset(CreatePlatformProvider());

#if defined(OS_CHROMEOS)
  if (g_testing_install_attributes)
    install_attributes_.reset(g_testing_install_attributes);

  // SystemSaltGetter or DBusThreadManager may be uninitialized on unit tests.

  // TODO(satorux): Remove SystemSaltGetter::IsInitialized() when it's ready
  // (removing it now breaks tests). crbug.com/141016.
  if (chromeos::SystemSaltGetter::IsInitialized() &&
      chromeos::DBusThreadManager::IsInitialized()) {
    chromeos::CryptohomeClient* cryptohome_client =
        chromeos::DBusThreadManager::Get()->GetCryptohomeClient();
    if (!g_testing_install_attributes) {
      install_attributes_.reset(
          new EnterpriseInstallAttributes(cryptohome_client));
    }
    base::FilePath install_attrs_file;
    CHECK(PathService::Get(chromeos::FILE_INSTALL_ATTRIBUTES,
                           &install_attrs_file));
    install_attributes_->ReadCacheFile(install_attrs_file);

    scoped_ptr<DeviceCloudPolicyStoreChromeOS> device_cloud_policy_store(
        new DeviceCloudPolicyStoreChromeOS(
            chromeos::DeviceSettingsService::Get(),
            install_attributes_.get(),
            GetBackgroundTaskRunner()));
    device_cloud_policy_manager_.reset(
        new DeviceCloudPolicyManagerChromeOS(
            device_cloud_policy_store.Pass(),
            base::MessageLoopProxy::current(),
            GetBackgroundTaskRunner(),
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
  DCHECK(BrowserThread::IsThreadInitialized(BrowserThread::FILE));
  DCHECK(!is_initialized()) << "BrowserPolicyConnector::Init() called twice.";

  local_state_ = local_state;
  request_context_ = request_context;

  scoped_ptr<DeviceManagementService::Configuration> configuration(
      new DeviceManagementServiceConfiguration);
  device_management_service_.reset(
      new DeviceManagementService(configuration.Pass(), request_context));
  device_management_service_->ScheduleInitialization(
      kServiceInitializationStartupDelay);

  if (g_testing_provider)
    g_testing_provider->Init(GetSchemaRegistry());
  if (platform_provider_)
    platform_provider_->Init(GetSchemaRegistry());

#if defined(OS_CHROMEOS)
  global_user_cloud_policy_provider_.Init(GetSchemaRegistry());

  if (device_cloud_policy_manager_) {
    // For now the |device_cloud_policy_manager_| is using the global schema
    // registry. Eventually it will have its own registry, once device cloud
    // policy for extensions is introduced.
    device_cloud_policy_manager_->Init(GetSchemaRegistry());
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
  if (!command_line->HasSwitch(chromeos::switches::kDisableLocalAccounts)) {
    device_local_account_policy_service_.reset(
        new DeviceLocalAccountPolicyService(
            chromeos::DBusThreadManager::Get()->GetSessionManagerClient(),
            chromeos::DeviceSettingsService::Get(),
            chromeos::CrosSettings::Get(),
            GetBackgroundTaskRunner(),
            GetBackgroundTaskRunner(),
            GetBackgroundTaskRunner(),
            content::BrowserThread::GetMessageLoopProxyForThread(
                content::BrowserThread::IO),
            request_context));
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
          base::MessageLoop::current()->message_loop_proxy()));
  policy_statistics_collector_->Initialize();

#if defined(OS_CHROMEOS)
  network_configuration_updater_ =
      NetworkConfigurationUpdater::CreateForDevicePolicy(
          scoped_ptr<chromeos::onc::CertificateImporter>(
              new chromeos::onc::CertificateImporterImpl),
          GetPolicyService(),
          chromeos::NetworkHandler::Get()
              ->managed_network_configuration_handler());
#endif

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

  network_configuration_updater_.reset();

  if (device_cloud_policy_manager_)
    device_cloud_policy_manager_->Shutdown();
  if (device_local_account_policy_service_)
    device_local_account_policy_service_->Shutdown();
  global_user_cloud_policy_provider_.Shutdown();
#endif

  device_management_service_.reset();

  request_context_ = NULL;
}

PolicyService* BrowserPolicyConnector::GetPolicyService() {
  if (!policy_service_) {
    std::vector<ConfigurationPolicyProvider*> providers;
#if defined(OS_CHROMEOS)
    providers.push_back(&global_user_cloud_policy_provider_);
#endif
    policy_service_ = CreatePolicyService(providers);
  }
  return policy_service_.get();
}

const Schema& BrowserPolicyConnector::GetChromeSchema() const {
  return chrome_schema_;
}

CombinedSchemaRegistry* BrowserPolicyConnector::GetSchemaRegistry() {
  return &schema_registry_;
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

scoped_ptr<PolicyService> BrowserPolicyConnector::CreatePolicyService(
    const std::vector<ConfigurationPolicyProvider*>& additional_providers) {
  std::vector<ConfigurationPolicyProvider*> providers;
  if (g_testing_provider) {
    providers.push_back(g_testing_provider);
  } else {
    // |providers| in decreasing order of priority.
    if (platform_provider_)
      providers.push_back(platform_provider_.get());
#if defined(OS_CHROMEOS)
    if (device_cloud_policy_manager_)
      providers.push_back(device_cloud_policy_manager_.get());
#endif
    std::copy(additional_providers.begin(), additional_providers.end(),
              std::back_inserter(providers));
  }
  return scoped_ptr<PolicyService>(new PolicyServiceImpl(providers));
}

const ConfigurationPolicyHandlerList*
    BrowserPolicyConnector::GetHandlerList() const {
  return handler_list_.get();
}

UserAffiliation BrowserPolicyConnector::GetUserAffiliation(
    const std::string& user_name) {
#if defined(OS_CHROMEOS)
  // An empty username means incognito user in case of ChromiumOS and
  // no logged-in user in case of Chromium (SigninService). Many tests use
  // nonsense email addresses (e.g. 'test') so treat those as non-enterprise
  // users.
  if (user_name.empty() || user_name.find('@') == std::string::npos)
    return USER_AFFILIATION_NONE;
  if (install_attributes_ &&
      (gaia::ExtractDomainName(gaia::CanonicalizeEmail(user_name)) ==
           install_attributes_->GetDomain() ||
       policy::IsDeviceLocalAccountUser(user_name, NULL))) {
    return USER_AFFILIATION_MANAGED;
  }
#endif

  return USER_AFFILIATION_NONE;
}

#if defined(OS_CHROMEOS)
AppPackUpdater* BrowserPolicyConnector::GetAppPackUpdater() {
  // request_context_ is NULL in unit tests.
  if (!app_pack_updater_ && request_context_.get()) {
    app_pack_updater_.reset(
        new AppPackUpdater(request_context_.get(), install_attributes_.get()));
  }
  return app_pack_updater_.get();
}

void BrowserPolicyConnector::SetUserPolicyDelegate(
    ConfigurationPolicyProvider* user_policy_provider) {
  global_user_cloud_policy_provider_.SetDelegate(user_policy_provider);
}

void BrowserPolicyConnector::SetInstallAttributesForTesting(
    EnterpriseInstallAttributes* attributes) {
  DCHECK(!g_testing_install_attributes);
  g_testing_install_attributes = attributes;
}
#endif

// static
void BrowserPolicyConnector::SetPolicyProviderForTesting(
    ConfigurationPolicyProvider* provider) {
  CHECK(!g_browser_process) << "Must be invoked before the browser is created";
  DCHECK(!g_testing_provider);
  g_testing_provider = provider;
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
  registry->RegisterIntegerPref(
      policy_prefs::kUserPolicyRefreshRate,
      CloudPolicyRefreshScheduler::kDefaultRefreshDelayMs);
#if defined(OS_CHROMEOS)
  registry->RegisterIntegerPref(
      prefs::kDevicePolicyRefreshRate,
      CloudPolicyRefreshScheduler::kDefaultRefreshDelayMs);
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

ConfigurationPolicyProvider* BrowserPolicyConnector::CreatePlatformProvider() {
#if defined(OS_WIN)
  const PolicyDefinitionList* policy_list = GetChromePolicyDefinitionList();
  scoped_ptr<AsyncPolicyLoader> loader(PolicyLoaderWin::Create(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE),
      policy_list));
  return new AsyncPolicyProvider(GetSchemaRegistry(), loader.Pass());
#elif defined(OS_MACOSX) && !defined(OS_IOS)
  const PolicyDefinitionList* policy_list = GetChromePolicyDefinitionList();
  scoped_ptr<AsyncPolicyLoader> loader(new PolicyLoaderMac(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE),
      policy_list,
      GetManagedPolicyPath(),
      new MacPreferences()));
  return new AsyncPolicyProvider(GetSchemaRegistry(), loader.Pass());
#elif defined(OS_POSIX) && !defined(OS_ANDROID)
  base::FilePath config_dir_path;
  if (PathService::Get(chrome::DIR_POLICY_FILES, &config_dir_path)) {
    scoped_ptr<AsyncPolicyLoader> loader(new ConfigDirPolicyLoader(
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE),
        config_dir_path,
        POLICY_SCOPE_MACHINE));
    return new AsyncPolicyProvider(GetSchemaRegistry(), loader.Pass());
  } else {
    return NULL;
  }
#else
  return NULL;
#endif
}

}  // namespace policy
