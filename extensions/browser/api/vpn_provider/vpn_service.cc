// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/vpn_provider/vpn_service.h"

#include <stdint.h>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chromeos/dbus/shill_third_party_vpn_driver_client.h"
#include "chromeos/dbus/shill_third_party_vpn_observer.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_configuration_observer.h"
#include "chromeos/network/network_profile.h"
#include "chromeos/network/network_profile_handler.h"
#include "crypto/sha2.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

namespace api_vpn = extensions::core_api::vpn_provider;

const int kInvalidHandle = -1;

std::string GetKey(const std::string& extension_id, const std::string& name) {
  const std::string key = crypto::SHA256HashString(extension_id + name);
  return base::HexEncode(key.data(), key.size());
}

void DoNothingFailureCallback(const std::string& error_name,
                              const std::string& error_message) {
  LOG(ERROR) << error_name << ": " << error_message;
}

}  // namespace

class VpnService::VpnConfiguration : public ShillThirdPartyVpnObserver {
 public:
  VpnConfiguration(const std::string& extension_id,
                   const std::string& name,
                   int handle,
                   base::WeakPtr<VpnService> vpn_service);
  ~VpnConfiguration();

  const std::string& extension_id() const { return extension_id_; }
  const std::string& name() const { return name_; }
  const std::string& key() const { return key_; }
  const int handle() const { return handle_; }
  const std::string& service_path() const { return service_path_; }
  void set_service_path(const std::string& service_path) {
    service_path_ = service_path;
  }
  const std::string& object_path() const { return object_path_; }

  // ShillThirdPartyVpnObserver:
  // TODO(kaliamoorthi): Replace |data| and |length| with a string value.
  void OnPacketReceived(const uint8_t* data, size_t length) override;
  void OnPlatformMessage(uint32_t message) override;

 private:
  const std::string extension_id_;
  const std::string name_;
  const std::string key_;
  const std::string object_path_;

  const int handle_;

  std::string service_path_;

  base::WeakPtr<VpnService> vpn_service_;

  DISALLOW_COPY_AND_ASSIGN(VpnConfiguration);
};

VpnService::VpnConfiguration::VpnConfiguration(
    const std::string& extension_id,
    const std::string& name,
    int handle,
    base::WeakPtr<VpnService> vpn_service)
    : extension_id_(extension_id),
      name_(name),
      key_(GetKey(extension_id, name)),
      object_path_(shill::kObjectPathBase + key_),
      handle_(handle),
      vpn_service_(vpn_service) {
  CHECK_LE(0, handle_);
}

VpnService::VpnConfiguration::~VpnConfiguration() {
}

void VpnService::VpnConfiguration::OnPacketReceived(const uint8_t* data,
                                                    size_t length) {
  if (!vpn_service_) {
    return;
  }
  scoped_ptr<base::ListValue> event_args = api_vpn::OnPacketReceived::Create(
      handle_, std::string(reinterpret_cast<const char*>(data), length));
  vpn_service_->SendSignalToExtension(
      extension_id_, api_vpn::OnPacketReceived::kEventName, event_args.Pass());
}

void VpnService::VpnConfiguration::OnPlatformMessage(uint32_t message) {
  if (!vpn_service_) {
    return;
  }
  DCHECK_GE(api_vpn::PLATFORM_MESSAGE_LAST, message);
  scoped_ptr<base::ListValue> event_args = api_vpn::OnPlatformMessage::Create(
      handle_, static_cast<api_vpn::PlatformMessage>(message));
  vpn_service_->SendSignalToExtension(
      extension_id_, api_vpn::OnPlatformMessage::kEventName, event_args.Pass());
}

VpnService::VpnService(
    content::BrowserContext* browser_context,
    extensions::ExtensionRegistry* extension_registry,
    extensions::EventRouter* event_router,
    ShillThirdPartyVpnDriverClient* shill_client,
    NetworkConfigurationHandler* network_configuration_handler,
    NetworkProfileHandler* network_profile_handler)
    : browser_context_(browser_context),
      next_handle_(0),
      extension_registry_(extension_registry),
      event_router_(event_router),
      shill_client_(shill_client),
      network_configuration_handler_(network_configuration_handler),
      network_profile_handler_(network_profile_handler),
      weak_factory_(this) {
  extension_registry_->AddObserver(this);
}

VpnService::~VpnService() {
  extension_registry_->RemoveObserver(this);
  STLDeleteContainerPairSecondPointers(handle_to_configuration_map_.begin(),
                                       handle_to_configuration_map_.end());
}

void VpnService::CreateConfiguration(const std::string& extension_id,
                                     const std::string& name,
                                     const SuccessCallbackWithHandle& success,
                                     const FailureCallback& failure) {
  if (name.empty()) {
    failure.Run(std::string(), std::string("Empty name not supported."));
    return;
  }

  if (ContainsKey(key_to_configuration_map_, GetKey(extension_id, name))) {
    failure.Run(std::string(), std::string("Name not unique."));
    return;
  }

  // TODO(kaliamoorthi): GetDefaultUserProfile is unsafe. Start using
  // GetProfileForUserhash after sorting out the dependency issues.
  const NetworkProfile* profile =
      network_profile_handler_->GetDefaultUserProfile();
  if (!profile) {
    failure.Run(
        std::string(),
        std::string("No user profile for unshared network configuration."));
    return;
  }

  const int handle = GetNewHandle();
  if (handle == kInvalidHandle) {
    failure.Run(std::string(), std::string("Ran out of handles."));
    return;
  }

  VpnConfiguration* configuration =
      CreateConfigurationInternal(extension_id, name, handle);

  base::DictionaryValue properties;
  properties.SetStringWithoutPathExpansion(shill::kTypeProperty,
                                           shill::kTypeVPN);
  properties.SetStringWithoutPathExpansion(shill::kNameProperty, name);
  properties.SetStringWithoutPathExpansion(shill::kProviderHostProperty,
                                           extension_id);
  properties.SetStringWithoutPathExpansion(shill::kObjectPathSuffixProperty,
                                           configuration->key());
  properties.SetStringWithoutPathExpansion(shill::kProviderTypeProperty,
                                           shill::kProviderThirdPartyVpn);
  properties.SetStringWithoutPathExpansion(shill::kProfileProperty,
                                           profile->path);

  network_configuration_handler_->CreateConfiguration(
      properties, NetworkConfigurationObserver::SOURCE_EXTENSION_INSTALL,
      base::Bind(&VpnService::OnCreateConfigurationSuccess,
                 weak_factory_.GetWeakPtr(), success, configuration),
      base::Bind(&VpnService::OnCreateConfigurationFailure,
                 weak_factory_.GetWeakPtr(), failure, configuration));
}

void VpnService::DestroyConfiguration(const std::string& extension_id,
                                      int handle,
                                      const SuccessCallback& success,
                                      const FailureCallback& failure) {
  if (!IsAuthorizedAndConfigurationExists(handle, extension_id)) {
    failure.Run(std::string(), std::string("Unauthorized access."));
    return;
  }

  VpnConfiguration* configuration = GetConfigurationByHandle(handle);

  shill_client_->RemoveShillThirdPartyVpnObserver(configuration->object_path());

  network_configuration_handler_->RemoveConfiguration(
      configuration->service_path(),
      NetworkConfigurationObserver::SOURCE_EXTENSION_INSTALL,
      base::Bind(&VpnService::OnRemoveConfigurationSuccess,
                 weak_factory_.GetWeakPtr(), success),
      base::Bind(&VpnService::OnRemoveConfigurationFailure,
                 weak_factory_.GetWeakPtr(), failure));

  DestroyConfigurationInternal(configuration);
}

void VpnService::SetParameters(const std::string& extension_id,
                               int handle,
                               const base::DictionaryValue& parameters,
                               const SuccessCallback& success,
                               const FailureCallback& failure) {
  if (!IsAuthorizedAndConfigurationExists(handle, extension_id)) {
    failure.Run(std::string(), std::string("Unauthorized access."));
    return;
  }

  VpnConfiguration* configuration = GetConfigurationByHandle(handle);
  shill_client_->SetParameters(configuration->object_path(), parameters,
                               success, failure);
}

void VpnService::SendPacket(const std::string& extension_id,
                            int handle,
                            const std::string& data,
                            const SuccessCallback& success,
                            const FailureCallback& failure) {
  if (!IsAuthorizedAndConfigurationExists(handle, extension_id)) {
    failure.Run(std::string(), std::string("Unauthorized access."));
    return;
  }

  if (data.empty()) {
    failure.Run(std::string(), std::string("Can't send an empty packet."));
    return;
  }

  VpnConfiguration* configuration = GetConfigurationByHandle(handle);
  shill_client_->SendPacket(configuration->object_path(), data, success,
                            failure);
}

void VpnService::NotifyConnectionStateChanged(const std::string& extension_id,
                                              int handle,
                                              api_vpn::VpnConnectionState state,
                                              const SuccessCallback& success,
                                              const FailureCallback& failure) {
  if (!IsAuthorizedAndConfigurationExists(handle, extension_id)) {
    failure.Run(std::string(), std::string("Unauthorized access."));
    return;
  }

  VpnConfiguration* configuration = GetConfigurationByHandle(handle);
  shill_client_->UpdateConnectionState(configuration->object_path(),
                                       static_cast<uint32_t>(state), success,
                                       failure);
}

void VpnService::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UninstallReason reason) {
  if (browser_context != browser_context_) {
    NOTREACHED();
    return;
  }

  std::vector<VpnConfiguration*> to_be_destroyed;
  for (const auto& iter : handle_to_configuration_map_) {
    if (iter.second->extension_id() == extension->id()) {
      to_be_destroyed.push_back(iter.second);
    }
  }

  for (auto& iter : to_be_destroyed) {
    DestroyConfiguration(extension->id(), iter->handle(),
                         base::Bind(base::DoNothing),
                         base::Bind(DoNothingFailureCallback));
  }
}

void VpnService::OnCreateConfigurationSuccess(
    const VpnService::SuccessCallbackWithHandle& callback,
    VpnConfiguration* configuration,
    const std::string& service_path) {
  configuration->set_service_path(service_path);
  shill_client_->AddShillThirdPartyVpnObserver(configuration->object_path(),
                                               configuration);
  callback.Run(configuration->handle());
}

void VpnService::OnCreateConfigurationFailure(
    const VpnService::FailureCallback& callback,
    VpnConfiguration* configuration,
    const std::string& error_name,
    scoped_ptr<base::DictionaryValue> error_data) {
  DestroyConfigurationInternal(configuration);
  callback.Run(error_name, std::string());
}

void VpnService::OnRemoveConfigurationSuccess(
    const VpnService::SuccessCallback& callback) {
  callback.Run();
}

void VpnService::OnRemoveConfigurationFailure(
    const VpnService::FailureCallback& callback,
    const std::string& error_name,
    scoped_ptr<base::DictionaryValue> error_data) {
  callback.Run(error_name, std::string());
}

void VpnService::SendSignalToExtension(const std::string& extension_id,
                                       const std::string& event_name,
                                       scoped_ptr<base::ListValue> event_args) {
  scoped_ptr<extensions::Event> event(
      new extensions::Event(event_name, event_args.Pass(), browser_context_));

  event_router_->DispatchEventToExtension(extension_id, event.Pass());
}

VpnService::VpnConfiguration* VpnService::CreateConfigurationInternal(
    const std::string& extension_id,
    const std::string& name,
    int handle) {
  VpnConfiguration* configuration = new VpnConfiguration(
      extension_id, name, handle, weak_factory_.GetWeakPtr());
  key_to_configuration_map_[configuration->key()] = configuration;
  // The object is owned by handle_to_configuration_map_ henceforth.
  handle_to_configuration_map_[configuration->handle()] = configuration;
  return configuration;
}

void VpnService::DestroyConfigurationInternal(VpnConfiguration* configuration) {
  key_to_configuration_map_.erase(configuration->key());
  handle_to_configuration_map_.erase(configuration->handle());
  delete configuration;
}

int VpnService::PostIncrementHandle() {
  const int handle = next_handle_;
  next_handle_ = ((next_handle_ + 1) < 0) ? 0 : (next_handle_ + 1);
  return handle;
}

int VpnService::GetNewHandle() {
  const int first_handle = next_handle_;
  CHECK_LE(0, next_handle_);

  // Find next handle
  while (GetConfigurationByHandle(next_handle_)) {
    PostIncrementHandle();

    // Wrap around return kInvalidHandle since no new handles were found.
    if (first_handle == next_handle_)
      return kInvalidHandle;
  }

  return PostIncrementHandle();
}

bool VpnService::IsAuthorizedAndConfigurationExists(
    int handle,
    const std::string& extension_id) {
  const VpnConfiguration* vpn_configuration = GetConfigurationByHandle(handle);
  return vpn_configuration && vpn_configuration->extension_id() == extension_id;
}

VpnService::VpnConfiguration* VpnService::GetConfigurationByHandle(int handle) {
  const HandleToConfigurationMap::iterator it =
      handle_to_configuration_map_.find(handle);
  return (it != handle_to_configuration_map_.end()) ? it->second : nullptr;
}

}  // namespace chromeos
