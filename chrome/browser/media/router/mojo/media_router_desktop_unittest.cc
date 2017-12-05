// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "chrome/browser/media/router/mojo/media_router_desktop.h"

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/media/router/discovery/dial/dial_media_sink_service.h"
#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service.h"
#include "chrome/browser/media/router/event_page_request_manager.h"
#include "chrome/browser/media/router/event_page_request_manager_factory.h"
#include "chrome/browser/media/router/media_router_factory.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/media/router/mojo/media_router_mojo_test.h"
#include "chrome/common/media_router/media_source_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::Mock;
using testing::Return;

namespace media_router {

namespace {

const char kOrigin[] = "http://origin/";
const char kRouteId[] = "routeId";
const char kSource[] = "source1";

class NullMessageObserver : public RouteMessageObserver {
 public:
  NullMessageObserver(MediaRouter* router, const MediaRoute::Id& route_id)
      : RouteMessageObserver(router, route_id) {}
  ~NullMessageObserver() final {}

  void OnMessagesReceived(
      const std::vector<content::PresentationConnectionMessage>& messages)
      final {}
};

class MockDialMediaSinkService : public DialMediaSinkService {
 public:
  explicit MockDialMediaSinkService(content::BrowserContext* context)
      : DialMediaSinkService(context) {}
  ~MockDialMediaSinkService() override {}

  MOCK_METHOD3(Start,
               void(const OnSinksDiscoveredCallback&,
                    const OnDialSinkAddedCallback&,
                    const scoped_refptr<base::SequencedTaskRunner>&));
  MOCK_METHOD0(ForceSinkDiscoveryCallback, void());
  MOCK_METHOD0(OnUserGesture, void());
};

class MockCastMediaSinkService : public CastMediaSinkService {
 public:
  explicit MockCastMediaSinkService(content::BrowserContext* context)
      : CastMediaSinkService(context) {}
  ~MockCastMediaSinkService() override {}

  MOCK_METHOD1(Start, void(const OnSinksDiscoveredCallback&));
  MOCK_METHOD0(ForceSinkDiscoveryCallback, void());
  MOCK_METHOD0(OnUserGesture, void());
};

}  // namespace

class MediaRouterDesktopTest : public MediaRouterMojoTest {
 public:
  MediaRouterDesktopTest() {}
  ~MediaRouterDesktopTest() override {}

 protected:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        {kEnableDialLocalDiscovery, kEnableCastDiscovery}, {});
    MediaRouterMojoTest::SetUp();
  }
  std::unique_ptr<MediaRouterMojoImpl> CreateMediaRouter() override {
    auto dial_media_sink_service =
        std::make_unique<MockDialMediaSinkService>(profile());
    dial_media_sink_service_ = dial_media_sink_service.get();
    EXPECT_CALL(*dial_media_sink_service_, ForceSinkDiscoveryCallback());

    auto cast_media_sink_service =
        std::make_unique<MockCastMediaSinkService>(profile());
    cast_media_sink_service_ = cast_media_sink_service.get();
    EXPECT_CALL(*cast_media_sink_service_, ForceSinkDiscoveryCallback());
    return std::unique_ptr<MediaRouterDesktop>(
        new MediaRouterDesktop(profile(), std::move(dial_media_sink_service),
                               std::move(cast_media_sink_service)));
  }

  MockDialMediaSinkService* dial_media_sink_service() const {
    return dial_media_sink_service_;
  }

  MockCastMediaSinkService* cast_media_sink_service() const {
    return cast_media_sink_service_;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  MockDialMediaSinkService* dial_media_sink_service_ = nullptr;
  MockCastMediaSinkService* cast_media_sink_service_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterDesktopTest);
};

#if defined(OS_WIN)
TEST_F(MediaRouterDesktopTest, EnableMdnsAfterEachRegister) {
  EXPECT_CALL(mock_extension_provider_, EnableMdnsDiscovery()).Times(0);
  EXPECT_CALL(*dial_media_sink_service(), ForceSinkDiscoveryCallback());
  EXPECT_CALL(*cast_media_sink_service(), ForceSinkDiscoveryCallback());
  RegisterExtensionProvider();
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(mock_extension_provider_,
              UpdateMediaSinks(MediaSourceForDesktop().id()));
  EXPECT_CALL(mock_extension_provider_, EnableMdnsDiscovery());
  EXPECT_CALL(*dial_media_sink_service(), OnUserGesture());
  EXPECT_CALL(*cast_media_sink_service(), OnUserGesture());
  router()->OnUserGesture();
  base::RunLoop().RunUntilIdle();

  // EnableMdnsDiscovery() is called on this RegisterExtensionProvider() because
  // we've already seen an mdns-enabling event.
  EXPECT_CALL(mock_extension_provider_, EnableMdnsDiscovery());
  EXPECT_CALL(*dial_media_sink_service(), ForceSinkDiscoveryCallback());
  EXPECT_CALL(*cast_media_sink_service(), ForceSinkDiscoveryCallback());
  RegisterExtensionProvider();
  base::RunLoop().RunUntilIdle();
}
#endif

TEST_F(MediaRouterDesktopTest, OnUserGesture) {
#if defined(OS_WIN)
  EXPECT_CALL(mock_extension_provider_, EnableMdnsDiscovery());
#endif
  EXPECT_CALL(mock_extension_provider_,
              UpdateMediaSinks(MediaSourceForDesktop().id()));
  EXPECT_CALL(*dial_media_sink_service(), OnUserGesture());
  EXPECT_CALL(*cast_media_sink_service(), OnUserGesture());
  router()->OnUserGesture();
  base::RunLoop().RunUntilIdle();
}

TEST_F(MediaRouterDesktopTest, SyncStateToMediaRouteProvider) {
  MediaSource media_source(kSource);
  std::unique_ptr<MockMediaSinksObserver> sinks_observer;
  std::unique_ptr<MockMediaRoutesObserver> routes_observer;
  std::unique_ptr<NullMessageObserver> messages_observer;
  ProvideTestRoute(MediaRouteProviderId::EXTENSION, kRouteId);

  router()->OnSinkAvailabilityUpdated(
      MediaRouteProviderId::EXTENSION,
      mojom::MediaRouter::SinkAvailability::PER_SOURCE);
  EXPECT_CALL(mock_extension_provider_,
              StartObservingMediaSinks(media_source.id()));
  sinks_observer = base::MakeUnique<MockMediaSinksObserver>(
      router(), media_source, url::Origin::Create(GURL(kOrigin)));
  EXPECT_TRUE(sinks_observer->Init());

  EXPECT_CALL(mock_extension_provider_,
              StartObservingMediaRoutes(media_source.id()));
  routes_observer =
      base::MakeUnique<MockMediaRoutesObserver>(router(), media_source.id());

  EXPECT_CALL(mock_extension_provider_,
              StartListeningForRouteMessages(kRouteId));
  messages_observer = base::MakeUnique<NullMessageObserver>(router(), kRouteId);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_extension_provider_));
}

TEST_F(MediaRouterDesktopTest, TestProvideSinks) {
  std::vector<MediaSinkInternal> sinks;
  MediaSink sink("sinkId", "sinkName", SinkIconType::CAST,
                 MediaRouteProviderId::EXTENSION);
  CastSinkExtraData extra_data;
  net::IPAddress ip_address;
  EXPECT_TRUE(ip_address.AssignFromIPLiteral("192.168.1.3"));
  extra_data.ip_endpoint = net::IPEndPoint(ip_address, 0);
  extra_data.capabilities = 2;
  extra_data.cast_channel_id = 3;
  MediaSinkInternal expected_sink(sink, extra_data);
  sinks.push_back(expected_sink);
  std::string provider_name = "cast";

  EXPECT_CALL(mock_extension_provider_, ProvideSinks(provider_name, sinks));

  MediaRouterDesktop* media_router_desktop =
      static_cast<MediaRouterDesktop*>(router());
  media_router_desktop->ProvideSinks(provider_name, sinks);
  base::RunLoop().RunUntilIdle();
}

// Tests that auto-join and Cast SDK join requests are routed to the extension
// MediaRouteProvider.
TEST_F(MediaRouterDesktopTest, SendCastJoinRequestsToExtension) {
  TestJoinRoute(kAutoJoinPresentationId);
  TestJoinRoute(kCastPresentationIdPrefix + std::string("123"));
}

}  // namespace media_router
