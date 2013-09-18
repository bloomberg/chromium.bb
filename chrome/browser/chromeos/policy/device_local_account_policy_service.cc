// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_local_account_policy_service.h"

#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/policy/device_local_account_policy_store.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/cros_settings_names.h"
#include "chrome/browser/chromeos/settings/cros_settings_provider.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/policy/cloud/cloud_policy_client.h"
#include "chrome/browser/policy/cloud/cloud_policy_constants.h"
#include "chrome/browser/policy/cloud/cloud_policy_refresh_scheduler.h"
#include "chrome/browser/policy/cloud/device_management_service.h"
#include "chrome/browser/policy/proto/cloud/device_management_backend.pb.h"
#include "chromeos/dbus/session_manager_client.h"
#include "content/public/browser/notification_details.h"
#include "policy/policy_constants.h"

namespace em = enterprise_management;

namespace policy {

namespace {

// Creates a broker for the device-local account with the given |user_id| and
// |account_id|.
scoped_ptr<DeviceLocalAccountPolicyBroker> CreateBroker(
    const std::string& user_id,
    const std::string& account_id,
    chromeos::SessionManagerClient* session_manager_client,
    chromeos::DeviceSettingsService* device_settings_service,
    DeviceLocalAccountPolicyService* device_local_account_policy_service) {
  scoped_ptr<DeviceLocalAccountPolicyStore> store(
      new DeviceLocalAccountPolicyStore(account_id, session_manager_client,
                                        device_settings_service));
  scoped_ptr<DeviceLocalAccountPolicyBroker> broker(
      new DeviceLocalAccountPolicyBroker(user_id,
                                         store.Pass(),
                                         base::MessageLoopProxy::current()));
  broker->core()->store()->AddObserver(device_local_account_policy_service);
  broker->core()->store()->Load();
  return broker.Pass();
}

// Creates and initializes a cloud policy client. Returns NULL if the device
// doesn't have credentials in device settings (i.e. is not
// enterprise-enrolled).
scoped_ptr<CloudPolicyClient> CreateClient(
    chromeos::DeviceSettingsService* device_settings_service,
    DeviceManagementService* device_management_service) {
  const em::PolicyData* policy_data = device_settings_service->policy_data();
  if (!policy_data ||
      !policy_data->has_request_token() ||
      !policy_data->has_device_id() ||
      !device_management_service) {
    return scoped_ptr<CloudPolicyClient>();
  }

  scoped_ptr<CloudPolicyClient> client(
      new CloudPolicyClient(std::string(), std::string(),
                            USER_AFFILIATION_MANAGED,
                            NULL, device_management_service));
  client->SetupRegistration(policy_data->request_token(),
                            policy_data->device_id());
  return client.Pass();
}

}  // namespace

DeviceLocalAccountPolicyBroker::DeviceLocalAccountPolicyBroker(
    const std::string& user_id,
    scoped_ptr<DeviceLocalAccountPolicyStore> store,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : user_id_(user_id),
      store_(store.Pass()),
      core_(PolicyNamespaceKey(dm_protocol::kChromePublicAccountPolicyType,
                               store_->account_id()),
            store_.get(),
            task_runner) {}

DeviceLocalAccountPolicyBroker::~DeviceLocalAccountPolicyBroker() {}

void DeviceLocalAccountPolicyBroker::Connect(
    scoped_ptr<CloudPolicyClient> client) {
  core_.Connect(client.Pass());
  core_.StartRefreshScheduler();
  UpdateRefreshDelay();
}

void DeviceLocalAccountPolicyBroker::Disconnect() {
  core_.Disconnect();
}

void DeviceLocalAccountPolicyBroker::UpdateRefreshDelay() {
  if (core_.refresh_scheduler()) {
    const Value* policy_value =
        store_->policy_map().GetValue(key::kPolicyRefreshRate);
    int delay = 0;
    if (policy_value && policy_value->GetAsInteger(&delay))
      core_.refresh_scheduler()->SetRefreshDelay(delay);
  }
}

std::string DeviceLocalAccountPolicyBroker::GetDisplayName() const {
  std::string display_name;
  const base::Value* display_name_value =
      store_->policy_map().GetValue(policy::key::kUserDisplayName);
  if (display_name_value)
    display_name_value->GetAsString(&display_name);
  return display_name;
}

DeviceLocalAccountPolicyService::PolicyBrokerWrapper::PolicyBrokerWrapper()
    : parent(NULL), broker(NULL) {}

DeviceLocalAccountPolicyBroker*
    DeviceLocalAccountPolicyService::PolicyBrokerWrapper::GetBroker() {
  if (!broker) {
    broker = CreateBroker(user_id, account_id,
                          parent->session_manager_client_,
                          parent->device_settings_service_,
                          parent).release();
  }
  return broker;
}

void DeviceLocalAccountPolicyService::PolicyBrokerWrapper::ConnectIfPossible() {
  if (broker && broker->core()->client())
    return;
  scoped_ptr<CloudPolicyClient> client(CreateClient(
      parent->device_settings_service_,
      parent->device_management_service_));
  if (client)
    GetBroker()->Connect(client.Pass());
}

void DeviceLocalAccountPolicyService::PolicyBrokerWrapper::Disconnect() {
  if (broker)
    broker->Disconnect();
}

void DeviceLocalAccountPolicyService::PolicyBrokerWrapper::DeleteBroker() {
  if (!broker)
    return;
  broker->core()->store()->RemoveObserver(parent);
  delete broker;
  broker = NULL;
}

DeviceLocalAccountPolicyService::DeviceLocalAccountPolicyService(
    chromeos::SessionManagerClient* session_manager_client,
    chromeos::DeviceSettingsService* device_settings_service,
    chromeos::CrosSettings* cros_settings)
    : session_manager_client_(session_manager_client),
      device_settings_service_(device_settings_service),
      cros_settings_(cros_settings),
      device_management_service_(NULL),
      cros_settings_callback_factory_(this) {
  cros_settings_->AddSettingsObserver(
      chromeos::kAccountsPrefDeviceLocalAccounts, this);
  UpdateAccountList();
}

DeviceLocalAccountPolicyService::~DeviceLocalAccountPolicyService() {
  cros_settings_->RemoveSettingsObserver(
      chromeos::kAccountsPrefDeviceLocalAccounts, this);
  DeleteBrokers(&policy_brokers_);
}

void DeviceLocalAccountPolicyService::Connect(
    DeviceManagementService* device_management_service) {
  DCHECK(!device_management_service_);
  device_management_service_ = device_management_service;

  // Connect the brokers.
  for (PolicyBrokerMap::iterator it(policy_brokers_.begin());
       it != policy_brokers_.end(); ++it) {
    it->second.ConnectIfPossible();
  }
}

void DeviceLocalAccountPolicyService::Disconnect() {
  DCHECK(device_management_service_);
  device_management_service_ = NULL;

  // Disconnect the brokers.
  for (PolicyBrokerMap::iterator it(policy_brokers_.begin());
       it != policy_brokers_.end(); ++it) {
    it->second.Disconnect();
  }
}

DeviceLocalAccountPolicyBroker*
    DeviceLocalAccountPolicyService::GetBrokerForUser(
        const std::string& user_id) {
  PolicyBrokerMap::iterator entry = policy_brokers_.find(user_id);
  if (entry == policy_brokers_.end())
    return NULL;

  return entry->second.GetBroker();
}

bool DeviceLocalAccountPolicyService::IsPolicyAvailableForUser(
    const std::string& user_id) {
  DeviceLocalAccountPolicyBroker* broker = GetBrokerForUser(user_id);
  return broker && broker->core()->store()->is_managed();
}

void DeviceLocalAccountPolicyService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DeviceLocalAccountPolicyService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void DeviceLocalAccountPolicyService::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type != chrome::NOTIFICATION_SYSTEM_SETTING_CHANGED ||
      *content::Details<const std::string>(details).ptr() !=
          chromeos::kAccountsPrefDeviceLocalAccounts) {
    NOTREACHED();
    return;
  }

  // Avoid unnecessary calls to UpdateAccountList(): If an earlier call is still
  // pending (because the |cros_settings_| are not trusted yet), the updated
  // account list will be processed by that call when it eventually runs.
  if (!cros_settings_callback_factory_.HasWeakPtrs())
    UpdateAccountList();
}

void DeviceLocalAccountPolicyService::OnStoreLoaded(CloudPolicyStore* store) {
  DeviceLocalAccountPolicyBroker* broker = GetBrokerForStore(store);
  DCHECK(broker);
  if (!broker)
    return;
  broker->UpdateRefreshDelay();
  FOR_EACH_OBSERVER(Observer, observers_, OnPolicyUpdated(broker->user_id()));
}

void DeviceLocalAccountPolicyService::OnStoreError(CloudPolicyStore* store) {
  DeviceLocalAccountPolicyBroker* broker = GetBrokerForStore(store);
  DCHECK(broker);
  if (!broker)
    return;
  FOR_EACH_OBSERVER(Observer, observers_, OnPolicyUpdated(broker->user_id()));
}

void DeviceLocalAccountPolicyService::UpdateAccountList() {
  if (chromeos::CrosSettingsProvider::TRUSTED !=
          cros_settings_->PrepareTrustedValues(
              base::Bind(&DeviceLocalAccountPolicyService::UpdateAccountList,
                         cros_settings_callback_factory_.GetWeakPtr()))) {
    return;
  }

  // Update |policy_brokers_|, keeping existing entries.
  PolicyBrokerMap new_policy_brokers;
  const std::vector<DeviceLocalAccount> device_local_accounts =
      GetDeviceLocalAccounts(cros_settings_);
  for (std::vector<DeviceLocalAccount>::const_iterator it =
           device_local_accounts.begin();
       it != device_local_accounts.end(); ++it) {
    PolicyBrokerWrapper& wrapper = new_policy_brokers[it->user_id];
    wrapper.user_id = it->user_id;
    wrapper.account_id = it->account_id;
    wrapper.parent = this;

    // Reuse the existing broker if present.
    PolicyBrokerWrapper& existing_wrapper = policy_brokers_[it->user_id];
    wrapper.broker = existing_wrapper.broker;
    existing_wrapper.broker = NULL;

    // Fire up the cloud connection for fetching policy for the account from
    // the cloud if this is an enterprise-managed device.
    wrapper.ConnectIfPossible();
  }
  policy_brokers_.swap(new_policy_brokers);
  DeleteBrokers(&new_policy_brokers);

  FOR_EACH_OBSERVER(Observer, observers_, OnDeviceLocalAccountsChanged());
}

void DeviceLocalAccountPolicyService::DeleteBrokers(PolicyBrokerMap* map) {
  for (PolicyBrokerMap::iterator it = map->begin(); it != map->end(); ++it)
    it->second.DeleteBroker();
  map->clear();
}

DeviceLocalAccountPolicyBroker*
    DeviceLocalAccountPolicyService::GetBrokerForStore(
        CloudPolicyStore* store) {
  for (PolicyBrokerMap::iterator it(policy_brokers_.begin());
       it != policy_brokers_.end(); ++it) {
    if (it->second.broker && it->second.broker->core()->store() == store)
      return it->second.broker;
  }
  return NULL;
}

}  // namespace policy
