// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_CEC_SERVICE_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_CEC_SERVICE_CLIENT_H_

#include "base/macros.h"
#include "chromeos/dbus/cec_service_client.h"

namespace chromeos {

class CHROMEOS_EXPORT FakeCecServiceClient : public CecServiceClient {
 public:
  FakeCecServiceClient();
  ~FakeCecServiceClient() override;

  enum CurrentState { kUndefined, kStandBy, kAwake };

  // CecServiceClient
  void SendStandBy() override;
  void SendWakeUp() override;

  int stand_by_call_count();
  int wake_up_call_count();

  CurrentState current_state();

 protected:
  void Init(dbus::Bus* bus) override;

 private:
  int stand_by_call_count_ = 0;
  int wake_up_call_count_ = 0;
  CurrentState current_state_ = kUndefined;

  DISALLOW_COPY_AND_ASSIGN(FakeCecServiceClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_CEC_SERVICE_CLIENT_H_
