// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_FIDO_REQUEST_HANDLER_BASE_H_
#define DEVICE_FIDO_FIDO_REQUEST_HANDLER_BASE_H_

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece_forward.h"
#include "device/fido/fido_device_authenticator.h"
#include "device/fido/fido_discovery.h"
#include "device/fido/fido_transport_protocol.h"

namespace service_manager {
class Connector;
};  // namespace service_manager

namespace device {

class FidoAuthenticator;
class FidoDevice;
class FidoTask;

struct COMPONENT_EXPORT(DEVICE_FIDO) PlatformAuthenticatorInfo {
  PlatformAuthenticatorInfo(std::unique_ptr<FidoAuthenticator> authenticator,
                            bool has_recognized_mac_touch_id_credential);
  PlatformAuthenticatorInfo(PlatformAuthenticatorInfo&&);
  PlatformAuthenticatorInfo& operator=(PlatformAuthenticatorInfo&& other);
  ~PlatformAuthenticatorInfo();

  std::unique_ptr<FidoAuthenticator> authenticator;
  bool has_recognized_mac_touch_id_credential;
};

// Base class that handles device discovery/removal. Each FidoRequestHandlerBase
// is owned by FidoRequestManager and its lifetime is equivalent to that of a
// single WebAuthn request. For each authenticator, the per-device work is
// carried out by one FidoTask instance, which is constructed on DeviceAdded(),
// and destroyed either on DeviceRemoved() or CancelOutgoingTaks().
class COMPONENT_EXPORT(DEVICE_FIDO) FidoRequestHandlerBase
    : public FidoDiscovery::Observer {
 public:
  using AuthenticatorMap =
      std::map<std::string, std::unique_ptr<FidoAuthenticator>, std::less<>>;
  using RequestCallback = base::RepeatingCallback<void(const std::string&)>;

  enum class RequestType { kMakeCredential, kGetAssertion };

  // Encapsulates data required to initiate WebAuthN UX dialog. Once all
  // components of TransportAvailabilityInfo is set,
  // AuthenticatorRequestClientDelegate should be notified.
  // TODO(hongjunchoi): Add async calls to notify embedder when Bluetooth is
  // powered on/off.
  struct COMPONENT_EXPORT(DEVICE_FIDO) TransportAvailabilityInfo {
    TransportAvailabilityInfo();
    TransportAvailabilityInfo(const TransportAvailabilityInfo& other);
    TransportAvailabilityInfo& operator=(
        const TransportAvailabilityInfo& other);
    ~TransportAvailabilityInfo();

    // TODO(hongjunchoi): Factor |rp_id| and |request_type| from
    // TransportAvailabilityInfo.
    // See: https://crbug.com/875011
    std::string rp_id;
    RequestType request_type = RequestType::kMakeCredential;

    // The intersection of transports supported by the client and allowed by the
    // relying party.
    base::flat_set<FidoTransportProtocol> available_transports;

    bool has_recognized_mac_touch_id_credential = false;
    bool is_ble_powered = false;
    bool can_power_on_ble_adapter = false;
  };

  class COMPONENT_EXPORT(DEVICE_FIDO) TransportAvailabilityObserver {
   public:
    virtual ~TransportAvailabilityObserver();

    // This method will not be invoked until the observer is set.
    virtual void OnTransportAvailabilityEnumerated(
        TransportAvailabilityInfo data) = 0;
    virtual void BluetoothAdapterPowerChanged(bool is_powered_on) = 0;
    virtual void FidoAuthenticatorAdded(const FidoAuthenticator& authenticator,
                                        bool* hold_off_request) = 0;
    virtual void FidoAuthenticatorRemoved(base::StringPiece device_id) = 0;
  };

  // TODO(https://crbug.com/769631): Remove the dependency on Connector once
  // device/fido is servicified. The |available_transports| should be the
  // intersection of transports supported by the client and allowed by the
  // relying party.
  FidoRequestHandlerBase(
      service_manager::Connector* connector,
      const base::flat_set<FidoTransportProtocol>& available_transports);
  ~FidoRequestHandlerBase() override;

  // Triggers DispatchRequest() if |active_authenticators_| hold
  // FidoAuthenticator with given |authenticator_id|.
  void StartAuthenticatorRequest(const std::string& authenticator_id);

  // Triggers cancellation of all per-device FidoTasks, except for the device
  // with |exclude_device_id|, if one is provided. Cancelled tasks are
  // immediately removed from |ongoing_tasks_|.
  //
  // This function is invoked either when:
  //  (a) the entire WebAuthn API request is canceled or,
  //  (b) a successful response or "invalid state error" is received from the
  //  any one of the connected authenticators, in which case all other
  //  per-device tasks are cancelled.
  // https://w3c.github.io/webauthn/#iface-pkcredential
  void CancelOngoingTasks(base::StringPiece exclude_device_id = nullptr);

  base::WeakPtr<FidoRequestHandlerBase> GetWeakPtr();

  void set_observer(TransportAvailabilityObserver* observer) {
    DCHECK(!observer_) << "Only one observer is supported.";
    observer_ = observer;

    DCHECK(notify_observer_callback_);
    notify_observer_callback_.Run();
  }

  // Set the platform authenticator for this request, if one is available.
  // |AuthenticatorImpl| must call this method after invoking |set_oberver| even
  // if no platform authenticator is available, in which case it passes nullptr.
  void SetPlatformAuthenticatorOrMarkUnavailable(
      base::Optional<PlatformAuthenticatorInfo> platform_authenticator_info);

  TransportAvailabilityInfo& transport_availability_info() {
    return transport_availability_info_;
  }

 protected:
  // Subclasses implement this method to dispatch their request onto the given
  // FidoAuthenticator. The FidoAuthenticator is owned by this
  // FidoRequestHandler and stored in active_authenticators().
  virtual void DispatchRequest(FidoAuthenticator*) = 0;

  void Start();

  // Testing seam to allow unit tests to inject a fake authenticator.
  virtual std::unique_ptr<FidoDeviceAuthenticator>
  CreateAuthenticatorFromDevice(FidoDevice* device);

  AuthenticatorMap& active_authenticators() { return active_authenticators_; }
  std::vector<std::unique_ptr<FidoDiscovery>>& discoveries() {
    return discoveries_;
  }
  TransportAvailabilityObserver* observer() const { return observer_; }

 private:
  // FidoDiscovery::Observer
  void DiscoveryStarted(FidoDiscovery* discovery, bool success) final;
  void DeviceAdded(FidoDiscovery* discovery, FidoDevice* device) final;
  void DeviceRemoved(FidoDiscovery* discovery, FidoDevice* device) final;
  void BluetoothAdapterPowerChanged(bool is_powered_on) final;

  void AddAuthenticator(std::unique_ptr<FidoAuthenticator> authenticator);
  void NotifyObserverTransportAvailability();

  AuthenticatorMap active_authenticators_;
  std::vector<std::unique_ptr<FidoDiscovery>> discoveries_;
  TransportAvailabilityObserver* observer_ = nullptr;
  TransportAvailabilityInfo transport_availability_info_;
  base::RepeatingClosure notify_observer_callback_;

  base::WeakPtrFactory<FidoRequestHandlerBase> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(FidoRequestHandlerBase);
};

}  // namespace device

#endif  // DEVICE_FIDO_FIDO_REQUEST_HANDLER_BASE_H_
