// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"

#include <utility>

#include "base/bind.h"
#include "base/stl_util.h"
#include "base/threading/sequenced_task_runner_handle.h"

namespace {

// Attempts to auto-select the most likely transport that will be used to
// service this request, or returns base::nullopt if unsure.
base::Optional<device::FidoTransportProtocol> SelectMostLikelyTransport(
    device::FidoRequestHandlerBase::TransportAvailabilityInfo
        transport_availability,
    base::Optional<device::FidoTransportProtocol> last_used_transport) {
  if (transport_availability.request_type ==
          device::FidoRequestHandlerBase::RequestType::kGetAssertion &&
      base::ContainsKey(transport_availability.available_transports,
                        device::FidoTransportProtocol::kInternal)) {
    // For GetAssertion requests, auto advance to Touch ID if the keychain
    // contains one of the allowedCredentials.
    if (transport_availability.has_recognized_mac_touch_id_credential) {
      return device::FidoTransportProtocol::kInternal;
    }
    // Otherwise make sure we never return Touch ID (unless there is no other
    // option, in which case we auto advance to a special error screen).
    if (transport_availability.available_transports.size() == 1) {
      return device::FidoTransportProtocol::kInternal;
    }
    transport_availability.available_transports.erase(
        device::FidoTransportProtocol::kInternal);
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
    transport_list_model_.AppendTransport(transport);
  }

  StartGuidedFlowForMostLikelyTransportOrShowTransportSelection();
}

void AuthenticatorRequestDialogModel::
    StartGuidedFlowForMostLikelyTransportOrShowTransportSelection() {
  DCHECK(current_step() == Step::kWelcomeScreen ||
         current_step() == Step::kNotStarted);
  auto most_likely_transport =
      SelectMostLikelyTransport(transport_availability_, last_used_transport_);
  if (most_likely_transport) {
    StartGuidedFlowForTransport(*most_likely_transport);
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
         current_step() == Step::kUsbInsertAndActivate ||
         current_step() == Step::kTouchId ||
         current_step() == Step::kBleActivate ||
         current_step() == Step::kCableActivate ||
         current_step() == Step::kNotStarted);
  switch (transport) {
    case AuthenticatorTransport::kUsbHumanInterfaceDevice:
      SetCurrentStep(Step::kUsbInsertAndActivate);
      break;
    case AuthenticatorTransport::kNearFieldCommunication:
      SetCurrentStep(Step::kTransportSelection);
      break;
    case AuthenticatorTransport::kInternal:
      StartTouchIdFlow();
      break;
    case AuthenticatorTransport::kBluetoothLowEnergy:
      EnsureBleAdapterIsPoweredBeforeContinuingWithStep(Step::kBleActivate);
      break;
    case AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy:
      EnsureBleAdapterIsPoweredBeforeContinuingWithStep(Step::kCableActivate);
      break;
    default:
      break;
  }
}

void AuthenticatorRequestDialogModel::
    EnsureBleAdapterIsPoweredBeforeContinuingWithStep(Step next_step) {
  DCHECK(current_step() == Step::kTransportSelection ||
         current_step() == Step::kWelcomeScreen ||
         current_step() == Step::kUsbInsertAndActivate ||
         current_step() == Step::kTouchId ||
         current_step() == Step::kBleActivate ||
         current_step() == Step::kCableActivate ||
         current_step() == Step::kNotStarted);
  if (ble_adapter_is_powered()) {
    SetCurrentStep(next_step);
  } else {
    next_step_once_ble_powered_ = next_step;
    if (transport_availability()->can_power_on_ble_adapter)
      SetCurrentStep(Step::kBlePowerOnAutomatic);
    else
      SetCurrentStep(Step::kBlePowerOnManual);
  }
}

void AuthenticatorRequestDialogModel::ContinueWithFlowAfterBleAdapterPowered() {
  DCHECK(current_step() == Step::kBlePowerOnManual ||
         current_step() == Step::kBlePowerOnAutomatic);
  DCHECK(ble_adapter_is_powered());
  DCHECK(next_step_once_ble_powered_.has_value());

  SetCurrentStep(*next_step_once_ble_powered_);
}

void AuthenticatorRequestDialogModel::PowerOnBleAdapter() {
  DCHECK_EQ(current_step(), Step::kBlePowerOnAutomatic);
  if (!bluetooth_adapter_power_on_callback_)
    return;

  bluetooth_adapter_power_on_callback_.Run();
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

void AuthenticatorRequestDialogModel::StartTouchIdFlow() {
  // Never try Touch ID if the request is known in advance to fail. Proceed to
  // a special error screen instead.
  if (transport_availability_.request_type ==
          device::FidoRequestHandlerBase::RequestType::kGetAssertion &&
      !transport_availability_.has_recognized_mac_touch_id_credential) {
    SetCurrentStep(Step::kErrorInternalUnrecognized);
    return;
  }

  SetCurrentStep(Step::kTouchId);

  auto touch_id_authenticator_it =
      std::find_if(saved_authenticators_.begin(), saved_authenticators_.end(),
                   [](const auto& authenticator) {
                     return authenticator.transport ==
                            device::FidoTransportProtocol::kInternal;
                   });

  if (touch_id_authenticator_it == saved_authenticators_.end())
    return;

  static base::TimeDelta kTouchIdDispatchDelay =
      base::TimeDelta::FromMilliseconds(1250);
  DispatchRequestAsync(&*touch_id_authenticator_it, kTouchIdDispatchDelay);
}

void AuthenticatorRequestDialogModel::Cancel() {
  if (is_request_complete()) {
    SetCurrentStep(Step::kClosed);
    return;
  }

  for (auto& observer : observers_)
    observer.OnCancelRequest();
}

void AuthenticatorRequestDialogModel::Back() {
  SetCurrentStep(Step::kTransportSelection);
}

void AuthenticatorRequestDialogModel::OnSheetModelDidChange() {
  for (auto& observer : observers_)
    observer.OnSheetModelChanged();
}

void AuthenticatorRequestDialogModel::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AuthenticatorRequestDialogModel::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void AuthenticatorRequestDialogModel::OnRequestComplete() {
  DCHECK_NE(current_step(), Step::kClosed);
  if (is_showing_post_mortem())
    return;
  SetCurrentStep(Step::kClosed);
}

void AuthenticatorRequestDialogModel::OnRequestTimeout() {
  DCHECK(!is_request_complete());
  SetCurrentStep(Step::kPostMortemTimedOut);
}

void AuthenticatorRequestDialogModel::OnActivatedKeyNotRegistered() {
  DCHECK(!is_request_complete());
  SetCurrentStep(Step::kPostMortemKeyNotRegistered);
}

void AuthenticatorRequestDialogModel::OnActivatedKeyAlreadyRegistered() {
  DCHECK(!is_request_complete());
  SetCurrentStep(Step::kPostMortemKeyAlreadyRegistered);
}

void AuthenticatorRequestDialogModel::OnBluetoothPoweredStateChanged(
    bool powered) {
  transport_availability_.is_ble_powered = powered;

  for (auto& observer : observers_)
    observer.OnBluetoothPoweredStateChanged();

  // For the manual flow, the user has to click the "next" button explicitly.
  if (current_step() == Step::kBlePowerOnAutomatic)
    ContinueWithFlowAfterBleAdapterPowered();
}

void AuthenticatorRequestDialogModel::SetRequestCallback(
    RequestCallback request_callback) {
  request_callback_ = request_callback;
}

void AuthenticatorRequestDialogModel::SetBluetoothAdapterPowerOnCallback(
    base::RepeatingClosure bluetooth_adapter_power_on_callback) {
  bluetooth_adapter_power_on_callback_ = bluetooth_adapter_power_on_callback;
}

void AuthenticatorRequestDialogModel::DispatchRequestAsync(
    AuthenticatorReference* authenticator,
    base::TimeDelta delay) {
  if (!request_callback_)
    return;

  // Dispatching to the same authenticator twice may result in unexpected
  // behavior.
  if (authenticator->dispatched)
    return;

  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(request_callback_, authenticator->authenticator_id),
      delay);
  authenticator->dispatched = true;
}
