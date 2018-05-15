// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_CONNECTION_ATTEMPT_DELEGATE_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_CONNECTION_ATTEMPT_DELEGATE_H_

#include <string>

#include "base/macros.h"
#include "chromeos/services/secure_channel/connection_attempt_delegate.h"

namespace cryptauth {
class SecureChannel;
}  // namespace cryptauth

namespace chromeos {

namespace secure_channel {

class FakeConnectionAttemptDelegate : public ConnectionAttemptDelegate {
 public:
  FakeConnectionAttemptDelegate();
  ~FakeConnectionAttemptDelegate() override;

  const cryptauth::SecureChannel* secure_channel() const {
    return secure_channel_.get();
  }

  const std::string& attempt_id() const { return attempt_id_; }

 private:
  // ConnectionAttemptDelegate:
  void OnConnectionAttemptSucceeded(
      const std::string& attempt_id,
      std::unique_ptr<cryptauth::SecureChannel> secure_channel) override;
  void OnConnectionAttemptFinishedWithoutConnection(
      const std::string& attempt_id) override;

  std::string attempt_id_;
  std::unique_ptr<cryptauth::SecureChannel> secure_channel_;

  DISALLOW_COPY_AND_ASSIGN(FakeConnectionAttemptDelegate);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_CONNECTION_ATTEMPT_DELEGATE_H_
