// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/cast_app_discovery_service.h"

#include "base/test/scoped_task_environment.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_simple_task_runner.h"
#include "chrome/browser/media/router/test/test_helper.h"
#include "chrome/common/media_router/discovery/media_sink_service_base.h"
#include "chrome/common/media_router/providers/cast/cast_media_source.h"
#include "chrome/common/media_router/test/test_helper.h"
#include "components/cast_channel/cast_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using cast_channel::GetAppAvailabilityResult;
using testing::_;
using testing::Invoke;
using testing::IsEmpty;

namespace media_router {

class CastAppDiscoveryServiceTest : public testing::Test {
 public:
  CastAppDiscoveryServiceTest()
      : task_runner_(base::MakeRefCounted<base::TestSimpleTaskRunner>()),
        socket_service_(task_runner_),
        message_handler_(&socket_service_),
        app_discovery_service_(
            std::make_unique<CastAppDiscoveryServiceImpl>(&message_handler_,
                                                          &socket_service_,
                                                          &media_sink_service_,
                                                          &clock_)),
        source_a_1_(*CastMediaSource::From(
            "cast:AAAAAAAA?clientId=1&capabilities=video_out,audio_out")),
        source_a_2_(*CastMediaSource::From(
            "cast:AAAAAAAA?clientId=2&capabilities=video_out,audio_out")),
        source_b_1_(*CastMediaSource::From(
            "cast:BBBBBBBB?clientId=1&capabilities=video_out,audio_out")),
        sink1_(CreateCastSink(1)) {
    ON_CALL(socket_service_, GetSocket(_))
        .WillByDefault(testing::Return(&socket_));
    task_runner_->RunPendingTasks();
  }

  ~CastAppDiscoveryServiceTest() override { task_runner_->RunPendingTasks(); }

  MOCK_METHOD2(OnSinkQueryUpdated,
               void(const MediaSource::Id&,
                    const std::vector<MediaSinkInternal>&));

  void AddOrUpdateSink(const MediaSinkInternal& sink) {
    media_sink_service_.AddOrUpdateSink(sink);
  }

  void RemoveSink(const MediaSinkInternal& sink) {
    media_sink_service_.RemoveSink(sink);
  }

  CastAppDiscoveryService::Subscription StartObservingMediaSinksInitially(
      const CastMediaSource& source) {
    auto subscription = app_discovery_service_->StartObservingMediaSinks(
        source,
        base::BindRepeating(&CastAppDiscoveryServiceTest::OnSinkQueryUpdated,
                            base::Unretained(this)));
    task_runner_->RunPendingTasks();
    return subscription;
  }

 protected:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::SimpleTestTickClock clock_;
  cast_channel::MockCastSocketService socket_service_;
  cast_channel::MockCastSocket socket_;
  cast_channel::MockCastMessageHandler message_handler_;
  TestMediaSinkService media_sink_service_;
  std::unique_ptr<CastAppDiscoveryService> app_discovery_service_;
  CastMediaSource source_a_1_;
  CastMediaSource source_a_2_;
  CastMediaSource source_b_1_;
  MediaSinkInternal sink1_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CastAppDiscoveryServiceTest);
};

TEST_F(CastAppDiscoveryServiceTest, StartObservingMediaSinks) {
  auto subscription1 = StartObservingMediaSinksInitially(source_a_1_);

  // Adding a sink after app registered causes app availability request to be
  // sent.
  cast_channel::GetAppAvailabilityCallback cb;
  EXPECT_CALL(message_handler_, DoRequestAppAvailability(_, "AAAAAAAA", _))
      .WillOnce(
          Invoke([&cb](cast_channel::CastSocket*, const std::string&,
                       cast_channel::GetAppAvailabilityCallback& callback) {
            cb = std::move(callback);
          }));

  AddOrUpdateSink(sink1_);

  // Same app ID should not trigger another request.
  EXPECT_CALL(message_handler_, DoRequestAppAvailability(_, _, _)).Times(0);
  auto subscription2 = app_discovery_service_->StartObservingMediaSinks(
      source_a_2_,
      base::BindRepeating(&CastAppDiscoveryServiceTest::OnSinkQueryUpdated,
                          base::Unretained(this)));

  std::vector<MediaSinkInternal> sinks_1 = {sink1_};
  EXPECT_CALL(*this, OnSinkQueryUpdated(source_a_1_.source_id(), sinks_1));
  EXPECT_CALL(*this, OnSinkQueryUpdated(source_a_2_.source_id(), sinks_1));
  std::move(cb).Run("AAAAAAAA", GetAppAvailabilityResult::kAvailable);

  // No more updates for |source_a_1_|.
  subscription1.reset();
  EXPECT_CALL(*this, OnSinkQueryUpdated(source_a_1_.source_id(), _)).Times(0);
  EXPECT_CALL(*this, OnSinkQueryUpdated(source_a_2_.source_id(), IsEmpty()));
  RemoveSink(sink1_);
}

TEST_F(CastAppDiscoveryServiceTest, SinkQueryUpdatedOnSinkUpdate) {
  auto subscription1 = StartObservingMediaSinksInitially(source_a_1_);

  // Adding a sink after app registered causes app availability request to be
  // sent.
  cast_channel::GetAppAvailabilityCallback cb;
  EXPECT_CALL(message_handler_, DoRequestAppAvailability(_, "AAAAAAAA", _))
      .WillOnce(
          Invoke([&cb](cast_channel::CastSocket*, const std::string&,
                       cast_channel::GetAppAvailabilityCallback& callback) {
            cb = std::move(callback);
          }));

  AddOrUpdateSink(sink1_);

  // Query now includes |sink1_|.
  std::vector<MediaSinkInternal> sinks_1 = {sink1_};
  EXPECT_CALL(*this, OnSinkQueryUpdated(source_a_1_.source_id(), sinks_1));
  std::move(cb).Run("AAAAAAAA", GetAppAvailabilityResult::kAvailable);

  // Updating |sink1| causes |source_a_1_| query to be updated.
  sink1_.sink().set_name("Updated name");
  sinks_1 = {sink1_};
  EXPECT_CALL(*this, OnSinkQueryUpdated(source_a_1_.source_id(), sinks_1));
  AddOrUpdateSink(sink1_);
}

TEST_F(CastAppDiscoveryServiceTest, Refresh) {
  auto subscription1 = StartObservingMediaSinksInitially(source_a_1_);
  auto subscription2 = StartObservingMediaSinksInitially(source_b_1_);

  EXPECT_CALL(*this, OnSinkQueryUpdated(_, _));
  EXPECT_CALL(message_handler_, DoRequestAppAvailability(_, "AAAAAAAA", _))
      .WillOnce(Invoke([](cast_channel::CastSocket*, const std::string& app_id,
                          cast_channel::GetAppAvailabilityCallback& callback) {
        std::move(callback).Run(app_id, GetAppAvailabilityResult::kAvailable);
      }));
  EXPECT_CALL(message_handler_, DoRequestAppAvailability(_, "BBBBBBBB", _))
      .WillOnce(Invoke([](cast_channel::CastSocket*, const std::string& app_id,
                          cast_channel::GetAppAvailabilityCallback& callback) {
        std::move(callback).Run(app_id, GetAppAvailabilityResult::kUnknown);
      }));
  AddOrUpdateSink(sink1_);

  MediaSinkInternal sink2 = CreateCastSink(2);
  EXPECT_CALL(message_handler_, DoRequestAppAvailability(_, "AAAAAAAA", _))
      .WillOnce(Invoke([](cast_channel::CastSocket*, const std::string& app_id,
                          cast_channel::GetAppAvailabilityCallback& callback) {
        std::move(callback).Run(app_id, GetAppAvailabilityResult::kUnavailable);
      }));
  EXPECT_CALL(message_handler_, DoRequestAppAvailability(_, "BBBBBBBB", _));
  AddOrUpdateSink(sink2);

  clock_.Advance(base::TimeDelta::FromSeconds(30));

  // Request app availability for app B for both sinks.
  // App A on |sink2| is not requested due to timing threshold.
  EXPECT_CALL(message_handler_, DoRequestAppAvailability(_, "AAAAAAAA", _))
      .Times(0);
  EXPECT_CALL(*this, OnSinkQueryUpdated(_, _)).Times(2);
  EXPECT_CALL(message_handler_, DoRequestAppAvailability(_, "BBBBBBBB", _))
      .Times(2)
      .WillRepeatedly(
          Invoke([](cast_channel::CastSocket*, const std::string& app_id,
                    cast_channel::GetAppAvailabilityCallback& callback) {
            std::move(callback).Run(app_id,
                                    GetAppAvailabilityResult::kAvailable);
          }));
  app_discovery_service_->Refresh();

  clock_.Advance(base::TimeDelta::FromSeconds(31));

  EXPECT_CALL(message_handler_, DoRequestAppAvailability(_, "AAAAAAAA", _));
  app_discovery_service_->Refresh();
}

TEST_F(CastAppDiscoveryServiceTest, StartObservingMediaSinksAfterSinkAdded) {
  // No registered apps.
  EXPECT_CALL(message_handler_, DoRequestAppAvailability(_, _, _)).Times(0);
  AddOrUpdateSink(sink1_);

  EXPECT_CALL(message_handler_, DoRequestAppAvailability(_, "AAAAAAAA", _));
  auto subscription1 = app_discovery_service_->StartObservingMediaSinks(
      source_a_1_,
      base::BindRepeating(&CastAppDiscoveryServiceTest::OnSinkQueryUpdated,
                          base::Unretained(this)));

  EXPECT_CALL(message_handler_, DoRequestAppAvailability(_, "BBBBBBBB", _));
  auto subscription2 = app_discovery_service_->StartObservingMediaSinks(
      source_b_1_,
      base::BindRepeating(&CastAppDiscoveryServiceTest::OnSinkQueryUpdated,
                          base::Unretained(this)));

  // Adding new sink causes availability requests for 2 apps to be sent to the
  // new sink.
  MediaSinkInternal sink2 = CreateCastSink(2);
  EXPECT_CALL(message_handler_, DoRequestAppAvailability(_, "AAAAAAAA", _));
  EXPECT_CALL(message_handler_, DoRequestAppAvailability(_, "BBBBBBBB", _));
  AddOrUpdateSink(sink2);
}

TEST_F(CastAppDiscoveryServiceTest, StartObservingMediaSinksCachedValue) {
  auto subscription1 = StartObservingMediaSinksInitially(source_a_1_);

  // Adding a sink after app registered causes app availability request to be
  // sent.
  cast_channel::GetAppAvailabilityCallback cb;
  EXPECT_CALL(message_handler_, DoRequestAppAvailability(_, "AAAAAAAA", _))
      .WillOnce(
          Invoke([&cb](cast_channel::CastSocket*, const std::string&,
                       cast_channel::GetAppAvailabilityCallback& callback) {
            cb = std::move(callback);
          }));
  AddOrUpdateSink(sink1_);

  std::vector<MediaSinkInternal> sinks_1 = {sink1_};
  EXPECT_CALL(*this, OnSinkQueryUpdated(source_a_1_.source_id(), sinks_1));
  std::move(cb).Run("AAAAAAAA", GetAppAvailabilityResult::kAvailable);

  // Same app ID should not trigger another request, but it should return
  // cached value.
  EXPECT_CALL(message_handler_, DoRequestAppAvailability(_, _, _)).Times(0);
  EXPECT_CALL(*this, OnSinkQueryUpdated(source_a_2_.source_id(), sinks_1));
  auto subscription2 = app_discovery_service_->StartObservingMediaSinks(
      source_a_2_,
      base::BindRepeating(&CastAppDiscoveryServiceTest::OnSinkQueryUpdated,
                          base::Unretained(this)));

  // Same source as |source_a_1_|. The callback will be invoked.
  auto source3 = CastMediaSource::From(source_a_1_.source_id());
  ASSERT_TRUE(source3);
  EXPECT_CALL(message_handler_, DoRequestAppAvailability(_, _, _)).Times(0);
  EXPECT_CALL(*this, OnSinkQueryUpdated(source_a_1_.source_id(), sinks_1));
  auto subscription3 = app_discovery_service_->StartObservingMediaSinks(
      *source3,
      base::BindRepeating(&CastAppDiscoveryServiceTest::OnSinkQueryUpdated,
                          base::Unretained(this)));
}

TEST_F(CastAppDiscoveryServiceTest, AvailabilityUnknownOrUnavailable) {
  auto subscription1 = StartObservingMediaSinksInitially(source_a_1_);

  // Adding a sink after app registered causes app availability request to be
  // sent.
  EXPECT_CALL(*this, OnSinkQueryUpdated(_, _)).Times(0);
  EXPECT_CALL(message_handler_, DoRequestAppAvailability(_, "AAAAAAAA", _))
      .WillOnce(Invoke([](cast_channel::CastSocket*, const std::string&,
                          cast_channel::GetAppAvailabilityCallback& callback) {
        std::move(callback).Run("AAAAAAAA", GetAppAvailabilityResult::kUnknown);
      }));
  AddOrUpdateSink(sink1_);

  // Sink updated and unknown app availability will cause request to be sent
  // again.
  EXPECT_CALL(*this, OnSinkQueryUpdated(_, _)).Times(0);
  EXPECT_CALL(message_handler_, DoRequestAppAvailability(_, "AAAAAAAA", _))
      .WillOnce(Invoke([](cast_channel::CastSocket*, const std::string&,
                          cast_channel::GetAppAvailabilityCallback& callback) {
        std::move(callback).Run("AAAAAAAA",
                                GetAppAvailabilityResult::kUnavailable);
      }));
  AddOrUpdateSink(sink1_);

  // Known availability -- no request sent.
  EXPECT_CALL(message_handler_, DoRequestAppAvailability(_, "AAAAAAAA", _))
      .Times(0);
  AddOrUpdateSink(sink1_);

  // Removing the sink will also remove previous availability information.
  // Next time sink is added, request will be sent.
  EXPECT_CALL(*this, OnSinkQueryUpdated(_, _)).Times(0);
  RemoveSink(sink1_);

  EXPECT_CALL(message_handler_, DoRequestAppAvailability(_, "AAAAAAAA", _));
  AddOrUpdateSink(sink1_);
}

TEST_F(CastAppDiscoveryServiceTest, CapabilitiesFiltering) {
  // Make |sink1_| an audio only device.
  sink1_.cast_data().capabilities =
      cast_channel::CastDeviceCapability::AUDIO_OUT;
  AddOrUpdateSink(sink1_);

  cast_channel::GetAppAvailabilityCallback cb;
  EXPECT_CALL(message_handler_, DoRequestAppAvailability(_, "AAAAAAAA", _))
      .WillOnce(testing::WithArg<2>(
          [&cb](cast_channel::GetAppAvailabilityCallback& callback) {
            cb = std::move(callback);
          }));
  auto subscription1 = app_discovery_service_->StartObservingMediaSinks(
      source_a_1_,
      base::BindRepeating(&CastAppDiscoveryServiceTest::OnSinkQueryUpdated,
                          base::Unretained(this)));

  // Even though the app is available, the sink does not fulfill the required
  // capabilities.
  EXPECT_CALL(*this, OnSinkQueryUpdated(source_a_1_.source_id(), IsEmpty()));
  std::move(cb).Run("AAAAAAAA", GetAppAvailabilityResult::kAvailable);
}

}  // namespace media_router
