// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/base/fake_base_task.h"
#include "jingle/notifier/base/weak_xmpp_client.h"
#include "talk/xmpp/asyncsocket.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace notifier {

using ::testing::_;
using ::testing::Return;

class MockAsyncSocket : public buzz::AsyncSocket {
 public:
  virtual ~MockAsyncSocket() {}

  MOCK_METHOD0(state, State());
  MOCK_METHOD0(error, Error());
  MOCK_METHOD0(GetError, int());
  MOCK_METHOD1(Connect, bool(const talk_base::SocketAddress&));
  MOCK_METHOD3(Read, bool(char*, size_t, size_t*));
  MOCK_METHOD2(Write, bool(const char*, size_t));
  MOCK_METHOD0(Close, bool());
  MOCK_METHOD1(StartTls, bool(const std::string&));
};

FakeBaseTask::FakeBaseTask() {
  // Owned by |task_pump_|.
  notifier::WeakXmppClient* weak_xmpp_client =
      new notifier::WeakXmppClient(&task_pump_);

  weak_xmpp_client->Start();
  buzz::XmppClientSettings settings;
  // Owned by |weak_xmpp_client|.
  MockAsyncSocket* mock_async_socket = new MockAsyncSocket();
  EXPECT_CALL(*mock_async_socket, Connect(_)).WillOnce(Return(true));
  weak_xmpp_client->Connect(settings, "en", mock_async_socket, NULL);
  // Initialize the XMPP client.
  task_pump_.RunTasks();

  base_task_ = weak_xmpp_client->AsWeakPtr();
}

FakeBaseTask::~FakeBaseTask() {}

base::WeakPtr<talk_base::Task> FakeBaseTask::AsWeakPtr() {
  return base_task_;
}

}  // namespace notifier
