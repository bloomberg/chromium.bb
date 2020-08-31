// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_FAKE_CONNECTION_PRESERVER_H_
#define CHROMEOS_COMPONENTS_TETHER_FAKE_CONNECTION_PRESERVER_H_

#include <string>

#include "base/macros.h"
#include "chromeos/components/tether/connection_preserver.h"

namespace chromeos {

namespace tether {

// Test double for ConnectionPreserver.
class FakeConnectionPreserver : public ConnectionPreserver {
 public:
  FakeConnectionPreserver();
  ~FakeConnectionPreserver() override;

  void HandleSuccessfulTetherAvailabilityResponse(
      const std::string& device_id) override;

  const std::string& last_requested_preserved_connection_device_id() {
    return last_requested_preserved_connection_device_id_;
  }

 private:
  std::string last_requested_preserved_connection_device_id_;

  DISALLOW_COPY_AND_ASSIGN(FakeConnectionPreserver);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_FAKE_CONNECTION_PRESERVER_H_
