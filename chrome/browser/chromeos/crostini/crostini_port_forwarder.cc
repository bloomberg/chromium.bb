// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>

#include "chrome/browser/chromeos/crostini/crostini_port_forwarder.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/no_destructor.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/dbus/permission_broker/permission_broker_client.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace crostini {

// Currently, we are not supporting ethernet/mlan/usb port forwarding.
const char kDefaultInterfaceToForward[] = "all";
const char kWlanInterface[] = "wlan0";
const char kPortNumberKey[] = "port_number";
const char kPortProtocolKey[] = "protocol_type";
const char kPortInterfaceKey[] = "input_ifname";
const char kPortLabelKey[] = "label";
const char kPortVmNameKey[] = "vm_name";
const char kPortContainerNameKey[] = "container_name";

class CrostiniPortForwarderFactory : public BrowserContextKeyedServiceFactory {
 public:
  static CrostiniPortForwarder* GetForProfile(Profile* profile) {
    return static_cast<CrostiniPortForwarder*>(
        GetInstance()->GetServiceForBrowserContext(profile, true));
  }

  static CrostiniPortForwarderFactory* GetInstance() {
    static base::NoDestructor<CrostiniPortForwarderFactory> factory;
    return factory.get();
  }

 private:
  friend class base::NoDestructor<CrostiniPortForwarderFactory>;

  CrostiniPortForwarderFactory()
      : BrowserContextKeyedServiceFactory(
            "CrostiniPortForwarderService",
            BrowserContextDependencyManager::GetInstance()) {}

  ~CrostiniPortForwarderFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override {
    Profile* profile = Profile::FromBrowserContext(context);
    return new CrostiniPortForwarder(profile);
  }
};

CrostiniPortForwarder* CrostiniPortForwarder::GetForProfile(Profile* profile) {
  return CrostiniPortForwarderFactory::GetForProfile(profile);
}

CrostiniPortForwarder::CrostiniPortForwarder(Profile* profile)
    : profile_(profile) {}

CrostiniPortForwarder::~CrostiniPortForwarder() = default;

void CrostiniPortForwarder::SignalActivePortsChanged() {
  for (auto& observer : observers_) {
    observer.OnActivePortsChanged(GetActivePorts());
  }
}

bool CrostiniPortForwarder::MatchPortRuleDict(const base::Value& dict,
                                              const PortRuleKey& key) {
  base::Optional<int> port_number = dict.FindIntKey(kPortNumberKey);
  base::Optional<int> protocol_type = dict.FindIntKey(kPortProtocolKey);
  const std::string* input_ifname = dict.FindStringKey(kPortInterfaceKey);
  const std::string* vm_name = dict.FindStringKey(kPortVmNameKey);
  const std::string* container_name = dict.FindStringKey(kPortContainerNameKey);
  return (port_number && port_number.value() == key.port_number) &&
         (protocol_type &&
          protocol_type.value() == static_cast<int>(key.protocol_type)) &&
         (input_ifname && *input_ifname == key.input_ifname) &&
         (vm_name && *vm_name == key.container_id.vm_name) &&
         (container_name && *container_name == key.container_id.container_name);
}

bool CrostiniPortForwarder::MatchPortRuleContainerId(
    const base::Value& dict,
    const ContainerId& container_id) {
  const std::string* vm_name = dict.FindStringKey(kPortVmNameKey);
  const std::string* container_name = dict.FindStringKey(kPortContainerNameKey);
  return (vm_name && *vm_name == container_id.vm_name) &&
         (container_name && *container_name == container_id.container_name);
}

void CrostiniPortForwarder::AddNewPortPreference(const PortRuleKey& key,
                                                 const std::string& label) {
  PrefService* pref_service = profile_->GetPrefs();
  ListPrefUpdate update(pref_service, crostini::prefs::kCrostiniPortForwarding);
  base::ListValue* all_ports = update.Get();
  base::Value new_port_metadata(base::Value::Type::DICTIONARY);
  new_port_metadata.SetIntKey(kPortNumberKey, key.port_number);
  new_port_metadata.SetIntKey(kPortProtocolKey,
                              static_cast<int>(key.protocol_type));
  new_port_metadata.SetStringKey(kPortInterfaceKey, kDefaultInterfaceToForward);
  new_port_metadata.SetStringKey(kPortLabelKey, label);
  new_port_metadata.SetStringKey(kPortVmNameKey, key.container_id.vm_name);
  new_port_metadata.SetStringKey(kPortContainerNameKey,
                                 key.container_id.container_name);
  all_ports->Append(std::move(new_port_metadata));
}

bool CrostiniPortForwarder::RemovePortPreference(const PortRuleKey& key) {
  PrefService* pref_service = profile_->GetPrefs();
  ListPrefUpdate update(pref_service, crostini::prefs::kCrostiniPortForwarding);
  base::ListValue* all_ports = update.Get();
  base::Value::ListView list_view = all_ports->GetList();
  auto it = std::find_if(
      list_view.begin(), list_view.end(),
      [&key, this](const auto& dict) { return MatchPortRuleDict(dict, key); });

  return all_ports->EraseListIter(it);
}

base::Optional<base::Value> CrostiniPortForwarder::ReadPortPreference(
    const PortRuleKey& key) {
  PrefService* pref_service = profile_->GetPrefs();
  const base::ListValue* all_ports =
      pref_service->GetList(crostini::prefs::kCrostiniPortForwarding);
  auto it = std::find_if(
      all_ports->begin(), all_ports->end(),
      [&key, this](const auto& dict) { return MatchPortRuleDict(dict, key); });
  if (it == all_ports->end()) {
    return base::nullopt;
  }
  return base::Optional<base::Value>(it->Clone());
}

void CrostiniPortForwarder::OnAddOrActivatePortCompleted(
    ResultCallback result_callback,
    PortRuleKey key,
    bool success) {
  if (!success) {
    forwarded_ports_.erase(key);
  }
  std::move(result_callback).Run(success);
  SignalActivePortsChanged();
}

void CrostiniPortForwarder::OnRemoveOrDeactivatePortCompleted(
    ResultCallback result_callback,
    PortRuleKey key,
    bool success) {
  forwarded_ports_.erase(key);
  std::move(result_callback).Run(success);
  SignalActivePortsChanged();
}

void CrostiniPortForwarder::TryActivatePort(
    const PortRuleKey& key,
    const ContainerId& container_id,
    base::OnceCallback<void(bool)> result_callback) {
  auto info = CrostiniManager::GetForProfile(profile_)->GetContainerInfo(
      container_id.vm_name, container_id.container_name);
  if (!info) {
    LOG(ERROR) << "Inactive container to make port rules for.";
    std::move(result_callback).Run(false);
    return;
  }

  chromeos::PermissionBrokerClient* client =
      chromeos::PermissionBrokerClient::Get();
  if (!client) {
    LOG(ERROR) << "Could not get permission broker client.";
    std::move(result_callback).Run(false);
    return;
  }

  int lifeline[2] = {-1, -1};
  if (pipe(lifeline) < 0) {
    LOG(ERROR) << "Failed to create a lifeline pipe";
    std::move(result_callback).Run(false);
    return;
  }

  base::ScopedFD lifeline_local(lifeline[0]);
  base::ScopedFD lifeline_remote(lifeline[1]);

  forwarded_ports_[key] = std::move(lifeline_local);

  // TODO(matterchen): Determining how to request all interfaces dynamically.
  switch (key.protocol_type) {
    case Protocol::TCP:
      client->RequestTcpPortForward(
          key.port_number, kWlanInterface, info->ipv4_address, key.port_number,
          lifeline_remote.get(), std::move(result_callback));
      break;
    case Protocol::UDP:
      client->RequestUdpPortForward(
          key.port_number, kWlanInterface, info->ipv4_address, key.port_number,
          lifeline_remote.get(), std::move(result_callback));
      break;
  }
}

void CrostiniPortForwarder::TryDeactivatePort(
    const PortRuleKey& key,
    const ContainerId& container_id,
    base::OnceCallback<void(bool)> result_callback) {
  auto info = CrostiniManager::GetForProfile(profile_)->GetContainerInfo(
      container_id.vm_name, container_id.container_name);
  if (!info) {
    LOG(ERROR) << "Inactive container to make port rules for.";
    std::move(result_callback).Run(false);
    return;
  }

  if (forwarded_ports_.find(key) == forwarded_ports_.end()) {
    LOG(ERROR) << "Port is already inactive.";
    std::move(result_callback).Run(false);
    return;
  }

  chromeos::PermissionBrokerClient* client =
      chromeos::PermissionBrokerClient::Get();
  if (!client) {
    LOG(ERROR) << "Could not get permission broker client.";
    std::move(result_callback).Run(false);
    return;
  }

  // TODO(matterchen): Determining how to release all interfaces.
  switch (key.protocol_type) {
    case Protocol::TCP:
      client->ReleaseTcpPortForward(key.port_number, kWlanInterface,
                                    std::move(result_callback));
      break;
    case Protocol::UDP:
      client->ReleaseUdpPortForward(key.port_number, kWlanInterface,
                                    std::move(result_callback));
  }
}

void CrostiniPortForwarder::AddPort(const ContainerId& container_id,
                                    uint16_t port_number,
                                    const Protocol& protocol_type,
                                    const std::string& label,
                                    ResultCallback result_callback) {
  PortRuleKey new_port_key = {
      .port_number = port_number,
      .protocol_type = protocol_type,
      .input_ifname = kDefaultInterfaceToForward,
      .container_id = container_id,
  };

  if (ReadPortPreference(new_port_key)) {
    LOG(ERROR) << "Trying to add port which already exists.";
    std::move(result_callback).Run(false);
    return;
  }
  AddNewPortPreference(new_port_key, label);
  base::OnceCallback<void(bool)> on_add_port_completed = base::BindOnce(
      &CrostiniPortForwarder::OnAddOrActivatePortCompleted,
      weak_ptr_factory_.GetWeakPtr(), std::move(result_callback), new_port_key);
  TryActivatePort(new_port_key, container_id, std::move(on_add_port_completed));
}

void CrostiniPortForwarder::ActivatePort(const ContainerId& container_id,
                                         uint16_t port_number,
                                         const Protocol& protocol_type,
                                         ResultCallback result_callback) {
  PortRuleKey existing_port_key = {
      .port_number = port_number,
      .protocol_type = protocol_type,
      .input_ifname = kDefaultInterfaceToForward,
      .container_id = container_id,
  };

  if (!ReadPortPreference(existing_port_key)) {
    LOG(ERROR) << "Trying to activate port not found in preferences.";
    std::move(result_callback).Run(false);
    return;
  }
  if (forwarded_ports_.find(existing_port_key) != forwarded_ports_.end()) {
    LOG(ERROR) << "Trying to activate already active port.";
    std::move(result_callback).Run(false);
    return;
  }

  base::OnceCallback<void(bool)> on_activate_port_completed =
      base::BindOnce(&CrostiniPortForwarder::OnAddOrActivatePortCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(result_callback),
                     existing_port_key);

  CrostiniPortForwarder::TryActivatePort(existing_port_key, container_id,
                                         std::move(on_activate_port_completed));
}

void CrostiniPortForwarder::DeactivatePort(const ContainerId& container_id,
                                           uint16_t port_number,
                                           const Protocol& protocol_type,
                                           ResultCallback result_callback) {
  PortRuleKey existing_port_key = {
      .port_number = port_number,
      .protocol_type = protocol_type,
      .input_ifname = kDefaultInterfaceToForward,
      .container_id = container_id,
  };

  if (!ReadPortPreference(existing_port_key)) {
    LOG(ERROR) << "Trying to deactivate port not found in preferences.";
    std::move(result_callback).Run(false);
    return;
  }
  base::OnceCallback<void(bool)> on_deactivate_port_completed =
      base::BindOnce(&CrostiniPortForwarder::OnRemoveOrDeactivatePortCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(result_callback),
                     existing_port_key);

  CrostiniPortForwarder::TryDeactivatePort(
      existing_port_key, container_id, std::move(on_deactivate_port_completed));
}

void CrostiniPortForwarder::RemovePort(const ContainerId& container_id,
                                       uint16_t port_number,
                                       const Protocol& protocol_type,
                                       ResultCallback result_callback) {
  PortRuleKey existing_port_key = {
      .port_number = port_number,
      .protocol_type = protocol_type,
      .input_ifname = kDefaultInterfaceToForward,
      .container_id = container_id,
  };

  if (!RemovePortPreference(existing_port_key)) {
    LOG(ERROR) << "Trying to remove port not found in preferences.";
    std::move(result_callback).Run(false);
    return;
  }
  base::OnceCallback<void(bool)> on_remove_port_completed =
      base::BindOnce(&CrostiniPortForwarder::OnRemoveOrDeactivatePortCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(result_callback),
                     existing_port_key);

  CrostiniPortForwarder::TryDeactivatePort(existing_port_key, container_id,
                                           std::move(on_remove_port_completed));
}

void CrostiniPortForwarder::DeactivateAllActivePorts(
    const ContainerId& container_id) {
  auto it = forwarded_ports_.begin();
  while (it != forwarded_ports_.end()) {
    if (it->first.container_id == container_id) {
      TryDeactivatePort(it->first, container_id, base::DoNothing());
      it = forwarded_ports_.erase(it);
    } else {
      ++it;
    }
  }
  SignalActivePortsChanged();
}

void CrostiniPortForwarder::RemoveAllPorts(const ContainerId& container_id) {
  PrefService* pref_service = profile_->GetPrefs();
  ListPrefUpdate update(pref_service, crostini::prefs::kCrostiniPortForwarding);
  update->EraseListValueIf([&container_id, this](const auto& dict) {
    return MatchPortRuleContainerId(dict, container_id);
  });

  DeactivateAllActivePorts(container_id);
}

base::ListValue CrostiniPortForwarder::GetActivePorts() {
  base::ListValue forwarded_ports_list;
  for (const auto& port : forwarded_ports_) {
    base::Value port_info(base::Value::Type::DICTIONARY);
    port_info.SetKey("port_number", base::Value(port.first.port_number));
    port_info.SetKey("protocol_type",
                     base::Value(static_cast<int>(port.first.protocol_type)));
    forwarded_ports_list.Append(std::move(port_info));
  }
  return forwarded_ports_list;
}

size_t CrostiniPortForwarder::GetNumberOfForwardedPortsForTesting() {
  return forwarded_ports_.size();
}

base::Optional<base::Value> CrostiniPortForwarder::ReadPortPreferenceForTesting(
    const PortRuleKey& key) {
  return ReadPortPreference(key);
}

}  // namespace crostini
