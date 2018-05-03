// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/dial/dial_media_route_provider.h"

#include "chrome/browser/media/router/test/mock_mojo_media_router.h"

#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/media/router/discovery/dial/dial_media_sink_service_impl.h"
#include "chrome/browser/media/router/test/test_helper.h"
#include "services/service_manager/public/cpp/connector.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::IsEmpty;
using ::testing::SaveArg;

namespace media_router {

class TestDialMediaSinkServiceImpl : public DialMediaSinkServiceImpl {
 public:
  TestDialMediaSinkServiceImpl()
      : DialMediaSinkServiceImpl(/* connector */ nullptr,
                                 base::DoNothing(),
                                 /* task_runner */ nullptr) {}

  ~TestDialMediaSinkServiceImpl() override = default;

  MOCK_METHOD0(Start, void());

  SinkQueryByAppSubscription StartMonitoringAvailableSinksForApp(
      const std::string& app_name,
      const SinkQueryByAppCallback& callback) override {
    DoStartMonitoringAvailableSinksForApp(app_name);
    auto& cb_list = sink_query_cbs_[app_name];
    if (!cb_list)
      cb_list = std::make_unique<SinkQueryByAppCallbackList>();
    return cb_list->Add(callback);
  }
  MOCK_METHOD1(DoStartMonitoringAvailableSinksForApp,
               void(const std::string& app_name));

  void SetAvailableSinks(const std::string& app_name,
                         const std::vector<MediaSinkInternal>& sinks) {
    available_sinks_[app_name] = sinks;
    for (const auto& sink : sinks)
      all_sinks_[sink.sink().id()] = sink;
  }

  void NotifyAvailableSinks(const std::string& app_name) {
    auto& cb_list = sink_query_cbs_[app_name];
    if (cb_list)
      cb_list->Notify(app_name);
  }

  std::vector<MediaSinkInternal> GetAvailableSinks(
      const std::string& app_name) const override {
    auto it = available_sinks_.find(app_name);
    return it != available_sinks_.end() ? it->second
                                        : std::vector<MediaSinkInternal>();
  }

 private:
  base::flat_map<std::string, std::unique_ptr<SinkQueryByAppCallbackList>>
      sink_query_cbs_;
  base::flat_map<std::string, std::vector<MediaSinkInternal>> available_sinks_;
  base::flat_map<MediaSink::Id, MediaSinkInternal> all_sinks_;
};

class DialMediaRouteProviderTest : public ::testing::Test {
 public:
  DialMediaRouteProviderTest() {}

  void SetUp() override {
    mojom::MediaRouterPtr router_ptr;
    router_binding_ = std::make_unique<mojo::Binding<mojom::MediaRouter>>(
        &mock_router_, mojo::MakeRequest(&router_ptr));

    EXPECT_CALL(mock_router_, OnSinkAvailabilityUpdated(_, _));
    provider_ = std::make_unique<DialMediaRouteProvider>(
        mojo::MakeRequest(&provider_ptr_), router_ptr.PassInterface(),
        &mock_sink_service_, base::SequencedTaskRunnerHandle::Get());

    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override { provider_.reset(); }

 protected:
  base::test::ScopedTaskEnvironment environment_;

  mojom::MediaRouteProviderPtr provider_ptr_;
  MockMojoMediaRouter mock_router_;
  std::unique_ptr<mojo::Binding<mojom::MediaRouter>> router_binding_;

  TestDialMediaSinkServiceImpl mock_sink_service_;
  std::unique_ptr<DialMediaRouteProvider> provider_;

  MediaSinkInternal sink_ = CreateDialSink(1);

  DISALLOW_COPY_AND_ASSIGN(DialMediaRouteProviderTest);
};

TEST_F(DialMediaRouteProviderTest, AddRemoveSinkQuery) {
  std::vector<url::Origin> youtube_origins = {
      url::Origin::Create(GURL("https://tv.youtube.com")),
      url::Origin::Create(GURL("https://tv-green-qa.youtube.com")),
      url::Origin::Create(GURL("https://tv-release-qa.youtube.com")),
      url::Origin::Create(GURL("https://web-green-qa.youtube.com")),
      url::Origin::Create(GURL("https://web-release-qa.youtube.com")),
      url::Origin::Create(GURL("https://www.youtube.com"))};
  std::string youtube_source("cast-dial:YouTube");
  EXPECT_CALL(mock_sink_service_,
              DoStartMonitoringAvailableSinksForApp("YouTube"));
  EXPECT_CALL(mock_router_,
              OnSinksReceived(MediaRouteProviderId::DIAL, youtube_source,
                              IsEmpty(), youtube_origins));
  provider_->StartObservingMediaSinks(youtube_source);
  base::RunLoop().RunUntilIdle();

  MediaSinkInternal sink = CreateDialSink(1);
  std::vector<MediaSinkInternal> sinks = {sink};
  mock_sink_service_.SetAvailableSinks("YouTube", sinks);

  EXPECT_CALL(mock_router_,
              OnSinksReceived(MediaRouteProviderId::DIAL, youtube_source, sinks,
                              youtube_origins));
  mock_sink_service_.NotifyAvailableSinks("YouTube");
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(mock_router_, OnSinksReceived(_, _, _, _)).Times(0);
  provider_->StopObservingMediaSinks(youtube_source);
  mock_sink_service_.NotifyAvailableSinks("YouTube");
  base::RunLoop().RunUntilIdle();
}

TEST_F(DialMediaRouteProviderTest, AddSinkQuerySameMediaSource) {
  std::string youtube_source("cast-dial:YouTube");
  EXPECT_CALL(mock_sink_service_,
              DoStartMonitoringAvailableSinksForApp("YouTube"));
  EXPECT_CALL(mock_router_, OnSinksReceived(MediaRouteProviderId::DIAL,
                                            youtube_source, IsEmpty(), _));
  provider_->StartObservingMediaSinks(youtube_source);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(mock_sink_service_, DoStartMonitoringAvailableSinksForApp(_))
      .Times(0);
  EXPECT_CALL(mock_router_,
              OnSinksReceived(MediaRouteProviderId::DIAL, _, _, _))
      .Times(0);
  provider_->StartObservingMediaSinks(youtube_source);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(mock_router_, OnSinksReceived(_, _, _, _)).Times(0);
  provider_->StopObservingMediaSinks(youtube_source);
  provider_->StopObservingMediaSinks(youtube_source);
  mock_sink_service_.NotifyAvailableSinks("YouTube");
  base::RunLoop().RunUntilIdle();
}

TEST_F(DialMediaRouteProviderTest,
       TestAddSinkQuerySameAppDifferentMediaSources) {
  std::string youtube_source1("cast-dial:YouTube");
  std::string youtube_source2("cast-dial:YouTube?clientId=15178573373126446");
  EXPECT_CALL(mock_sink_service_,
              DoStartMonitoringAvailableSinksForApp("YouTube"));
  EXPECT_CALL(mock_router_, OnSinksReceived(MediaRouteProviderId::DIAL,
                                            youtube_source1, IsEmpty(), _));
  provider_->StartObservingMediaSinks(youtube_source1);
  base::RunLoop().RunUntilIdle();

  MediaSinkInternal sink = CreateDialSink(1);
  std::vector<MediaSinkInternal> sinks = {sink};
  mock_sink_service_.SetAvailableSinks("YouTube", sinks);
  EXPECT_CALL(mock_router_, OnSinksReceived(MediaRouteProviderId::DIAL,
                                            youtube_source1, sinks, _));
  mock_sink_service_.NotifyAvailableSinks("YouTube");

  EXPECT_CALL(mock_sink_service_, DoStartMonitoringAvailableSinksForApp(_))
      .Times(0);
  EXPECT_CALL(mock_router_, OnSinksReceived(MediaRouteProviderId::DIAL,
                                            youtube_source2, sinks, _));
  provider_->StartObservingMediaSinks(youtube_source2);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(mock_router_, OnSinksReceived(MediaRouteProviderId::DIAL,
                                            youtube_source1, sinks, _));
  EXPECT_CALL(mock_router_, OnSinksReceived(MediaRouteProviderId::DIAL,
                                            youtube_source2, sinks, _));
  mock_sink_service_.NotifyAvailableSinks("YouTube");
  base::RunLoop().RunUntilIdle();

  provider_->StopObservingMediaSinks(youtube_source1);
  EXPECT_CALL(mock_router_, OnSinksReceived(MediaRouteProviderId::DIAL,
                                            youtube_source2, sinks, _));
  mock_sink_service_.NotifyAvailableSinks("YouTube");
  base::RunLoop().RunUntilIdle();

  provider_->StopObservingMediaSinks(youtube_source2);
  EXPECT_CALL(mock_router_, OnSinksReceived(_, _, _, _)).Times(0);
  mock_sink_service_.NotifyAvailableSinks("YouTube");
  base::RunLoop().RunUntilIdle();
}

TEST_F(DialMediaRouteProviderTest, AddSinkQueryDifferentApps) {
  std::string youtube_source("cast-dial:YouTube");
  std::string netflix_source("cast-dial:Netflix");
  EXPECT_CALL(mock_sink_service_,
              DoStartMonitoringAvailableSinksForApp("YouTube"));
  EXPECT_CALL(mock_router_, OnSinksReceived(MediaRouteProviderId::DIAL,
                                            youtube_source, IsEmpty(), _));
  provider_->StartObservingMediaSinks(youtube_source);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(mock_sink_service_,
              DoStartMonitoringAvailableSinksForApp("Netflix"));
  EXPECT_CALL(mock_router_, OnSinksReceived(MediaRouteProviderId::DIAL,
                                            netflix_source, IsEmpty(), _));
  provider_->StartObservingMediaSinks(netflix_source);
  base::RunLoop().RunUntilIdle();

  MediaSinkInternal sink = CreateDialSink(1);
  std::vector<MediaSinkInternal> sinks = {sink};
  mock_sink_service_.SetAvailableSinks("YouTube", sinks);
  EXPECT_CALL(mock_router_, OnSinksReceived(MediaRouteProviderId::DIAL,
                                            youtube_source, sinks, _));
  mock_sink_service_.NotifyAvailableSinks("YouTube");
  base::RunLoop().RunUntilIdle();

  mock_sink_service_.SetAvailableSinks("Netflix", sinks);
  EXPECT_CALL(mock_router_, OnSinksReceived(MediaRouteProviderId::DIAL,
                                            netflix_source, sinks, _));
  mock_sink_service_.NotifyAvailableSinks("Netflix");
  base::RunLoop().RunUntilIdle();

  provider_->StopObservingMediaSinks(youtube_source);
  provider_->StopObservingMediaSinks(netflix_source);

  EXPECT_CALL(mock_router_, OnSinksReceived(_, _, _, _)).Times(0);
  mock_sink_service_.NotifyAvailableSinks("YouTube");
  mock_sink_service_.NotifyAvailableSinks("Netflix");
  base::RunLoop().RunUntilIdle();
}

}  // namespace media_router
