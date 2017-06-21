// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_CHANNEL_CAST_TEST_UTIL_H_
#define COMPONENTS_CAST_CHANNEL_CAST_TEST_UTIL_H_

#include <string>
#include <utility>

#include "base/macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/cast_channel/cast_socket.h"
#include "components/cast_channel/cast_transport.h"
#include "components/cast_channel/proto/cast_channel.pb.h"
#include "net/base/ip_endpoint.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace cast_channel {

class MockCastTransport : public CastTransport {
 public:
  MockCastTransport();
  ~MockCastTransport() override;

  void SetReadDelegate(
      std::unique_ptr<CastTransport::Delegate> delegate) override;

  MOCK_METHOD2(SendMessage,
               void(const CastMessage& message,
                    const net::CompletionCallback& callback));

  MOCK_METHOD0(Start, void(void));

  // Gets the read delegate that is currently active for this transport.
  CastTransport::Delegate* current_delegate() const;

 private:
  std::unique_ptr<CastTransport::Delegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(MockCastTransport);
};

class MockCastTransportDelegate : public CastTransport::Delegate {
 public:
  MockCastTransportDelegate();
  ~MockCastTransportDelegate() override;

  MOCK_METHOD1(OnError, void(ChannelError error));
  MOCK_METHOD1(OnMessage, void(const CastMessage& message));
  MOCK_METHOD0(Start, void(void));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCastTransportDelegate);
};

class MockCastSocketObserver : public CastSocket::Observer {
 public:
  MockCastSocketObserver();
  ~MockCastSocketObserver() override;

  MOCK_METHOD2(OnError, void(const CastSocket& socket, ChannelError error));
  MOCK_METHOD2(OnMessage,
               void(const CastSocket& socket, const CastMessage& message));
};

class MockCastSocket : public CastSocket {
 public:
  MockCastSocket();
  ~MockCastSocket() override;

  MOCK_METHOD1(Connect, void(base::Callback<void(ChannelError)> callback));
  MOCK_METHOD1(Close, void(const net::CompletionCallback& callback));
  MOCK_CONST_METHOD0(ip_endpoint, const net::IPEndPoint&());
  MOCK_CONST_METHOD0(id, int());
  MOCK_METHOD1(set_id, void(int id));
  MOCK_CONST_METHOD0(ready_state, ReadyState());
  MOCK_CONST_METHOD0(error_state, ChannelError());
  MOCK_CONST_METHOD0(keep_alive, bool(void));
  MOCK_CONST_METHOD0(audio_only, bool(void));
  MOCK_METHOD1(SetErrorState, void(ChannelError error_state));
  MOCK_METHOD1(AddObserver, void(Observer* observer));

  CastTransport* transport() const override { return mock_transport_.get(); }

  MockCastTransport* mock_transport() const { return mock_transport_.get(); }

 private:
  std::unique_ptr<MockCastTransport> mock_transport_;
  std::unique_ptr<Observer> observer_;

  DISALLOW_COPY_AND_ASSIGN(MockCastSocket);
};

// Creates the IPEndpoint 192.168.1.1.
net::IPEndPoint CreateIPEndPointForTest();

// Checks if two proto messages are the same.
// From
// third_party/cacheinvalidation/overrides/google/cacheinvalidation/deps/gmock.h
// TODO(kmarshall): promote to a shared testing library.
MATCHER_P(EqualsProto, message, "") {
  std::string expected_serialized, actual_serialized;
  message.SerializeToString(&expected_serialized);
  arg.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

ACTION_TEMPLATE(PostCompletionCallbackTask,
                HAS_1_TEMPLATE_PARAMS(int, cb_idx),
                AND_1_VALUE_PARAMS(rv)) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(testing::get<cb_idx>(args), rv));
}

}  // namespace cast_channel

#endif  // COMPONENTS_CAST_CHANNEL_CAST_TEST_UTIL_H_
