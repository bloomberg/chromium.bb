// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/attestation/attestation_ca_client.h"
#include "chrome/browser/chromeos/policy/active_directory_policy_manager.h"
#include "chrome/browser/chromeos/policy/affiliated_cloud_policy_invalidator.h"
#include "chrome/browser/chromeos/policy/affiliated_invalidation_service_provider.h"
#include "chrome/browser/chromeos/policy/affiliated_invalidation_service_provider_impl.h"
#include "chrome/browser/chromeos/policy/bluetooth_policy_handler.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_initializer.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_store_chromeos.h"
#include "chrome/browser/chromeos/policy/device_dock_mac_address_source_handler.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/policy/device_local_account_policy_service.h"
#include "chrome/browser/chromeos/policy/device_network_configuration_updater.h"
#include "chrome/browser/chromeos/policy/device_policy_cloud_external_data_manager.h"
#include "chrome/browser/chromeos/policy/device_wifi_allowed_handler.h"
#include "chrome/browser/chromeos/policy/enrollment_config.h"
#include "chrome/browser/chromeos/policy/external_data_handlers/device_native_printers_external_data_handler.h"
#include "chrome/browser/chromeos/policy/external_data_handlers/device_wallpaper_image_external_data_handler.h"
#include "chrome/browser/chromeos/policy/external_data_handlers/device_wilco_dtc_configuration_external_data_handler.h"
#include "chrome/browser/chromeos/policy/hostname_handler.h"
#include "chrome/browser/chromeos/policy/minimum_version_policy_handler.h"
#include "chrome/browser/chromeos/policy/minimum_version_policy_handler_delegate_impl.h"
#include "chrome/browser/chromeos/policy/remote_commands/affiliated_remote_commands_invalidator.h"
#include "chrome/browser/chromeos/policy/scheduled_update_checker/device_scheduled_update_checker.h"
#include "chrome/browser/chromeos/policy/server_backed_state_keys_broker.h"
#include "chrome/browser/chromeos/policy/system_proxy_manager.h"
#include "chrome/browser/chromeos/policy/tpm_auto_update_mode_policy_handler.h"
#include "chrome/browser/chromeos/printing/bulk_printers_calculator_factory.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/chromeos/system/timezone_util.h"
#include "chrome/browser/policy/device_management_service_configuration.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chromeos/attestation/attestation_flow.h"
#include "chromeos/constants/chromeos_paths.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "chromeos/dbus/upstart/upstart_client.h"
#include "chromeos/network/network_cert_loader.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/onc/onc_certificate_importer_impl.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/settings/cros_settings_provider.h"
#include "chromeos/settings/timezone_settings.h"
#include "chromeos/system/statistics_provider.h"
#include "chromeos/tpm/install_attributes.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_refresh_scheduler.h"
#include "components/policy/core/common/cloud/resource_cache.h"
#include "components/policy/core/common/proxy_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/pref_names.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace policy {

namespace em = enterprise_management;

namespace {

// Helper that returns a new BACKGROUND SequencedTaskRunner. Each
// SequencedTaskRunner returned is independent from the others.
scoped_refptr<base::SequencedTaskRunner> GetBackgroundTaskRunner() {
  return base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
}

MarketSegment TranslateMarketSegment(
    em::PolicyData::MarketSegment market_segment) {
  switch (market_segment) {
    case em::PolicyData::MARKET_SEGMENT_UNSPECIFIED:
      return MarketSegment::UNKNOWN;
    case em::PolicyData::ENROLLED_EDUCATION:
      return MarketSegment::EDUCATION;
    case em::PolicyData::ENROLLED_ENTERPRISE:
      return MarketSegment::ENTERPRISE;
  }
  NOTREACHED();
  return MarketSegment::UNKNOWN;
}

}  // namespace

BrowserPolicyConnectorChromeOS::BrowserPolicyConnectorChromeOS() {
  DCHECK(chromeos::InstallAttributes::IsInitialized());

  // DBusThreadManager or DeviceSettingsService may be
  // uninitialized on unit tests.

  // TODO(satorux): Remove SystemSaltGetter::IsInitialized() when it's ready
  // (removing it now breaks tests). crbug.com/141016.
  if (chromeos::DBusThreadManager::IsInitialized() &&
      chromeos::DeviceSettingsService::IsInitialized()) {
    std::unique_ptr<DeviceCloudPolicyStoreChromeOS> device_cloud_policy_store =
        std::make_unique<DeviceCloudPolicyStoreChromeOS>(
            chromeos::DeviceSettingsService::Get(),
            chromeos::InstallAttributes::Get(), GetBackgroundTaskRunner());

    if (chromeos::InstallAttributes::Get()->IsActiveDirectoryManaged()) {
      chromeos::UpstartClient::Get()->StartAuthPolicyService();

      device_active_directory_policy_manager_ =
          new DeviceActiveDirectoryPolicyManager(
              std::move(device_cloud_policy_store));
      providers_for_init_.push_back(
          base::WrapUnique<ConfigurationPolicyProvider>(
              device_active_directory_policy_manager_));
    } else {
      state_keys_broker_ = std::make_unique<ServerBackedStateKeysBroker>(
          chromeos::SessionManagerClient::Get());

      base::FilePath device_policy_external_data_path;
      CHECK(base::PathService::Get(chromeos::DIR_DEVICE_POLICY_EXTERNAL_DATA,
                                   &device_policy_external_data_path));

      auto external_data_manager =
          std::make_unique<DevicePolicyCloudExternalDataManager>(
              base::BindRepeating(&GetChromePolicyDetails),
              GetBackgroundTaskRunner(), device_policy_external_data_path,
              device_cloud_policy_store.get());

      device_cloud_policy_manager_ = new DeviceCloudPolicyManagerChromeOS(
          std::move(device_cloud_policy_store),
          std::move(external_data_manager), base::ThreadTaskRunnerHandle::Get(),
          state_keys_broker_.get());
      providers_for_init_.push_back(
          base::WrapUnique<ConfigurationPolicyProvider>(
              device_cloud_policy_manager_));
    }
  }

  global_user_cloud_policy_provider_ = new ProxyPolicyProvider();
  providers_for_init_.push_back(std::unique_ptr<ConfigurationPolicyProvider>(
      global_user_cloud_policy_provider_));
}

BrowserPolicyConnectorChromeOS::~BrowserPolicyConnectorChromeOS() {}

void BrowserPolicyConnectorChromeOS::Init(
    PrefService* local_state,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  local_state_ = local_state;
  ChromeBrowserPolicyConnector::Init(local_state, url_loader_factory);

  affiliated_invalidation_service_provider_ =
      std::make_unique<AffiliatedInvalidationServiceProviderImpl>();

  if (device_cloud_policy_manager_) {
    // Note: for now the |device_cloud_policy_manager_| is using the global
    // schema registry. Eventually it will have its own registry, once device
    // cloud policy for extensions is introduced. That means it'd have to be
    // initialized from here instead of BrowserPolicyConnector::Init().

    device_cloud_policy_manager_->Initialize(local_state);
    device_cloud_policy_manager_->AddDeviceCloudPolicyManagerObserver(this);
    RestartDeviceCloudPolicyInitializer();
  }

  if (!chromeos::InstallAttributes::Get()->IsActiveDirectoryManaged()) {
    device_local_account_policy_service_ =
        std::make_unique<DeviceLocalAccountPolicyService>(
            chromeos::SessionManagerClient::Get(),
            chromeos::DeviceSettingsService::Get(),
            chromeos::CrosSettings::Get(),
            affiliated_invalidation_service_provider_.get(),
            GetBackgroundTaskRunner(), GetBackgroundTaskRunner(),
            GetBackgroundTaskRunner(), url_loader_factory);
    device_local_account_policy_service_->Connect(device_management_service());
  }

  if (device_cloud_policy_manager_) {
    device_cloud_policy_invalidator_ =
        std::make_unique<AffiliatedCloudPolicyInvalidator>(
            PolicyInvalidationScope::kDevice,
            device_cloud_policy_manager_->core(),
            affiliated_invalidation_service_provider_.get());
    device_remote_commands_invalidator_ =
        std::make_unique<AffiliatedRemoteCommandsInvalidator>(
            device_cloud_policy_manager_->core(),
            affiliated_invalidation_service_provider_.get(),
            PolicyInvalidationScope::kDevice);
  }

  SetTimezoneIfPolicyAvailable();

  device_network_configuration_updater_ =
      DeviceNetworkConfigurationUpdater::CreateForDevicePolicy(
          GetPolicyService(),
          chromeos::NetworkHandler::Get()
              ->managed_network_configuration_handler(),
          chromeos::NetworkHandler::Get()->network_device_handler(),
          chromeos::CrosSettings::Get(),
          DeviceNetworkConfigurationUpdater::DeviceAssetIDFetcher());
  // NetworkCertLoader may be not initialized in tests.
  if (chromeos::NetworkCertLoader::IsInitialized()) {
    chromeos::NetworkCertLoader::Get()->SetDevicePolicyCertificateProvider(
        device_network_configuration_updater_.get());
  }

  bluetooth_policy_handler_ =
      std::make_unique<BluetoothPolicyHandler>(chromeos::CrosSettings::Get());

  hostname_handler_ =
      std::make_unique<HostnameHandler>(chromeos::CrosSettings::Get());

  minimum_version_policy_handler_delegate_ =
      std::make_unique<MinimumVersionPolicyHandlerDelegateImpl>();

  minimum_version_policy_handler_ =
      std::make_unique<MinimumVersionPolicyHandler>(
          minimum_version_policy_handler_delegate_.get(),
          chromeos::CrosSettings::Get());

  device_dock_mac_address_source_handler_ =
      std::make_unique<DeviceDockMacAddressHandler>(
          chromeos::CrosSettings::Get(),
          chromeos::NetworkHandler::Get()->network_device_handler());

  device_wifi_allowed_handler_ =
      std::make_unique<DeviceWiFiAllowedHandler>(chromeos::CrosSettings::Get());

  tpm_auto_update_mode_policy_handler_ =
      std::make_unique<TPMAutoUpdateModePolicyHandler>(
          chromeos::CrosSettings::Get(), local_state);

  device_scheduled_update_checker_ =
      std::make_unique<DeviceScheduledUpdateChecker>(
          chromeos::CrosSettings::Get(),
          chromeos::NetworkHandler::Get()->network_state_handler());

  chromeos::BulkPrintersCalculatorFactory* calculator_factory =
      chromeos::BulkPrintersCalculatorFactory::Get();
  DCHECK(calculator_factory)
      << "Policy connector initialized before the bulk printers factory";
  device_cloud_external_data_policy_handlers_.push_back(
      std::make_unique<policy::DeviceNativePrintersExternalDataHandler>(
          GetPolicyService(), calculator_factory->GetForDevice()));

  device_cloud_external_data_policy_handlers_.push_back(
      std::make_unique<policy::DeviceWallpaperImageExternalDataHandler>(
          local_state, GetPolicyService()));
  if (base::FeatureList::IsEnabled(::features::kWilcoDtc)) {
    device_cloud_external_data_policy_handlers_.push_back(
        std::make_unique<
            policy::DeviceWilcoDtcConfigurationExternalDataHandler>(
            GetPolicyService()));
  }
  system_proxy_manager_ =
      std::make_unique<SystemProxyManager>(chromeos::CrosSettings::Get());
}

void BrowserPolicyConnectorChromeOS::PreShutdown() {
  // Let the |affiliated_invalidation_service_provider_| unregister itself as an
  // observer of per-Profile InvalidationServices and the device-global
  // invalidation::TiclInvalidationService it may have created as an observer of
  // the DeviceOAuth2TokenService that is destroyed before Shutdown() is called.
  if (affiliated_invalidation_service_provider_)
    affiliated_invalidation_service_provider_->Shutdown();
}

void BrowserPolicyConnectorChromeOS::Shutdown() {
  device_cert_provisioning_scheduler_.reset();

  // NetworkCertLoader may be not initialized in tests.
  if (chromeos::NetworkCertLoader::IsInitialized()) {
    chromeos::NetworkCertLoader::Get()->SetDevicePolicyCertificateProvider(
        nullptr);
  }
  device_network_configuration_updater_.reset();

  if (device_local_account_policy_service_)
    device_local_account_policy_service_->Shutdown();

  if (device_cloud_policy_initializer_)
    device_cloud_policy_initializer_->Shutdown();

  if (device_cloud_policy_manager_)
    device_cloud_policy_manager_->RemoveDeviceCloudPolicyManagerObserver(this);

  device_scheduled_update_checker_.reset();

  if (hostname_handler_)
    hostname_handler_->Shutdown();

  for (auto& device_cloud_external_data_policy_handler :
       device_cloud_external_data_policy_handlers_) {
    device_cloud_external_data_policy_handler->Shutdown();
  }

  ChromeBrowserPolicyConnector::Shutdown();
}

bool BrowserPolicyConnectorChromeOS::IsEnterpriseManaged() const {
  return chromeos::InstallAttributes::Get()->IsEnterpriseManaged();
}

bool BrowserPolicyConnectorChromeOS::HasMachineLevelPolicies() {
  NOTREACHED() << "This method is only defined for desktop Chrome";
  return false;
}

bool BrowserPolicyConnectorChromeOS::IsCloudManaged() const {
  return chromeos::InstallAttributes::Get()->IsCloudManaged();
}

bool BrowserPolicyConnectorChromeOS::IsActiveDirectoryManaged() const {
  return chromeos::InstallAttributes::Get()->IsActiveDirectoryManaged();
}

std::string BrowserPolicyConnectorChromeOS::GetEnterpriseEnrollmentDomain()
    const {
  return chromeos::InstallAttributes::Get()->GetDomain();
}

std::string BrowserPolicyConnectorChromeOS::GetEnterpriseDisplayDomain() const {
  const em::PolicyData* policy = GetDevicePolicy();
  if (policy && policy->has_display_domain())
    return policy->display_domain();
  return GetEnterpriseEnrollmentDomain();
}

std::string BrowserPolicyConnectorChromeOS::GetRealm() const {
  return chromeos::InstallAttributes::Get()->GetRealm();
}

std::string BrowserPolicyConnectorChromeOS::GetDeviceAssetID() const {
  const em::PolicyData* policy = GetDevicePolicy();
  if (policy && policy->has_annotated_asset_id())
    return policy->annotated_asset_id();
  return std::string();
}

std::string BrowserPolicyConnectorChromeOS::GetMachineName() const {
  const em::PolicyData* policy = GetDevicePolicy();
  if (policy && policy->has_machine_name())
    return policy->machine_name();
  return std::string();
}

std::string BrowserPolicyConnectorChromeOS::GetDeviceAnnotatedLocation() const {
  const em::PolicyData* policy = GetDevicePolicy();
  if (policy && policy->has_annotated_location())
    return policy->annotated_location();
  return std::string();
}

std::string BrowserPolicyConnectorChromeOS::GetDirectoryApiID() const {
  const em::PolicyData* policy = GetDevicePolicy();
  if (policy && policy->has_directory_api_id())
    return policy->directory_api_id();
  return std::string();
}

std::string BrowserPolicyConnectorChromeOS::GetCustomerLogoURL() const {
  const em::PolicyData* policy = GetDevicePolicy();
  if (policy && policy->has_customer_logo())
    return policy->customer_logo().logo_url();
  return std::string();
}

DeviceMode BrowserPolicyConnectorChromeOS::GetDeviceMode() const {
  return chromeos::InstallAttributes::Get()->GetMode();
}

chromeos::InstallAttributes*
BrowserPolicyConnectorChromeOS::GetInstallAttributes() const {
  return chromeos::InstallAttributes::Get();
}

EnrollmentConfig BrowserPolicyConnectorChromeOS::GetPrescribedEnrollmentConfig()
    const {
  if (device_cloud_policy_initializer_)
    return device_cloud_policy_initializer_->GetPrescribedEnrollmentConfig();

  return EnrollmentConfig();
}

MarketSegment BrowserPolicyConnectorChromeOS::GetEnterpriseMarketSegment()
    const {
  const em::PolicyData* policy = GetDevicePolicy();
  if (policy && policy->has_market_segment())
    return TranslateMarketSegment(policy->market_segment());
  return MarketSegment::UNKNOWN;
}

ProxyPolicyProvider*
BrowserPolicyConnectorChromeOS::GetGlobalUserCloudPolicyProvider() {
  return global_user_cloud_policy_provider_;
}

void BrowserPolicyConnectorChromeOS::SetDeviceCloudPolicyInitializerForTesting(
    std::unique_ptr<DeviceCloudPolicyInitializer> initializer) {
  device_cloud_policy_initializer_ = std::move(initializer);
}

// static
void BrowserPolicyConnectorChromeOS::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      prefs::kDevicePolicyRefreshRate,
      CloudPolicyRefreshScheduler::kDefaultRefreshDelayMs);
}

void BrowserPolicyConnectorChromeOS::OnDeviceCloudPolicyManagerConnected() {
  CHECK(device_cloud_policy_initializer_);

  // DeviceCloudPolicyInitializer might still be on the call stack, so we
  // should delete the initializer after this function returns.
  device_cloud_policy_initializer_->Shutdown();
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(
      FROM_HERE, std::move(device_cloud_policy_initializer_));

  // TODO(miersh) Move to BrowserPolicyConnectorChromeOS::Init() when
  // CertProvisioningScheduler does not depend on SignIn Profile.
  if (!device_cert_provisioning_scheduler_) {
    device_cert_provisioning_scheduler_ = chromeos::cert_provisioning::
        CertProvisioningScheduler::CreateDeviceCertProvisioningScheduler(
            affiliated_invalidation_service_provider_.get());
  }
}

void BrowserPolicyConnectorChromeOS::OnDeviceCloudPolicyManagerDisconnected() {
  DCHECK(!device_cloud_policy_initializer_);

  RestartDeviceCloudPolicyInitializer();
}

std::vector<std::unique_ptr<policy::ConfigurationPolicyProvider>>
BrowserPolicyConnectorChromeOS::CreatePolicyProviders() {
  auto providers = ChromeBrowserPolicyConnector::CreatePolicyProviders();
  for (auto& provider_ptr : providers_for_init_)
    providers.push_back(std::move(provider_ptr));
  providers_for_init_.clear();
  return providers;
}

void BrowserPolicyConnectorChromeOS::SetTimezoneIfPolicyAvailable() {
  typedef chromeos::CrosSettingsProvider Provider;
  Provider::TrustedStatus result =
      chromeos::CrosSettings::Get()->PrepareTrustedValues(base::BindOnce(
          &BrowserPolicyConnectorChromeOS::SetTimezoneIfPolicyAvailable,
          weak_ptr_factory_.GetWeakPtr()));

  if (result != Provider::TRUSTED)
    return;

  std::string timezone;
  if (chromeos::CrosSettings::Get()->GetString(chromeos::kSystemTimezonePolicy,
                                               &timezone) &&
      !timezone.empty()) {
    chromeos::system::SetSystemAndSigninScreenTimezone(timezone);
  }
}

void BrowserPolicyConnectorChromeOS::RestartDeviceCloudPolicyInitializer() {
  device_cloud_policy_initializer_ =
      std::make_unique<DeviceCloudPolicyInitializer>(
          local_state_, device_management_service(), GetBackgroundTaskRunner(),
          chromeos::InstallAttributes::Get(), state_keys_broker_.get(),
          device_cloud_policy_manager_->device_store(),
          device_cloud_policy_manager_,
          cryptohome::AsyncMethodCaller::GetInstance(), CreateAttestationFlow(),
          chromeos::system::StatisticsProvider::GetInstance());
  device_cloud_policy_initializer_->Init();
}

std::unique_ptr<chromeos::attestation::AttestationFlow>
BrowserPolicyConnectorChromeOS::CreateAttestationFlow() {
  return std::make_unique<chromeos::attestation::AttestationFlow>(
      cryptohome::AsyncMethodCaller::GetInstance(),
      chromeos::CryptohomeClient::Get(),
      std::make_unique<chromeos::attestation::AttestationCAClient>());
}

chromeos::AffiliationIDSet
BrowserPolicyConnectorChromeOS::GetDeviceAffiliationIDs() const {
  chromeos::AffiliationIDSet affiliation_ids;
  const em::PolicyData* policy = GetDevicePolicy();
  if (policy) {
    affiliation_ids.insert(policy->device_affiliation_ids().begin(),
                           policy->device_affiliation_ids().end());
  }
  return affiliation_ids;
}

const em::PolicyData* BrowserPolicyConnectorChromeOS::GetDevicePolicy() const {
  if (device_cloud_policy_manager_)
    return device_cloud_policy_manager_->device_store()->policy();

  if (device_active_directory_policy_manager_)
    return device_active_directory_policy_manager_->store()->policy();

  return nullptr;
}

}  // namespace policy
