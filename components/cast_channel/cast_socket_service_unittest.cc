// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_channel/cast_socket_service.h"
#include "components/cast_channel/cast_test_util.h"
#include "content/public/test/test_browser_thread_bundle.h"

#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::SaveArg;

namespace cast_channel {

class CastSocketServiceTest : public testing::Test {
 public:
  CastSocketServiceTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        cast_socket_service_(new CastSocketService()) {}

 protected:
  content::TestBrowserThreadBundle thread_bundle_;
  scoped_refptr<CastSocketService> cast_socket_service_;
};

TEST_F(CastSocketServiceTest, TestAddSocket) {
  int channel_id_1 = 0;
  auto socket1 = base::MakeUnique<MockCastSocket>();
  EXPECT_CALL(*socket1, set_id(_)).WillOnce(SaveArg<0>(&channel_id_1));

  int channel_id = cast_socket_service_->AddSocket(std::move(socket1));
  EXPECT_EQ(channel_id_1, channel_id);
  EXPECT_NE(0, channel_id_1);

  int channel_id_2 = 0;
  auto socket2 = base::MakeUnique<MockCastSocket>();
  EXPECT_CALL(*socket2, set_id(_)).WillOnce(SaveArg<0>(&channel_id_2));

  auto* socket_ptr = socket2.get();
  channel_id = cast_socket_service_->AddSocket(std::move(socket2));
  EXPECT_EQ(channel_id_2, channel_id);
  EXPECT_NE(channel_id_1, channel_id_2);

  auto removed_socket = cast_socket_service_->RemoveSocket(channel_id);
  EXPECT_EQ(socket_ptr, removed_socket.get());

  int channel_id_3 = 0;
  auto socket3 = base::MakeUnique<MockCastSocket>();
  EXPECT_CALL(*socket3, set_id(_)).WillOnce(SaveArg<0>(&channel_id_3));

  channel_id = cast_socket_service_->AddSocket(std::move(socket3));
  EXPECT_EQ(channel_id_3, channel_id);
  EXPECT_NE(channel_id_1, channel_id_3);
  EXPECT_NE(channel_id_2, channel_id_3);
}

TEST_F(CastSocketServiceTest, TestRemoveAndGetSocket) {
  int channel_id = 1;
  auto* socket_ptr = cast_socket_service_->GetSocket(channel_id);
  EXPECT_FALSE(socket_ptr);
  auto socket = cast_socket_service_->RemoveSocket(channel_id);
  EXPECT_FALSE(socket);

  auto mock_socket = base::MakeUnique<MockCastSocket>();
  EXPECT_CALL(*mock_socket, set_id(_));

  auto* mock_socket_ptr = mock_socket.get();
  channel_id = cast_socket_service_->AddSocket(std::move(mock_socket));
  EXPECT_EQ(mock_socket_ptr, cast_socket_service_->GetSocket(channel_id));
  socket = cast_socket_service_->RemoveSocket(channel_id);
  EXPECT_TRUE(socket);
}

}  // namespace cast_channel
