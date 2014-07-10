// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/browser_policy_connector.h"

#include <algorithm>
#include <vector>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/policy/core/common/cloud/cloud_policy_refresh_scheduler.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "components/policy/core/common/policy_statistics_collector.h"
#include "components/policy/core/common/policy_switches.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "net/url_request/url_request_context_getter.h"
#include "policy/policy_constants.h"
#include "third_party/icu/source/i18n/unicode/regex.h"

namespace policy {

namespace {

// The URL for the device management server.
const char kDefaultDeviceManagementServerUrl[] =
    "https://m.google.com/devicemanagement/data/api";

// Used in BrowserPolicyConnector::SetPolicyProviderForTesting.
bool g_created_policy_service = false;
ConfigurationPolicyProvider* g_testing_provider = NULL;

// Returns true if |domain| matches the regex |pattern|.
bool MatchDomain(const base::string16& domain, const base::string16& pattern) {
  UErrorCode status = U_ZERO_ERROR;
  const icu::UnicodeString icu_pattern(pattern.data(), pattern.length());
  icu::RegexMatcher matcher(icu_pattern, UREGEX_CASE_INSENSITIVE, status);
  if (!U_SUCCESS(status)) {
    // http://crbug.com/365351 - if for some reason the matcher creation fails
    // just return that the pattern doesn't match the domain. This is safe
    // because the calling method (IsNonEnterpriseUser()) is just used to enable
    // an optimization for non-enterprise users - better to skip the
    // optimization than crash.
    DLOG(ERROR) << "Possible invalid domain pattern: " << pattern
                << " - Error: " << status;
    return false;
  }
  icu::UnicodeString icu_input(domain.data(), domain.length());
  matcher.reset(icu_input);
  status = U_ZERO_ERROR;
  UBool match = matcher.matches(status);
  DCHECK(U_SUCCESS(status));
  return !!match;  // !! == convert from UBool to bool.
}

}  // namespace

BrowserPolicyConnector::BrowserPolicyConnector(
    const HandlerListFactory& handler_list_factory)
    : is_initialized_(false),
      platform_policy_provider_(NULL) {
  // GetPolicyService() must be ready after the constructor is done.
  // The connector is created very early during startup, when the browser
  // threads aren't running yet; initialize components that need local_state,
  // the system request context or other threads (e.g. FILE) at Init().

  // Initialize the SchemaRegistry with the Chrome schema before creating any
  // of the policy providers in subclasses.
  chrome_schema_ = Schema::Wrap(GetChromeSchemaData());
  handler_list_ = handler_list_factory.Run(chrome_schema_);
  schema_registry_.RegisterComponent(PolicyNamespace(POLICY_DOMAIN_CHROME, ""),
                                     chrome_schema_);
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
    scoped_refptr<net::URLRequestContextGetter> request_context,
    scoped_ptr<DeviceManagementService> device_management_service) {
  DCHECK(!is_initialized());

  device_management_service_ = device_management_service.Pass();

  if (g_testing_provider)
    g_testing_provider->Init(GetSchemaRegistry());
  for (size_t i = 0; i < policy_providers_.size(); ++i)
    policy_providers_[i]->Init(GetSchemaRegistry());

  policy_statistics_collector_.reset(
      new policy::PolicyStatisticsCollector(
          base::Bind(&GetChromePolicyDetails),
          GetChromeSchema(),
          GetPolicyService(),
          local_state,
          base::MessageLoop::current()->message_loop_proxy()));
  policy_statistics_collector_->Initialize();

  is_initialized_ = true;
}

void BrowserPolicyConnector::Shutdown() {
  is_initialized_ = false;
  if (g_testing_provider)
    g_testing_provider->Shutdown();
  for (size_t i = 0; i < policy_providers_.size(); ++i)
    policy_providers_[i]->Shutdown();
  // Drop g_testing_provider so that tests executed with --single_process can
  // call SetPolicyProviderForTesting() again. It is still owned by the test.
  g_testing_provider = NULL;
  g_created_policy_service = false;
  device_management_service_.reset();
}

PolicyService* BrowserPolicyConnector::GetPolicyService() {
  if (!policy_service_) {
    g_created_policy_service = true;
    std::vector<ConfigurationPolicyProvider*> providers;
    if (g_testing_provider) {
      providers.push_back(g_testing_provider);
    } else {
      providers.resize(policy_providers_.size());
      std::copy(policy_providers_.begin(),
                policy_providers_.end(),
                providers.begin());
    }
    policy_service_.reset(new PolicyServiceImpl(providers));
  }
  return policy_service_.get();
}

ConfigurationPolicyProvider* BrowserPolicyConnector::GetPlatformProvider() {
  if (g_testing_provider)
    return g_testing_provider;
  return platform_policy_provider_;
}

const Schema& BrowserPolicyConnector::GetChromeSchema() const {
  return chrome_schema_;
}

CombinedSchemaRegistry* BrowserPolicyConnector::GetSchemaRegistry() {
  return &schema_registry_;
}

void BrowserPolicyConnector::ScheduleServiceInitialization(
    int64 delay_milliseconds) {
  // Skip device initialization if the BrowserPolicyConnector was never
  // initialized (unit tests).
  if (device_management_service_)
    device_management_service_->ScheduleInitialization(delay_milliseconds);
}

const ConfigurationPolicyHandlerList*
    BrowserPolicyConnector::GetHandlerList() const {
  return handler_list_.get();
}

// static
void BrowserPolicyConnector::SetPolicyProviderForTesting(
    ConfigurationPolicyProvider* provider) {
  // If this function is used by a test then it must be called before the
  // browser is created, and GetPolicyService() gets called.
  CHECK(!g_created_policy_service);
  DCHECK(!g_testing_provider);
  g_testing_provider = provider;
}

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
  const base::string16 domain = base::UTF8ToUTF16(
      gaia::ExtractDomainName(gaia::CanonicalizeEmail(username)));
  for (size_t i = 0; i < arraysize(kNonManagedDomainPatterns); i++) {
    base::string16 pattern = base::WideToUTF16(kNonManagedDomainPatterns[i]);
    if (MatchDomain(domain, pattern))
      return true;
  }
  return false;
}

// static
std::string BrowserPolicyConnector::GetDeviceManagementUrl() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDeviceManagementUrl))
    return command_line->GetSwitchValueASCII(switches::kDeviceManagementUrl);
  else
    return kDefaultDeviceManagementServerUrl;
}

// static
void BrowserPolicyConnector::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      policy_prefs::kUserPolicyRefreshRate,
      CloudPolicyRefreshScheduler::kDefaultRefreshDelayMs);
}

void BrowserPolicyConnector::AddPolicyProvider(
    scoped_ptr<ConfigurationPolicyProvider> provider) {
  policy_providers_.push_back(provider.release());
}

void BrowserPolicyConnector::SetPlatformPolicyProvider(
    scoped_ptr<ConfigurationPolicyProvider> provider) {
  CHECK(!platform_policy_provider_);
  platform_policy_provider_ = provider.get();
  AddPolicyProvider(provider.Pass());
}

}  // namespace policy
