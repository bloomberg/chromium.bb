// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/message_loop.h"
#include "chrome/browser/sync/notifier/chrome_invalidation_client.h"
#include "chrome/browser/sync/notifier/state_writer.h"
#include "jingle/notifier/base/fake_base_task.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_notifier {

using ::testing::_;
using ::testing::Return;

class MockListener : public ChromeInvalidationClient::Listener {
 public:
  MOCK_METHOD1(OnInvalidate, void(syncable::ModelType));
  MOCK_METHOD0(OnInvalidateAll, void());
};

class MockStateWriter : public StateWriter {
 public:
  MOCK_METHOD1(WriteState, void(const std::string&));
};

class MockInvalidationClient : public invalidation::InvalidationClient {
 public:
  MOCK_METHOD1(Start, void(const std::string& str));
  MOCK_METHOD1(Register, void(const invalidation::ObjectId&));
  MOCK_METHOD1(Unregister, void(const invalidation::ObjectId&));
  MOCK_METHOD0(network_endpoint, invalidation::NetworkEndpoint*());
};

namespace {
const char kClientId[] = "client_id";
const char kClientInfo[] = "client_info";
const char kState[] = "state";
}  // namespace

class ChromeInvalidationClientTest : public testing::Test {
 protected:
  virtual void SetUp() {
    client_.Start(kClientId, kClientInfo, kState,
                  &mock_listener_, &mock_state_writer_,
                  fake_base_task_.AsWeakPtr());
  }

  virtual void TearDown() {
    client_.Stop();
    message_loop_.RunAllPending();
  }

  // Simulates DoInformOutboundListener() from network-manager.cc.
  virtual void SimulateInformOutboundListener() {
    // Explicitness hack here to work around broken callback
    // implementations.
    void (invalidation::NetworkCallback::*run_function)(
        invalidation::NetworkEndpoint* const&) =
        &invalidation::NetworkCallback::Run;

    client_.chrome_system_resources_.ScheduleOnListenerThread(
        invalidation::NewPermanentCallback(
            client_.handle_outbound_packet_callback_.get(), run_function,
            client_.invalidation_client_->network_endpoint()));
  }

  MessageLoop message_loop_;
  MockListener mock_listener_;
  MockStateWriter mock_state_writer_;
  notifier::FakeBaseTask fake_base_task_;
  ChromeInvalidationClient client_;
};

// Outbound packet sending should be resilient to
// changing/disappearing base tasks.
TEST_F(ChromeInvalidationClientTest, OutboundPackets) {
  SimulateInformOutboundListener();

  notifier::FakeBaseTask fake_base_task;
  client_.ChangeBaseTask(fake_base_task.AsWeakPtr());

  SimulateInformOutboundListener();

  {
    notifier::FakeBaseTask fake_base_task2;
    client_.ChangeBaseTask(fake_base_task2.AsWeakPtr());
  }

  SimulateInformOutboundListener();
}

// TODO(akalin): Flesh out unit tests.

}  // namespace sync_notifier
