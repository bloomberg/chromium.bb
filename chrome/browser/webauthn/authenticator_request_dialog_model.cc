// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"

#include <utility>

#include "base/stl_util.h"

namespace {

// Attempts to auto-select the most likely transport that will be used to
// service this request, or returns base::nullopt if unsure.
base::Optional<device::FidoTransportProtocol> SelectMostLikelyTransport(
    device::FidoRequestHandlerBase::TransportAvailabilityInfo
        transport_availability,
    base::Optional<device::FidoTransportProtocol> last_used_transport) {
  // If the KeyChain contains one of the |allowedCredentials|, then we are
  // certain we can service the request using Touch ID, as long as allowed by
  // the RP, so go for the certain choice here.
  if (transport_availability.has_recognized_mac_touch_id_credential &&
      base::ContainsKey(transport_availability.available_transports,
                        device::FidoTransportProtocol::kInternal)) {
    return device::FidoTransportProtocol::kInternal;
  }

  // For GetAssertion call, if the |last_used_transport| is available, use that.
  if (transport_availability.request_type ==
          device::FidoRequestHandlerBase::RequestType::kGetAssertion &&
      last_used_transport &&
      base::ContainsKey(transport_availability.available_transports,
                        *last_used_transport)) {
    return *last_used_transport;
  }

  // If there is only one transport available we can use, select that, instead
  // of showing a transport selection screen with only a single transport.
  if (transport_availability.available_transports.size() == 1) {
    return *transport_availability.available_transports.begin();
  }

  return base::nullopt;
}

AuthenticatorTransport ToAuthenticatorTransport(
    device::FidoTransportProtocol transport) {
  switch (transport) {
    case device::FidoTransportProtocol::kUsbHumanInterfaceDevice:
      return AuthenticatorTransport::kUsb;
    case device::FidoTransportProtocol::kNearFieldCommunication:
      return AuthenticatorTransport::kNearFieldCommunication;
    case device::FidoTransportProtocol::kBluetoothLowEnergy:
      return AuthenticatorTransport::kBluetoothLowEnergy;
    case device::FidoTransportProtocol::kInternal:
      return AuthenticatorTransport::kInternal;
    case device::FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy:
      return AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy;
  }
  NOTREACHED();
  return AuthenticatorTransport::kUsb;
}

}  // namespace

// AuthenticatorRequestDialogModel::AuthenticatorReference --------------------

AuthenticatorRequestDialogModel::AuthenticatorReference::AuthenticatorReference(
    base::StringPiece authenticator_id,
    device::FidoTransportProtocol transport)
    : authenticator_id(authenticator_id), transport(transport) {}
AuthenticatorRequestDialogModel::AuthenticatorReference::AuthenticatorReference(
    AuthenticatorReference&& data) = default;
AuthenticatorRequestDialogModel::AuthenticatorReference&
AuthenticatorRequestDialogModel::AuthenticatorReference::operator=(
    AuthenticatorReference&& other) = default;
AuthenticatorRequestDialogModel::AuthenticatorReference::
    ~AuthenticatorReference() = default;

// AuthenticatorRequestDialogModel --------------------------------------------

AuthenticatorRequestDialogModel::AuthenticatorRequestDialogModel() {}
AuthenticatorRequestDialogModel::~AuthenticatorRequestDialogModel() {
  for (auto& observer : observers_)
    observer.OnModelDestroyed();
}

void AuthenticatorRequestDialogModel::SetCurrentStep(Step step) {
  current_step_ = step;
  for (auto& observer : observers_)
    observer.OnStepTransition();
}

void AuthenticatorRequestDialogModel::StartFlow(
    TransportAvailabilityInfo transport_availability,
    base::Optional<device::FidoTransportProtocol> last_used_transport) {
  DCHECK_EQ(current_step(), Step::kNotStarted);

  transport_availability_ = std::move(transport_availability);
  last_used_transport_ = last_used_transport;
  for (const auto transport : transport_availability_.available_transports) {
    transport_list_model_.AppendTransport(ToAuthenticatorTransport(transport));
  }

  if (last_used_transport) {
    StartGuidedFlowForMostLikelyTransportOrShowTransportSelection();
  } else {
    SetCurrentStep(Step::kWelcomeScreen);
  }
}

void AuthenticatorRequestDialogModel::
    StartGuidedFlowForMostLikelyTransportOrShowTransportSelection() {
  DCHECK(current_step() == Step::kWelcomeScreen ||
         current_step() == Step::kNotStarted);
  auto most_likely_transport =
      SelectMostLikelyTransport(transport_availability_, last_used_transport_);
  if (most_likely_transport) {
    StartGuidedFlowForTransport(
        ToAuthenticatorTransport(*most_likely_transport));
  } else if (!transport_availability_.available_transports.empty()) {
    DCHECK_GE(transport_availability_.available_transports.size(), 2u);
    SetCurrentStep(Step::kTransportSelection);
  } else {
    SetCurrentStep(Step::kErrorNoAvailableTransports);
  }
}

void AuthenticatorRequestDialogModel::StartGuidedFlowForTransport(
    AuthenticatorTransport transport) {
  DCHECK(current_step() == Step::kTransportSelection ||
         current_step() == Step::kWelcomeScreen ||
         current_step() == Step::kNotStarted);
  switch (transport) {
    case AuthenticatorTransport::kUsb:
      SetCurrentStep(Step::kUsbInsertAndActivate);
      break;
    case AuthenticatorTransport::kNearFieldCommunication:
      SetCurrentStep(Step::kTransportSelection);
      break;
    case AuthenticatorTransport::kInternal:
      TryTouchId();
      break;
    case AuthenticatorTransport::kBluetoothLowEnergy:
      SetCurrentStep(Step::kBleActivate);
      break;
    case AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy:
      SetCurrentStep(Step::kCableActivate);
      break;
    default:
      break;
  }
}

void AuthenticatorRequestDialogModel::TryIfBleAdapterIsPowered() {
  DCHECK_EQ(current_step(), Step::kBlePowerOnManual);
}

void AuthenticatorRequestDialogModel::PowerOnBleAdapter() {
  DCHECK_EQ(current_step(), Step::kBlePowerOnAutomatic);
}

void AuthenticatorRequestDialogModel::StartBleDiscovery() {
  DCHECK_EQ(current_step(), Step::kBlePairingBegin);
}

void AuthenticatorRequestDialogModel::InitiatePairingDevice(
    const std::string& device_address) {
  DCHECK_EQ(current_step(), Step::kBleDeviceSelection);
}

void AuthenticatorRequestDialogModel::FinishPairingWithPin(
    const base::string16& pin) {
  DCHECK_EQ(current_step(), Step::kBlePinEntry);
}

void AuthenticatorRequestDialogModel::TryUsbDevice() {
  DCHECK_EQ(current_step(), Step::kUsbInsertAndActivate);
}

void AuthenticatorRequestDialogModel::TryTouchId() {
  SetCurrentStep(Step::kTouchId);
  if (!request_callback_)
    return;

  auto touch_id_authenticator =
      std::find_if(saved_authenticators_.begin(), saved_authenticators_.end(),
                   [](const auto& authenticator) {
                     return authenticator.transport ==
                            device::FidoTransportProtocol::kInternal;
                   });

  if (touch_id_authenticator == saved_authenticators_.end())
    return;

  request_callback_.Run(touch_id_authenticator->authenticator_id);
}

void AuthenticatorRequestDialogModel::Cancel() {
  for (auto& observer : observers_)
    observer.OnCancelRequest();
}

void AuthenticatorRequestDialogModel::Back() {
  if (current_step() == Step::kWelcomeScreen) {
    Cancel();
  } else if (current_step() == Step::kTransportSelection) {
    SetCurrentStep(Step::kWelcomeScreen);
  } else {
    SetCurrentStep(transport_availability_.available_transports.size() >= 2u
                       ? Step::kTransportSelection
                       : Step::kWelcomeScreen);
  }
}

void AuthenticatorRequestDialogModel::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AuthenticatorRequestDialogModel::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void AuthenticatorRequestDialogModel::OnRequestComplete() {
  SetCurrentStep(Step::kCompleted);
}

void AuthenticatorRequestDialogModel::OnRequestTimeout() {
  SetCurrentStep(Step::kErrorTimedOut);
}

void AuthenticatorRequestDialogModel::OnBluetoothPoweredStateChanged(
    bool powered) {
  transport_availability_.is_ble_powered = powered;
}

void AuthenticatorRequestDialogModel::SetRequestCallback(
    device::FidoRequestHandlerBase::RequestCallback request_callback) {
  request_callback_ = request_callback;
}
