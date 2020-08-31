// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_MESSAGE_RECEIVER_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_MESSAGE_RECEIVER_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel.mojom.h"

namespace chromeos {

namespace secure_channel {

// Test MessageReceiver implementation.
class FakeMessageReceiver : public mojom::MessageReceiver {
 public:
  FakeMessageReceiver();
  ~FakeMessageReceiver() override;

  const std::vector<std::string>& received_messages() {
    return received_messages_;
  }

 private:
  // mojom::MessageReceiver:
  void OnMessageReceived(const std::string& message) override;

  std::vector<std::string> received_messages_;

  DISALLOW_COPY_AND_ASSIGN(FakeMessageReceiver);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_MESSAGE_RECEIVER_H_
