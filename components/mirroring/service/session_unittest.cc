// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/session.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/simple_test_tick_clock.h"
#include "components/mirroring/service/fake_network_service.h"
#include "components/mirroring/service/fake_video_capture_host.h"
#include "components/mirroring/service/interface.h"
#include "media/cast/test/utility/default_config.h"
#include "media/cast/test/utility/net_utility.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InvokeWithoutArgs;
using ::testing::_;
using media::cast::FrameSenderConfig;
using media::cast::Packet;

namespace mirroring {

class SessionTest : public SessionClient, public ::testing::Test {
 public:
  SessionTest() : weak_factory_(this) {
    testing_clock_.Advance(base::TimeTicks::Now() - base::TimeTicks());
  }

  ~SessionTest() override { scoped_task_environment_.RunUntilIdle(); }

  // SessionClient implemenation.
  MOCK_METHOD1(OnError, void(SessionError));
  MOCK_METHOD0(DidStart, void());
  MOCK_METHOD0(DidStop, void());
  MOCK_METHOD0(OnOfferAnswerExchange, void());
  MOCK_METHOD0(OnGetVideoCaptureHost, void());
  MOCK_METHOD0(OnGetNetworkContext, void());

  void GetVideoCaptureHost(
      media::mojom::VideoCaptureHostRequest request) override {
    video_host_ = std::make_unique<FakeVideoCaptureHost>(std::move(request));
    OnGetVideoCaptureHost();
  }

  void GetNetWorkContext(
      network::mojom::NetworkContextRequest request) override {
    network_context_ = std::make_unique<MockNetworkContext>(std::move(request));
    OnGetNetworkContext();
  }

  void DoOfferAnswerExchange(
      const std::vector<FrameSenderConfig>& audio_configs,
      const std::vector<FrameSenderConfig>& video_configs,
      GetAnswerCallback callback) override {
    OnOfferAnswerExchange();
    std::move(callback).Run(FrameSenderConfig(),
                            media::cast::GetDefaultVideoSenderConfig());
  }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  base::SimpleTestTickClock testing_clock_;

  std::unique_ptr<Session> session_;
  std::unique_ptr<FakeVideoCaptureHost> video_host_;
  std::unique_ptr<MockNetworkContext> network_context_;

  base::WeakPtrFactory<SessionTest> weak_factory_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionTest);
};

TEST_F(SessionTest, Mirroring) {
  // Start a mirroring session.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(*this, OnGetVideoCaptureHost()).Times(1);
    EXPECT_CALL(*this, OnGetNetworkContext()).Times(1);
    EXPECT_CALL(*this, OnOfferAnswerExchange()).Times(1);
    EXPECT_CALL(*this, DidStart())
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    session_ =
        std::make_unique<Session>(SessionType::AUDIO_AND_VIDEO,
                                  media::cast::test::GetFreeLocalPort(), this);
    run_loop.Run();
  }

  scoped_task_environment_.RunUntilIdle();

  {
    base::RunLoop run_loop;
    // Expect to send out some UDP packets.
    EXPECT_CALL(*network_context_->udp_socket(), OnSend())
        .Times(testing::AtLeast(1));
    EXPECT_CALL(*video_host_, ReleaseBuffer(_, _, _))
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    // Send one video frame to the consumer.
    video_host_->SendOneFrame(gfx::Size(64, 32), testing_clock_.NowTicks());
    run_loop.Run();
  }

  scoped_task_environment_.RunUntilIdle();

  // Stop the session.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(*video_host_, OnStopped()).Times(1);
    EXPECT_CALL(*this, DidStop())
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    session_.reset();
    run_loop.Run();
  }
  scoped_task_environment_.RunUntilIdle();
}

}  // namespace mirroring
