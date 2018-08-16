// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/fido_request_handler_base.h"

#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "device/fido/fido_device.h"
#include "device/fido/fido_task.h"
#include "services/service_manager/public/cpp/connector.h"

namespace device {

namespace {

bool ShouldDeferRequestDispatchToUi(const FidoAuthenticator& authenticator) {
  // TODO(hongjunchoi): Change this to be dependent on authenticator transport
  // type once UI component is in place.
  return false;
}

}  // namespace

// FidoRequestHandlerBase::TransportAvailabilityInfo --------------------------

FidoRequestHandlerBase::TransportAvailabilityInfo::TransportAvailabilityInfo() =
    default;

FidoRequestHandlerBase::TransportAvailabilityInfo::TransportAvailabilityInfo(
    const TransportAvailabilityInfo& data) = default;

FidoRequestHandlerBase::TransportAvailabilityInfo&
FidoRequestHandlerBase::TransportAvailabilityInfo::operator=(
    const TransportAvailabilityInfo& other) = default;

FidoRequestHandlerBase::TransportAvailabilityInfo::
    ~TransportAvailabilityInfo() = default;

// FidoRequestHandlerBase::TransportAvailabilityObserver ----------------------

FidoRequestHandlerBase::TransportAvailabilityObserver::
    ~TransportAvailabilityObserver() = default;

// FidoRequestHandlerBase -----------------------------------------------------

FidoRequestHandlerBase::FidoRequestHandlerBase(
    service_manager::Connector* connector,
    const base::flat_set<FidoTransportProtocol>& available_transports)
    : FidoRequestHandlerBase(connector,
                             available_transports,
                             AddPlatformAuthenticatorCallback()) {}

FidoRequestHandlerBase::FidoRequestHandlerBase(
    service_manager::Connector* connector,
    const base::flat_set<FidoTransportProtocol>& available_transports,
    AddPlatformAuthenticatorCallback add_platform_authenticator)
    : add_platform_authenticator_(std::move(add_platform_authenticator)),
      weak_factory_(this) {
  // The number of times |notify_observer_callback_| needs to be invoked before
  // Observer::OnTransportAvailabilityEnumerated is dispatched. Essentially this
  // is used to wait until all the parts of |transport_availability_info_| are
  // filled out; the |notify_observer_callback_| is invoked once for each part
  // once that part is ready, namely:
  //
  //   1) Once the platform authenticator related fields are filled out.
  //   2) [Optionally, if BLE enabled] Once BLE related fields are filled out.
  //   3) [Optionally, if caBLE enabled] Once caBLE related fields are filled
  //   out.
  //
  // On top of that, we wait for (4) an invocation that happens when the
  // |observer_| is set, so that OnTransportAvailabilityEnumerated is never
  // called before the observer is set.
  size_t transport_info_callback_count = 1u + 0u + 0u + 1u;

  for (const auto transport : available_transports) {
    // Construction of CaBleDiscovery is handled by the implementing class as it
    // requires an extension passed on from the relying party.
    if (transport == FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy) {
      ++transport_info_callback_count;
      continue;
    }

    if (transport == FidoTransportProtocol::kInternal) {
      // Internal authenticators are injected through
      // AddPlatformAuthenticatorCallback.
      continue;
    }

    auto discovery = FidoDiscovery::Create(transport, connector);
    if (discovery == nullptr) {
      // This can occur in tests when a ScopedVirtualU2fDevice is in effect and
      // HID transports are not configured.
      continue;
    }

    if (transport == FidoTransportProtocol::kBluetoothLowEnergy)
      ++transport_info_callback_count;

    discovery->set_observer(this);
    discoveries_.push_back(std::move(discovery));
  }

  transport_availability_info_.available_transports = available_transports;
  notify_observer_callback_ = base::BarrierClosure(
      transport_info_callback_count,
      base::BindOnce(&FidoRequestHandlerBase::NotifyObserverUiData,
                     weak_factory_.GetWeakPtr()));
}

FidoRequestHandlerBase::~FidoRequestHandlerBase() = default;

void FidoRequestHandlerBase::CancelOngoingTasks(
    base::StringPiece exclude_device_id) {
  for (auto task_it = active_authenticators_.begin();
       task_it != active_authenticators_.end();) {
    DCHECK(!task_it->first.empty());
    if (task_it->first != exclude_device_id) {
      DCHECK(task_it->second);
      task_it->second->Cancel();
      task_it = active_authenticators_.erase(task_it);
    } else {
      ++task_it;
    }
  }
}

void FidoRequestHandlerBase::Start() {
  for (const auto& discovery : discoveries_)
    discovery->Start();

  MaybeAddPlatformAuthenticator();
}

void FidoRequestHandlerBase::DiscoveryStarted(FidoDiscovery* discovery,
                                              bool success) {
  if (discovery->transport() == FidoTransportProtocol::kBluetoothLowEnergy ||
      discovery->transport() ==
          FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy) {
    // For FidoBleDiscovery and FidoCableDiscovery, discovery is started with
    // |success| set to false if no BluetoothAdapter is present in the system.
    if (!success) {
      transport_availability_info_.available_transports.erase(
          discovery->transport());
    }

    DCHECK(notify_observer_callback_);
    notify_observer_callback_.Run();
  }
}

void FidoRequestHandlerBase::DeviceAdded(FidoDiscovery* discovery,
                                         FidoDevice* device) {
  DCHECK(!base::ContainsKey(active_authenticators(), device->GetId()));
  AddAuthenticator(CreateAuthenticatorFromDevice(device));
}

std::unique_ptr<FidoDeviceAuthenticator>
FidoRequestHandlerBase::CreateAuthenticatorFromDevice(FidoDevice* device) {
  return std::make_unique<FidoDeviceAuthenticator>(device);
}

void FidoRequestHandlerBase::DeviceRemoved(FidoDiscovery* discovery,
                                           FidoDevice* device) {
  // Device connection has been lost or device has already been removed.
  // Thus, calling CancelTask() is not necessary. Also, below
  // ongoing_tasks_.erase() will have no effect for the devices that have been
  // already removed due to processing error or due to invocation of
  // CancelOngoingTasks().
  DCHECK(device);
  active_authenticators_.erase(device->GetId());

  if (observer_)
    observer_->FidoAuthenticatorRemoved(device->GetId());
}

void FidoRequestHandlerBase::BluetoothAdapterPowerChanged(bool is_powered_on) {
  if (!observer_)
    return;

  observer_->BluetoothAdapterPowerChanged(is_powered_on);
}

void FidoRequestHandlerBase::AddAuthenticator(
    std::unique_ptr<FidoAuthenticator> authenticator) {
  DCHECK(authenticator &&
         !base::ContainsKey(active_authenticators(), authenticator->GetId()));
  FidoAuthenticator* authenticator_ptr = authenticator.get();
  active_authenticators_.emplace(authenticator->GetId(),
                                 std::move(authenticator));
  if (!ShouldDeferRequestDispatchToUi(*authenticator_ptr)) {
    // Post |DispatchRequest| into its own task. This avoids hairpinning, even
    // if the authenticator immediately invokes the request callback.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&FidoRequestHandlerBase::DispatchRequest,
                                  GetWeakPtr(), authenticator_ptr));
  }

  if (observer_)
    observer_->FidoAuthenticatorAdded(*authenticator_ptr);
}

void FidoRequestHandlerBase::MaybeAddPlatformAuthenticator() {
  std::unique_ptr<FidoAuthenticator> authenticator;
  if (add_platform_authenticator_ &&
      base::ContainsKey(transport_availability_info_.available_transports,
                        FidoTransportProtocol::kInternal)) {
    authenticator = std::move(add_platform_authenticator_).Run();
  }

  if (authenticator) {
    AddAuthenticator(std::move(authenticator));
  } else {
    transport_availability_info_.available_transports.erase(
        FidoTransportProtocol::kInternal);
  }

  DCHECK(notify_observer_callback_);
  notify_observer_callback_.Run();
}

void FidoRequestHandlerBase::NotifyObserverUiData() {
  DCHECK(observer_);

  observer_->OnTransportAvailabilityEnumerated(transport_availability_info_);
}

}  // namespace device
