// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_channel/cast_socket_service.h"
#include "base/test/mock_callback.h"
#include "components/cast_channel/cast_test_util.h"
#include "content/public/test/test_browser_thread_bundle.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::Return;
using testing::ReturnRef;
using testing::SaveArg;
using testing::WithArgs;

namespace cast_channel {

class CastSocketServiceTest : public testing::Test {
 public:
  CastSocketServiceTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        cast_socket_service_(new CastSocketService()) {}

  CastSocket* AddSocket(std::unique_ptr<CastSocket> socket) {
    return cast_socket_service_->AddSocket(std::move(socket));
  }

  void TearDown() override { cast_socket_service_ = nullptr; }

 protected:
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<CastSocketService> cast_socket_service_;
  base::MockCallback<CastSocket::OnOpenCallback> mock_on_open_callback_;
  MockCastSocketObserver mock_observer_;
};

TEST_F(CastSocketServiceTest, TestAddSocket) {
  auto socket1 = base::MakeUnique<MockCastSocket>();
  auto* socket_ptr1 = AddSocket(std::move(socket1));
  EXPECT_NE(0, socket_ptr1->id());

  auto socket2 = base::MakeUnique<MockCastSocket>();
  auto* socket_ptr2 = AddSocket(std::move(socket2));
  EXPECT_NE(socket_ptr1->id(), socket_ptr2->id());

  auto removed_socket = cast_socket_service_->RemoveSocket(socket_ptr2->id());
  EXPECT_EQ(socket_ptr2, removed_socket.get());

  auto socket3 = base::MakeUnique<MockCastSocket>();
  auto* socket_ptr3 = AddSocket(std::move(socket3));
  EXPECT_NE(socket_ptr1->id(), socket_ptr3->id());
  EXPECT_NE(socket_ptr2->id(), socket_ptr3->id());
}

TEST_F(CastSocketServiceTest, TestRemoveAndGetSocket) {
  int channel_id = 1;
  auto* socket_ptr = cast_socket_service_->GetSocket(channel_id);
  EXPECT_FALSE(socket_ptr);
  auto socket = cast_socket_service_->RemoveSocket(channel_id);
  EXPECT_FALSE(socket);

  auto mock_socket = base::MakeUnique<MockCastSocket>();

  auto* mock_socket_ptr = AddSocket(std::move(mock_socket));
  channel_id = mock_socket_ptr->id();
  EXPECT_EQ(mock_socket_ptr, cast_socket_service_->GetSocket(channel_id));

  socket = cast_socket_service_->RemoveSocket(channel_id);
  EXPECT_TRUE(socket);
}

TEST_F(CastSocketServiceTest, TestOpenChannel) {
  auto* mock_socket = new MockCastSocket();
  auto ip_endpoint = CreateIPEndPointForTest();
  mock_socket->SetIPEndpoint(ip_endpoint);
  cast_socket_service_->SetSocketForTest(base::WrapUnique(mock_socket));

  EXPECT_CALL(*mock_socket, ConnectInternal(_))
      .WillOnce(WithArgs<0>(
          Invoke([&](const MockCastSocket::MockOnOpenCallback& callback) {
            callback.Run(mock_socket->id(), ChannelError::NONE);
          })));
  EXPECT_CALL(mock_on_open_callback_, Run(_, ChannelError::NONE));
  EXPECT_CALL(*mock_socket, AddObserver(_));

  cast_socket_service_->OpenSocket(ip_endpoint, nullptr /* net_log */,
                                   mock_on_open_callback_.Get(),
                                   &mock_observer_);
}

}  // namespace cast_channel
