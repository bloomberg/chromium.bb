// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_CONNECTION_DELEGATE_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_CONNECTION_DELEGATE_H_

#include "base/macros.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"

namespace chromeos {

namespace secure_channel {

// Test ConnectionDelegate implementation.
class FakeConnectionDelegate : public mojom::ConnectionDelegate {
 public:
  FakeConnectionDelegate();
  ~FakeConnectionDelegate() override;

  mojom::ConnectionDelegatePtr GenerateInterfacePtr();
  void DisconnectGeneratedPtrs();

  const base::Optional<mojom::ConnectionAttemptFailureReason>&
  connection_attempt_failure_reason() const {
    return connection_attempt_failure_reason_;
  }

 private:
  // mojom::ConnectionDelegate:
  void OnConnectionAttemptFailure(
      mojom::ConnectionAttemptFailureReason reason) override;

  mojo::BindingSet<mojom::ConnectionDelegate> bindings_;

  base::Optional<mojom::ConnectionAttemptFailureReason>
      connection_attempt_failure_reason_;

  DISALLOW_COPY_AND_ASSIGN(FakeConnectionDelegate);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_CONNECTION_DELEGATE_H_
