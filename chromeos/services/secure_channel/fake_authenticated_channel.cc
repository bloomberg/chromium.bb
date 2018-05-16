// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/fake_authenticated_channel.h"

namespace chromeos {

namespace secure_channel {

FakeAuthenticatedChannel::FakeAuthenticatedChannel() : AuthenticatedChannel() {}

FakeAuthenticatedChannel::~FakeAuthenticatedChannel() = default;

void FakeAuthenticatedChannel::PerformSendMessage(
    const std::string& feature,
    const std::string& payload,
    base::OnceClosure on_sent_callback) {
  sent_messages_.push_back(
      std::make_tuple(feature, payload, std::move(on_sent_callback)));
}

FakeAuthenticatedChannelObserver::FakeAuthenticatedChannelObserver(
    const std::string& expected_channel_id)
    : expected_channel_id_(expected_channel_id) {}

FakeAuthenticatedChannelObserver::~FakeAuthenticatedChannelObserver() = default;

void FakeAuthenticatedChannelObserver::OnDisconnected(
    const std::string& channel_id) {
  DCHECK_EQ(expected_channel_id_, channel_id);
  has_been_notified_of_disconnection_ = true;
}

void FakeAuthenticatedChannelObserver::OnMessageReceived(
    const std::string& channel_id,
    const std::string& feature,
    const std::string& payload) {
  DCHECK_EQ(expected_channel_id_, channel_id);
  received_messages_.push_back(std::make_pair(feature, payload));
}

}  // namespace secure_channel

}  // namespace chromeos
