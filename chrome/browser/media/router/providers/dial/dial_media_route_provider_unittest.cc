// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/dial/dial_media_route_provider.h"

#include "chrome/browser/media/router/test/mock_mojo_media_router.h"

#include "base/run_loop.h"
#include "chrome/browser/media/router/test/test_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::StrictMock;

namespace media_router {

class DialMediaRouteProviderTest : public ::testing::Test {
 public:
  DialMediaRouteProviderTest() : mock_service_() {}

  void SetUp() override {
    mojom::MediaRouterPtr router_ptr;
    router_binding_ = std::make_unique<mojo::Binding<mojom::MediaRouter>>(
        &mock_router_, mojo::MakeRequest(&router_ptr));

    EXPECT_CALL(mock_router_, OnSinkAvailabilityUpdated(_, _));
    provider_ = std::make_unique<DialMediaRouteProvider>(
        mojo::MakeRequest(&provider_ptr_), std::move(router_ptr),
        &mock_service_);
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override { provider_.reset(); }

  MOCK_METHOD1(OnSinksDiscovered, void(std::vector<MediaSinkInternal> sinks));
  MOCK_METHOD1(OnDialSinkAdded, void(const MediaSinkInternal& sink));

 protected:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;

  mojom::MediaRouteProviderPtr provider_ptr_;
  MockMojoMediaRouter mock_router_;
  std::unique_ptr<mojo::Binding<mojom::MediaRouter>> router_binding_;

  StrictMock<MockDialMediaSinkService> mock_service_;
  std::unique_ptr<DialMediaRouteProvider> provider_;
  std::vector<MediaSinkInternal> cached_sinks_;

  DISALLOW_COPY_AND_ASSIGN(DialMediaRouteProviderTest);
};

TEST_F(DialMediaRouteProviderTest, TestAddRemoveSinkQuery) {
  std::string youtube_source("cast-dial:YouTube");
  EXPECT_CALL(mock_service_, StartMonitoringAvailableSinksForApp("YouTube", _));
  EXPECT_CALL(mock_service_, GetCachedAvailableSinks("YouTube"))
      .WillOnce(Return(cached_sinks_));
  provider_->StartObservingMediaSinks(youtube_source);

  MediaSinkInternal sink_internal = CreateDialSink(1);
  std::vector<MediaSinkInternal> sinks{sink_internal};
  std::vector<url::Origin> youtube_origins = {
      url::Origin::Create(GURL("https://tv.youtube.com")),
      url::Origin::Create(GURL("https://tv-green-qa.youtube.com")),
      url::Origin::Create(GURL("https://tv-release-qa.youtube.com")),
      url::Origin::Create(GURL("https://web-green-qa.youtube.com")),
      url::Origin::Create(GURL("https://web-release-qa.youtube.com")),
      url::Origin::Create(GURL("https://www.youtube.com"))};
  EXPECT_CALL(mock_router_,
              OnSinksReceived(MediaRouteProviderId::DIAL, youtube_source, sinks,
                              youtube_origins));
  provider_->OnAvailableSinksUpdated("YouTube", sinks);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(mock_router_, OnSinksReceived(_, _, _, _)).Times(0);
  provider_->StopObservingMediaSinks(youtube_source);
  provider_->OnAvailableSinksUpdated("YouTube", sinks);
  base::RunLoop().RunUntilIdle();
}

TEST_F(DialMediaRouteProviderTest, TestAddSinkQuerySameMediaSource) {
  std::string youtube_source("cast-dial:YouTube");
  EXPECT_CALL(mock_service_, StartMonitoringAvailableSinksForApp("YouTube", _));
  EXPECT_CALL(mock_service_, GetCachedAvailableSinks("YouTube"))
      .WillOnce(Return(cached_sinks_));
  provider_->StartObservingMediaSinks(youtube_source);

  EXPECT_CALL(mock_service_, StartMonitoringAvailableSinksForApp(_, _))
      .Times(0);
  provider_->StartObservingMediaSinks(youtube_source);

  EXPECT_CALL(mock_router_, OnSinksReceived(_, _, _, _)).Times(0);
  provider_->StopObservingMediaSinks(youtube_source);
  provider_->StopObservingMediaSinks(youtube_source);
  provider_->OnAvailableSinksUpdated("YouTube",
                                     std::vector<MediaSinkInternal>());
  base::RunLoop().RunUntilIdle();
}

TEST_F(DialMediaRouteProviderTest,
       TestAddSinkQuerySameAppDifferentMediaSources) {
  std::string youtube_source1("cast-dial:YouTube");
  std::string youtube_source2("cast-dial:YouTube?clientId=15178573373126446");
  EXPECT_CALL(mock_service_, StartMonitoringAvailableSinksForApp("YouTube", _));
  EXPECT_CALL(mock_service_, GetCachedAvailableSinks("YouTube"))
      .WillOnce(Return(cached_sinks_));
  provider_->StartObservingMediaSinks(youtube_source1);

  MediaSinkInternal sink_internal = CreateDialSink(1);
  std::vector<MediaSinkInternal> sinks{sink_internal};
  EXPECT_CALL(mock_service_, GetCachedAvailableSinks("YouTube"))
      .WillOnce(Return(sinks));
  EXPECT_CALL(mock_service_, StartMonitoringAvailableSinksForApp(_, _))
      .Times(0);
  EXPECT_CALL(mock_router_, OnSinksReceived(MediaRouteProviderId::DIAL,
                                            youtube_source2, sinks, _));
  provider_->StartObservingMediaSinks(youtube_source2);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(mock_router_, OnSinksReceived(MediaRouteProviderId::DIAL,
                                            youtube_source1, sinks, _));
  EXPECT_CALL(mock_router_, OnSinksReceived(MediaRouteProviderId::DIAL,
                                            youtube_source2, sinks, _));
  provider_->OnAvailableSinksUpdated("YouTube", sinks);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(mock_router_, OnSinksReceived(MediaRouteProviderId::DIAL,
                                            youtube_source2, sinks, _));
  provider_->StopObservingMediaSinks(youtube_source1);
  provider_->OnAvailableSinksUpdated("YouTube", sinks);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(mock_router_, OnSinksReceived(_, _, _, _)).Times(0);
  provider_->StopObservingMediaSinks(youtube_source2);
  provider_->OnAvailableSinksUpdated("YouTube", sinks);
  base::RunLoop().RunUntilIdle();
}

TEST_F(DialMediaRouteProviderTest, TestAddSinkQueryDifferentApps) {
  std::string youtube_source("cast-dial:YouTube");
  std::string netflix_source("cast-dial:Netflix");
  EXPECT_CALL(mock_service_, StartMonitoringAvailableSinksForApp("YouTube", _));
  EXPECT_CALL(mock_service_, GetCachedAvailableSinks("YouTube"))
      .WillOnce(Return(cached_sinks_));
  provider_->StartObservingMediaSinks(youtube_source);

  EXPECT_CALL(mock_service_, StartMonitoringAvailableSinksForApp("Netflix", _));
  EXPECT_CALL(mock_service_, GetCachedAvailableSinks("Netflix"))
      .WillOnce(Return(cached_sinks_));
  provider_->StartObservingMediaSinks(netflix_source);

  MediaSinkInternal sink_internal = CreateDialSink(1);
  std::vector<MediaSinkInternal> sinks{sink_internal};
  EXPECT_CALL(mock_router_, OnSinksReceived(MediaRouteProviderId::DIAL,
                                            youtube_source, sinks, _));
  provider_->OnAvailableSinksUpdated("YouTube", sinks);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(mock_router_, OnSinksReceived(MediaRouteProviderId::DIAL,
                                            netflix_source, sinks, _));
  provider_->OnAvailableSinksUpdated("Netflix", sinks);
  base::RunLoop().RunUntilIdle();

  provider_->StopObservingMediaSinks(youtube_source);
  provider_->StopObservingMediaSinks(netflix_source);

  EXPECT_CALL(mock_router_, OnSinksReceived(_, _, _, _)).Times(0);
  provider_->OnAvailableSinksUpdated("YouTube", sinks);
  provider_->OnAvailableSinksUpdated("Netflix", sinks);
  base::RunLoop().RunUntilIdle();
}

}  // namespace media_router
