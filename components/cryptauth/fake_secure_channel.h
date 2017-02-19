// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRYPTAUTH_FAKE_SECURE_CHANNEL_H_
#define COMPONENTS_CRYPTAUTH_FAKE_SECURE_CHANNEL_H_

#include "base/macros.h"
#include "components/cryptauth/secure_channel.h"

namespace cryptauth {

// A fake implementation of SecureChannel to use in tests.
class FakeSecureChannel : public SecureChannel {
 public:
  FakeSecureChannel(std::unique_ptr<Connection> connection,
                    std::unique_ptr<Delegate> delegate);
  ~FakeSecureChannel() override;

  struct SentMessage {
    SentMessage(const std::string& feature, const std::string& payload);

    std::string feature;
    std::string payload;
  };

  void ChangeStatus(const Status& new_status);
  void ReceiveMessage(const std::string& feature, const std::string& payload);

  std::vector<Observer*> observers() { return observers_; }

  std::vector<SentMessage> sent_messages() { return sent_messages_; }

  // SecureChannel:
  void Initialize() override;
  void SendMessage(const std::string& feature,
                   const std::string& payload) override;
  void Disconnect() override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

 private:
  std::vector<Observer*> observers_;
  std::vector<SentMessage> sent_messages_;

  DISALLOW_COPY_AND_ASSIGN(FakeSecureChannel);
};

}  // namespace cryptauth

#endif  // COMPONENTS_CRYPTAUTH_FAKE_SECURE_CHANNEL_H_
