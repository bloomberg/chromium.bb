// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAIRING_SHARK_CONNECTION_LISTENER_H_
#define COMPONENTS_PAIRING_SHARK_CONNECTION_LISTENER_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/pairing/host_pairing_controller.h"

namespace pairing_chromeos {

// Listens for incoming connection from shark controller. If connection
// is established, invokes callback passing HostPairingController
// as an argument.
class SharkConnectionListener : public HostPairingController::Observer {
 public:
  typedef base::Callback<void(scoped_ptr<HostPairingController>)>
      OnConnectedCallback;

  explicit SharkConnectionListener(OnConnectedCallback callback);
  ~SharkConnectionListener() override;

 private:
  typedef HostPairingController::Stage Stage;

  // HostPairingController::Observer overrides:
  void PairingStageChanged(Stage new_stage) override;

  OnConnectedCallback callback_;
  scoped_ptr<HostPairingController> controller_;

  DISALLOW_COPY_AND_ASSIGN(SharkConnectionListener);
};

}  // namespace pairing_chromeos

#endif  // COMPONENTS_PAIRING_SHARK_CONNECTION_LISTENER_H_
