// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_FAKE_TETHER_CONNECTOR_H_
#define CHROMEOS_COMPONENTS_TETHER_FAKE_TETHER_CONNECTOR_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "chromeos/components/tether/tether_connector.h"
#include "chromeos/network/network_connection_handler.h"

namespace chromeos {

namespace tether {

// Test double for TetherConnector.
class FakeTetherConnector : public TetherConnector {
 public:
  FakeTetherConnector();
  ~FakeTetherConnector() override;

  std::string last_connected_tether_network_guid() {
    return last_connected_tether_network_guid_;
  }

  void set_connection_error_name(const std::string& connection_error_name) {
    connection_error_name_ = connection_error_name;
  }

  std::string last_canceled_tether_network_guid() {
    return last_canceled_tether_network_guid_;
  }

  void set_should_cancel_successfully(bool should_cancel_successfully) {
    should_cancel_successfully_ = should_cancel_successfully;
  }

  // TetherConnector:
  void ConnectToNetwork(
      const std::string& tether_network_guid,
      const base::Closure& success_callback,
      const network_handler::StringResultCallback& error_callback) override;
  bool CancelConnectionAttempt(const std::string& tether_network_guid) override;

 private:
  std::string last_connected_tether_network_guid_;
  std::string connection_error_name_;

  std::string last_canceled_tether_network_guid_;
  bool should_cancel_successfully_;

  DISALLOW_COPY_AND_ASSIGN(FakeTetherConnector);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_FAKE_TETHER_CONNECTOR_H_
