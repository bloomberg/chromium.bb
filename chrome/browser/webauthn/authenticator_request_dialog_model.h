// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_AUTHENTICATOR_REQUEST_DIALOG_MODEL_H_
#define CHROME_BROWSER_WEBAUTHN_AUTHENTICATOR_REQUEST_DIALOG_MODEL_H_

#include <string>
#include <vector>

#include "base/observer_list.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/webauthn/transport_list_model.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_transport_protocol.h"

// Encapsulates the model behind the Web Authentication request dialog's UX
// flow. This is essentially a state machine going through the states defined in
// the `Step` enumeration.
//
// Ultimately, this will become an observer of the AuthenticatorRequest, and
// contain the logic to figure out which steps the user needs to take, in which
// order, to complete the authentication flow.
class AuthenticatorRequestDialogModel {
 public:
  using TransportAvailabilityInfo =
      device::FidoRequestHandlerBase::TransportAvailabilityInfo;

  // Defines the potential steps of the Web Authentication API request UX flow.
  enum class Step {
    // The UX flow has not started yet, the dialog should still be hidden.
    kNotStarted,

    kWelcomeScreen,
    kTransportSelection,
    kErrorTimedOut,
    kCompleted,

    // Universal Serial Bus (USB).
    kUsbInsertAndActivate,

    // Bluetooth Low Energy (BLE).
    kBlePowerOnAutomatic,
    kBlePowerOnManual,

    kBlePairingBegin,
    kBleEnterPairingMode,
    kBleDeviceSelection,
    kBlePinEntry,

    kBleActivate,
    kBleVerifying,

    // Touch ID.
    kTouchId,

    // Phone as a security key.
    kCableActivate,
  };

  // Encapsulates information about authenticators that have been found but are
  // in inactive state because we want to dispatch the requests after receiving
  // confirmation from the user via the WebAuthN UI flow.
  struct AuthenticatorReference {
    AuthenticatorReference(base::StringPiece device_id,
                           device::FidoTransportProtocol transport);
    AuthenticatorReference(AuthenticatorReference&& data);
    AuthenticatorReference& operator=(AuthenticatorReference&& other);

    ~AuthenticatorReference();

    std::string device_id;
    device::FidoTransportProtocol transport;
  };

  // Implemented by the dialog to observe this model and show the UI panels
  // appropriate for the current step.
  class Observer {
   public:
    // Called just before the model is destructed.
    virtual void OnModelDestroyed() = 0;

    // Called when the UX flow has navigated to a different step, so the UI
    // should update.
    virtual void OnStepTransition() {}

    // Called when the user cancelled WebAuthN request by clicking the
    // "cancel" button or the back arrow in the UI dialog.
    virtual void OnCancelRequest() {}
  };

  AuthenticatorRequestDialogModel();
  ~AuthenticatorRequestDialogModel();

  void SetCurrentStep(Step step);
  Step current_step() const { return current_step_; }

  TransportListModel* transport_list_model() { return &transport_list_model_; }

  // Starts the UX flow, by either showing the welcome screen, the transport
  // selection screen, or the guided flow for them most likely transport.
  //
  // Valid action when at step: kNotStarted.
  void StartFlow(
      TransportAvailabilityInfo transport_availability,
      base::Optional<device::FidoTransportProtocol> last_used_transport);

  // Starts the UX flow. Tries to figure out the most likely transport to be
  // used, and starts the guided flow for that transport; or shows the manual
  // transport selection screen if the transport could not be uniquely
  // identified.
  //
  // Valid action when at step: kNotStarted, kWelcomeScreen.
  void StartGuidedFlowForMostLikelyTransportOrShowTransportSelection();

  // Requests that the step-by-step wizard flow commence, guiding the user
  // through using the Secutity Key with the given |transport|.
  //
  // Valid action when at step: kNotStarted, kWelcomeScreen,
  // kTransportSelection.
  void StartGuidedFlowForTransport(AuthenticatorTransport transport);

  // Tries if the BLE adapter is now powered -- the user claims they turned it
  // on.
  //
  // Valid action when at step: kBlePowerOnManual.
  void TryIfBleAdapterIsPowered();

  // Turns on the BLE adapter automatically.
  //
  // Valid action when at step: kBlePowerOnAutomatic.
  void PowerOnBleAdapter();

  // Lets the pairing procedure start after the user learned about the need.
  //
  // Valid action when at step: kBlePairingBegin.
  void StartBleDiscovery();

  // Initiates pairing of the device that the user has chosen.
  //
  // Valid action when at step: kBleDeviceSelection.
  void InitiatePairingDevice(const std::string& device_address);

  // Finishes pairing of the previously chosen device with the |pin| code
  // entered.
  //
  // Valid action when at step: kBlePinEntry.
  void FinishPairingWithPin(const base::string16& pin);

  // Tries if a USB device is present -- the user claims they plugged it in.
  //
  // Valid action when at step: kUsbInsert.
  void TryUsbDevice();

  // Tries to use Touch ID -- either because the request requires it or because
  // the user told us to.
  //
  // Valid action when at step: kTouchId.
  void TryTouchId();

  // Cancels the flow as a result of the user clicking `Cancel` on the UI.
  //
  // Valid action at all steps.
  void Cancel();

  // Backtracks in the flow as a result of the user clicking `Back` on the UI.
  //
  // Valid action at all steps.
  void Back();

  // The |observer| must either outlive the object, or unregister itself on its
  // destruction.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // To be called when the Web Authentication request is complete.
  void OnRequestComplete();

  // To be called when Web Authentication request times-out.
  void OnRequestTimeout();

  // To be called when the Bluetooth adapter powered state changes.
  void OnBluetoothPoweredStateChanged(bool powered);

  std::vector<AuthenticatorReference>& saved_authenticators() {
    return saved_authenticators_;
  }

 private:
  // The current step of the request UX flow that is currently shown.
  Step current_step_ = Step::kNotStarted;

  TransportListModel transport_list_model_;
  base::ObserverList<Observer> observers_;

  // These fields are only filled out when the UX flow is started.
  TransportAvailabilityInfo transport_availability_;
  base::Optional<device::FidoTransportProtocol> last_used_transport_;

  // Transport type and id of Mac TouchId and BLE authenticators are cached so
  // that the WebAuthN request for the corresponding authenticators can be
  // dispatched lazily after the user interacts with the UI element.
  std::vector<AuthenticatorReference> saved_authenticators_;

  DISALLOW_COPY_AND_ASSIGN(AuthenticatorRequestDialogModel);
};

#endif  // CHROME_BROWSER_WEBAUTHN_AUTHENTICATOR_REQUEST_DIALOG_MODEL_H_
