// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_FIDO_REQUEST_HANDLER_H_
#define DEVICE_FIDO_FIDO_REQUEST_HANDLER_H_

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/strings/string_piece_forward.h"
#include "device/fido/fido_discovery.h"
#include "device/fido/u2f_transport_protocol.h"

namespace service_manager {
class Connector;
};  // namespace service_manager

namespace device {

class FidoDevice;
class FidoTask;

// Base class that handles device discovery/removal. Each FidoRequestHandler is
// owned by FidoRequestManager and its lifetime is equivalent to that of a
// single WebAuthn request. For each authenticator, the per-device work is
// carried out by one FidoTask instance, which is constructed on DeviceAdded(),
// and destroyed either on DeviceRemoved() or CancelOutgoingTaks().
class COMPONENT_EXPORT(DEVICE_FIDO) FidoRequestHandler
    : public FidoDiscovery::Observer {
 public:
  using TaskMap = std::map<std::string, std::unique_ptr<FidoTask>, std::less<>>;

  FidoRequestHandler(service_manager::Connector* connector,
                     const base::flat_set<U2fTransportProtocol>& transports);
  ~FidoRequestHandler() override;

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

  void set_is_complete(bool is_complete = true) { is_complete_ = is_complete; }
  bool is_complete() const { return is_complete_; }

 protected:
  // Creates a new FidoTask that asynchronously starts a device request.
  virtual std::unique_ptr<FidoTask> CreateTaskForNewDevice(FidoDevice*) = 0;

  TaskMap& ongoing_tasks() { return ongoing_tasks_; }

 private:
  // FidoDiscovery::Observer
  void DiscoveryStarted(FidoDiscovery* discovery, bool success) final;
  void DeviceAdded(FidoDiscovery* discovery, FidoDevice* device) final;
  void DeviceRemoved(FidoDiscovery* discovery, FidoDevice* device) final;

  // Set to true when the first user presence verified response has been
  // received from one of the connected authenticators. This guarantees that at
  // most one response is sent to the relying party.
  bool is_complete_ = false;
  TaskMap ongoing_tasks_;
  std::vector<std::unique_ptr<FidoDiscovery>> discoveries_;

  DISALLOW_COPY_AND_ASSIGN(FidoRequestHandler);
};

}  // namespace device

#endif  // DEVICE_FIDO_FIDO_REQUEST_HANDLER_H_
