// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_local_account_policy_service.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome/browser/policy/cloud_policy_client.h"
#include "chrome/browser/policy/cloud_policy_refresh_scheduler.h"
#include "chrome/browser/policy/cloud_policy_service.h"
#include "chrome/browser/policy/device_management_service.h"
#include "chrome/browser/policy/proto/chrome_device_policy.pb.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "chromeos/dbus/session_manager_client.h"
#include "policy/policy_constants.h"

namespace em = enterprise_management;

namespace policy {

DeviceLocalAccountPolicyBroker::DeviceLocalAccountPolicyBroker(
    const std::string& account_id,
    chromeos::SessionManagerClient* session_manager_client,
    chromeos::DeviceSettingsService* device_settings_service)
    : account_id_(account_id),
      store_(account_id, session_manager_client, device_settings_service) {}

DeviceLocalAccountPolicyBroker::~DeviceLocalAccountPolicyBroker() {}

void DeviceLocalAccountPolicyBroker::RefreshPolicy(
    const base::Closure& callback) {
  if (service_.get())
    service_->RefreshPolicy(callback);
  else
    callback.Run();
}

void DeviceLocalAccountPolicyBroker::Connect(
    scoped_ptr<CloudPolicyClient> client) {
  DCHECK(!client_.get());
  client_ = client.Pass();
  service_.reset(new CloudPolicyService(client_.get(), &store_));
  refresh_scheduler_.reset(
      new CloudPolicyRefreshScheduler(
          client_.get(), &store_,
          MessageLoop::current()->message_loop_proxy()));
  UpdateRefreshDelay();
}

void DeviceLocalAccountPolicyBroker::Disconnect() {
  DCHECK(client_.get());
  refresh_scheduler_.reset();
  service_.reset();
  client_.reset();
}

void DeviceLocalAccountPolicyBroker::UpdateRefreshDelay() {
  if (refresh_scheduler_) {
    const Value* policy_value =
        store_.policy_map().GetValue(key::kPolicyRefreshRate);
    int delay = 0;
    if (policy_value && policy_value->GetAsInteger(&delay))
      refresh_scheduler_->SetRefreshDelay(delay);
  }
}

DeviceLocalAccountPolicyService::DeviceLocalAccountPolicyService(
    chromeos::SessionManagerClient* session_manager_client,
    chromeos::DeviceSettingsService* device_settings_service)
    : session_manager_client_(session_manager_client),
      device_settings_service_(device_settings_service),
      device_management_service_(NULL) {
  device_settings_service_->AddObserver(this);
  DeviceSettingsUpdated();
}

DeviceLocalAccountPolicyService::~DeviceLocalAccountPolicyService() {
  device_settings_service_->RemoveObserver(this);
  DeleteBrokers(&policy_brokers_);
}

void DeviceLocalAccountPolicyService::Connect(
    DeviceManagementService* device_management_service) {
  DCHECK(!device_management_service_);
  device_management_service_ = device_management_service;

  // Connect the brokers.
  for (PolicyBrokerMap::iterator broker(policy_brokers_.begin());
       broker != policy_brokers_.end(); ++broker) {
    DCHECK(!broker->second->client());
    broker->second->Connect(
        CreateClientForAccount(broker->second->account_id()).Pass());
  }
}

void DeviceLocalAccountPolicyService::Disconnect() {
  DCHECK(device_management_service_);
  device_management_service_ = NULL;

  // Disconnect the brokers.
  for (PolicyBrokerMap::iterator broker(policy_brokers_.begin());
       broker != policy_brokers_.end(); ++broker) {
    broker->second->Disconnect();
  }
}

DeviceLocalAccountPolicyBroker*
    DeviceLocalAccountPolicyService::GetBrokerForAccount(
        const std::string& account_id) {
  PolicyBrokerMap::iterator entry = policy_brokers_.find(account_id);
  if (entry == policy_brokers_.end())
    return NULL;

  if (!entry->second)
    entry->second = CreateBroker(account_id).release();

  return entry->second;
}

void DeviceLocalAccountPolicyService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DeviceLocalAccountPolicyService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void DeviceLocalAccountPolicyService::OwnershipStatusChanged() {
  // TODO(mnissler): The policy key has changed, re-fetch policy. For
  // consumer devices, re-sign the current settings and send updates to
  // session_manager.
}

void DeviceLocalAccountPolicyService::DeviceSettingsUpdated() {
  const em::ChromeDeviceSettingsProto* device_settings =
      device_settings_service_->device_settings();
  if (device_settings)
    UpdateAccountList(*device_settings);
}

void DeviceLocalAccountPolicyService::OnStoreLoaded(CloudPolicyStore* store) {
  DeviceLocalAccountPolicyBroker* broker = GetBrokerForStore(store);
  broker->UpdateRefreshDelay();
  FOR_EACH_OBSERVER(Observer, observers_,
                    OnPolicyUpdated(broker->account_id()));
}

void DeviceLocalAccountPolicyService::OnStoreError(CloudPolicyStore* store) {
  DeviceLocalAccountPolicyBroker* broker = GetBrokerForStore(store);
  FOR_EACH_OBSERVER(Observer, observers_,
                    OnPolicyUpdated(broker->account_id()));
}

void DeviceLocalAccountPolicyService::UpdateAccountList(
    const em::ChromeDeviceSettingsProto& device_settings) {
  using google::protobuf::RepeatedPtrField;

  // Update |policy_brokers_|, keeping existing entries.
  PolicyBrokerMap new_policy_brokers;
  const RepeatedPtrField<em::DeviceLocalAccountInfoProto>& accounts =
      device_settings.device_local_accounts().account();
  RepeatedPtrField<em::DeviceLocalAccountInfoProto>::const_iterator entry;
  for (entry = accounts.begin(); entry != accounts.end(); ++entry) {
    if (entry->has_id()) {
      // Reuse the existing broker if present.
      DeviceLocalAccountPolicyBroker*& broker = policy_brokers_[entry->id()];
      DeviceLocalAccountPolicyBroker*& new_broker =
          new_policy_brokers[entry->id()];
      new_broker = broker;
      broker = NULL;

      // Fire up the cloud connection for fetching policy for the account from
      // the cloud if this is an enterprise-managed device.
      if (!new_broker || !new_broker->client()) {
        scoped_ptr<CloudPolicyClient> client(
            CreateClientForAccount(entry->id()));
        if (client.get()) {
          if (!new_broker)
            new_broker = CreateBroker(entry->id()).release();
          new_broker->Connect(client.Pass());
        }
      }
    }
  }
  policy_brokers_.swap(new_policy_brokers);
  DeleteBrokers(&new_policy_brokers);

  FOR_EACH_OBSERVER(Observer, observers_, OnDeviceLocalAccountsChanged());
}

scoped_ptr<DeviceLocalAccountPolicyBroker>
    DeviceLocalAccountPolicyService::CreateBroker(
        const std::string& account_id) {
  scoped_ptr<DeviceLocalAccountPolicyBroker> broker(
      new DeviceLocalAccountPolicyBroker(account_id, session_manager_client_,
                                         device_settings_service_));
  broker->store()->AddObserver(this);
  broker->store()->Load();
  return broker.Pass();
}

void DeviceLocalAccountPolicyService::DeleteBrokers(PolicyBrokerMap* map) {
  for (PolicyBrokerMap::iterator broker = map->begin(); broker != map->end();
       ++broker) {
    if (broker->second) {
      broker->second->store()->RemoveObserver(this);
      delete broker->second;
    }
  }
  map->clear();
}

DeviceLocalAccountPolicyBroker*
    DeviceLocalAccountPolicyService::GetBrokerForStore(
        CloudPolicyStore* store) {
  for (PolicyBrokerMap::iterator broker(policy_brokers_.begin());
       broker != policy_brokers_.end(); ++broker) {
    if (broker->second->store() == store)
      return broker->second;
  }
  return NULL;
}

scoped_ptr<CloudPolicyClient>
    DeviceLocalAccountPolicyService::CreateClientForAccount(
        const std::string& account_id) {
  const em::PolicyData* policy_data = device_settings_service_->policy_data();
  if (!policy_data ||
      !policy_data->has_request_token() ||
      !policy_data->has_device_id() ||
      !device_management_service_) {
    return scoped_ptr<CloudPolicyClient>();
  }

  scoped_ptr<CloudPolicyClient> client(
      new CloudPolicyClient(std::string(), std::string(),
                            USER_AFFILIATION_MANAGED,
                            CloudPolicyClient::POLICY_TYPE_PUBLIC_ACCOUNT,
                            NULL, device_management_service_));
  client->SetupRegistration(policy_data->request_token(),
                            policy_data->device_id());
  client->set_entity_id(account_id);
  return client.Pass();
}

}  // namespace policy
