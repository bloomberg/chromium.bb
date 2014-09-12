// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"

#include <string>
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/path_service.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chromeos/policy/app_pack_updater.h"
#include "chrome/browser/chromeos/policy/consumer_management_service.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_initializer.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_invalidator.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_store_chromeos.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/policy/device_local_account_policy_service.h"
#include "chrome/browser/chromeos/policy/device_network_configuration_updater.h"
#include "chrome/browser/chromeos/policy/enterprise_install_attributes.h"
#include "chrome/browser/chromeos/policy/server_backed_state_keys_broker.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/policy/device_management_service_configuration.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_paths.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/onc/onc_certificate_importer_impl.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/settings/cros_settings_provider.h"
#include "chromeos/settings/timezone_settings.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_refresh_scheduler.h"
#include "components/policy/core/common/proxy_policy_provider.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "net/url_request/url_request_context_getter.h"

using content::BrowserThread;

namespace policy {

namespace {

// TODO(davidyu): Update the URL to the real one once it is ready.
// http://crbug.com/366491.
//
// The URL for the consumer device management server.
const char kDefaultConsumerDeviceManagementServerUrl[] =
    "https://m.google.com/devicemanagement/data/api";

// Install attributes for tests.
EnterpriseInstallAttributes* g_testing_install_attributes = NULL;

// Helper that returns a new SequencedTaskRunner backed by the blocking pool.
// Each SequencedTaskRunner returned is independent from the others.
scoped_refptr<base::SequencedTaskRunner> GetBackgroundTaskRunner() {
  base::SequencedWorkerPool* pool = BrowserThread::GetBlockingPool();
  CHECK(pool);
  return pool->GetSequencedTaskRunnerWithShutdownBehavior(
      pool->GetSequenceToken(), base::SequencedWorkerPool::SKIP_ON_SHUTDOWN);
}

std::string GetDeviceManagementServerUrlForConsumer() {
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(
          chromeos::switches::kConsumerDeviceManagementUrl)) {
    return command_line->GetSwitchValueASCII(
        chromeos::switches::kConsumerDeviceManagementUrl);
  }
  return kDefaultConsumerDeviceManagementServerUrl;
}

}  // namespace

BrowserPolicyConnectorChromeOS::BrowserPolicyConnectorChromeOS()
    : device_cloud_policy_manager_(NULL),
      global_user_cloud_policy_provider_(NULL),
      weak_ptr_factory_(this) {
  if (g_testing_install_attributes) {
    install_attributes_.reset(g_testing_install_attributes);
    g_testing_install_attributes = NULL;
  }

  // SystemSaltGetter or DBusThreadManager may be uninitialized on unit tests.

  // TODO(satorux): Remove SystemSaltGetter::IsInitialized() when it's ready
  // (removing it now breaks tests). crbug.com/141016.
  if (chromeos::SystemSaltGetter::IsInitialized() &&
      chromeos::DBusThreadManager::IsInitialized()) {
    state_keys_broker_.reset(new ServerBackedStateKeysBroker(
        chromeos::DBusThreadManager::Get()->GetSessionManagerClient(),
        base::MessageLoopProxy::current()));

    chromeos::CryptohomeClient* cryptohome_client =
        chromeos::DBusThreadManager::Get()->GetCryptohomeClient();
    if (!install_attributes_) {
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
    device_cloud_policy_manager_ =
        new DeviceCloudPolicyManagerChromeOS(device_cloud_policy_store.Pass(),
                                             base::MessageLoopProxy::current(),
                                             state_keys_broker_.get());
    AddPolicyProvider(
        scoped_ptr<ConfigurationPolicyProvider>(device_cloud_policy_manager_));
  }

  global_user_cloud_policy_provider_ = new ProxyPolicyProvider();
  AddPolicyProvider(scoped_ptr<ConfigurationPolicyProvider>(
      global_user_cloud_policy_provider_));
}

BrowserPolicyConnectorChromeOS::~BrowserPolicyConnectorChromeOS() {}

void BrowserPolicyConnectorChromeOS::Init(
    PrefService* local_state,
    scoped_refptr<net::URLRequestContextGetter> request_context) {
  ChromeBrowserPolicyConnector::Init(local_state, request_context);

  scoped_ptr<DeviceManagementService::Configuration> configuration(
      new DeviceManagementServiceConfiguration(
          GetDeviceManagementServerUrlForConsumer()));
  consumer_device_management_service_.reset(
      new DeviceManagementService(configuration.Pass()));
  consumer_device_management_service_->ScheduleInitialization(
      kServiceInitializationStartupDelay);

  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(chromeos::switches::kEnableConsumerManagement)) {
    chromeos::CryptohomeClient* cryptohome_client =
        chromeos::DBusThreadManager::Get()->GetCryptohomeClient();
    consumer_management_service_.reset(
        new ConsumerManagementService(cryptohome_client,
                                      chromeos::DeviceSettingsService::Get()));
  }

  if (device_cloud_policy_manager_) {
    // Note: for now the |device_cloud_policy_manager_| is using the global
    // schema registry. Eventually it will have its own registry, once device
    // cloud policy for extensions is introduced. That means it'd have to be
    // initialized from here instead of BrowserPolicyConnector::Init().

    device_cloud_policy_manager_->Initialize(local_state);

    device_cloud_policy_initializer_.reset(
        new DeviceCloudPolicyInitializer(
            local_state,
            device_management_service(),
            GetDeviceManagementServiceForConsumer(),
            GetBackgroundTaskRunner(),
            install_attributes_.get(),
            state_keys_broker_.get(),
            device_cloud_policy_manager_->device_store(),
            device_cloud_policy_manager_,
            chromeos::DeviceSettingsService::Get(),
            base::Bind(&BrowserPolicyConnectorChromeOS::
                           OnDeviceCloudPolicyManagerConnected,
                       base::Unretained(this))));
    device_cloud_policy_initializer_->Init();
  }

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
  device_local_account_policy_service_->Connect(device_management_service());
  device_cloud_policy_invalidator_.reset(new DeviceCloudPolicyInvalidator);

  // request_context is NULL in unit tests.
  if (request_context.get() && install_attributes_) {
    app_pack_updater_.reset(
        new AppPackUpdater(request_context.get(), install_attributes_.get()));
  }

  SetTimezoneIfPolicyAvailable();

  network_configuration_updater_ =
      DeviceNetworkConfigurationUpdater::CreateForDevicePolicy(
          GetPolicyService(),
          chromeos::NetworkHandler::Get()
              ->managed_network_configuration_handler(),
          chromeos::NetworkHandler::Get()->network_device_handler(),
          chromeos::CrosSettings::Get());
}

void BrowserPolicyConnectorChromeOS::PreShutdown() {
  // Let the |device_cloud_policy_invalidator_| unregister itself as an
  // observer of per-Profile InvalidationServices and the device-global
  // invalidation::TiclInvalidationService it may have created as an observer of
  // the DeviceOAuth2TokenService that is destroyed before Shutdown() is called.
  device_cloud_policy_invalidator_.reset();

  // The |consumer_management_service_| may be observing a
  // ProfileOAuth2TokenService and needs to be destroyed before the token
  // service.
  consumer_management_service_.reset();
}

void BrowserPolicyConnectorChromeOS::Shutdown() {
  // Verify that PreShutdown() has been called first.
  DCHECK(!device_cloud_policy_invalidator_);
  DCHECK(!consumer_management_service_);

  // The AppPackUpdater may be observing the |device_cloud_policy_manager_|.
  // Delete it first.
  app_pack_updater_.reset();

  network_configuration_updater_.reset();

  if (device_local_account_policy_service_)
    device_local_account_policy_service_->Shutdown();

  if (device_cloud_policy_initializer_)
    device_cloud_policy_initializer_->Shutdown();

  ChromeBrowserPolicyConnector::Shutdown();
}

bool BrowserPolicyConnectorChromeOS::IsEnterpriseManaged() {
  return install_attributes_ && install_attributes_->IsEnterpriseDevice();
}

std::string BrowserPolicyConnectorChromeOS::GetEnterpriseDomain() {
  return install_attributes_ ? install_attributes_->GetDomain() : std::string();
}

DeviceMode BrowserPolicyConnectorChromeOS::GetDeviceMode() {
  return install_attributes_ ? install_attributes_->GetMode()
                             : DEVICE_MODE_NOT_SET;
}

UserAffiliation BrowserPolicyConnectorChromeOS::GetUserAffiliation(
    const std::string& user_name) {
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

  return USER_AFFILIATION_NONE;
}

AppPackUpdater* BrowserPolicyConnectorChromeOS::GetAppPackUpdater() {
  return app_pack_updater_.get();
}

void BrowserPolicyConnectorChromeOS::SetUserPolicyDelegate(
    ConfigurationPolicyProvider* user_policy_provider) {
  global_user_cloud_policy_provider_->SetDelegate(user_policy_provider);
}

void BrowserPolicyConnectorChromeOS::SetDeviceCloudPolicyInitializerForTesting(
    scoped_ptr<DeviceCloudPolicyInitializer> initializer) {
  device_cloud_policy_initializer_ = initializer.Pass();
}

void BrowserPolicyConnectorChromeOS::SetInstallAttributesForTesting(
    EnterpriseInstallAttributes* attributes) {
  DCHECK(!g_testing_install_attributes);
  g_testing_install_attributes = attributes;
}

void BrowserPolicyConnectorChromeOS::RemoveInstallAttributesForTesting() {
  if (g_testing_install_attributes) {
    delete g_testing_install_attributes;
    g_testing_install_attributes = NULL;
  }
}

// static
void BrowserPolicyConnectorChromeOS::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      prefs::kDevicePolicyRefreshRate,
      CloudPolicyRefreshScheduler::kDefaultRefreshDelayMs);
}

void BrowserPolicyConnectorChromeOS::SetTimezoneIfPolicyAvailable() {
  typedef chromeos::CrosSettingsProvider Provider;
  Provider::TrustedStatus result =
      chromeos::CrosSettings::Get()->PrepareTrustedValues(base::Bind(
          &BrowserPolicyConnectorChromeOS::SetTimezoneIfPolicyAvailable,
          weak_ptr_factory_.GetWeakPtr()));

  if (result != Provider::TRUSTED)
    return;

  std::string timezone;
  if (chromeos::CrosSettings::Get()->GetString(chromeos::kSystemTimezonePolicy,
                                               &timezone) &&
      !timezone.empty()) {
    chromeos::system::TimezoneSettings::GetInstance()->SetTimezoneFromID(
        base::UTF8ToUTF16(timezone));
  }
}

void BrowserPolicyConnectorChromeOS::OnDeviceCloudPolicyManagerConnected() {
  // This function is invoked by DCPInitializer, so we should release the
  // initializer after this function returns.
  if (device_cloud_policy_initializer_) {
    device_cloud_policy_initializer_->Shutdown();
    base::MessageLoop::current()->DeleteSoon(
        FROM_HERE, device_cloud_policy_initializer_.release());
  }
}

}  // namespace policy
