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
#include "build/build_config.h"
#include "chrome/browser/media/router/event_page_request_manager.h"
#include "chrome/browser/media/router/event_page_request_manager_factory.h"
#include "chrome/browser/media/router/media_router_factory.h"
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

}  // namespace

class MediaRouterDesktopTest : public MediaRouterMojoTest {
 public:
  MediaRouterDesktopTest() {}
  ~MediaRouterDesktopTest() override {}

 protected:
  MediaRouterMojoImpl* SetTestingFactoryAndUse() override {
    return static_cast<MediaRouterMojoImpl*>(
        MediaRouterFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(), &CreateMediaRouter));
  }

 private:
  static std::unique_ptr<KeyedService> CreateMediaRouter(
      content::BrowserContext* context) {
    return std::unique_ptr<KeyedService>(new MediaRouterDesktop(
        context, MediaRouterDesktop::FirewallCheck::SKIP_FOR_TESTING));
  }
};

#if defined(OS_WIN)
TEST_F(MediaRouterDesktopTest, EnableMdnsAfterEachRegister) {
  EXPECT_CALL(mock_extension_provider_, EnableMdnsDiscovery()).Times(0);
  RegisterExtensionProvider();
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(mock_extension_provider_,
              UpdateMediaSinks(MediaSourceForDesktop().id()));
  EXPECT_CALL(mock_extension_provider_, EnableMdnsDiscovery());
  router()->OnUserGesture();
  base::RunLoop().RunUntilIdle();

  // EnableMdnsDiscovery() is called on this RegisterExtensionProvider() because
  // we've already seen an mdns-enabling event.
  EXPECT_CALL(mock_extension_provider_, EnableMdnsDiscovery());
  RegisterExtensionProvider();
  base::RunLoop().RunUntilIdle();
}
#endif

TEST_F(MediaRouterDesktopTest, UpdateMediaSinksOnUserGesture) {
#if defined(OS_WIN)
  EXPECT_CALL(mock_extension_provider_, EnableMdnsDiscovery());
#endif
  EXPECT_CALL(mock_extension_provider_,
              UpdateMediaSinks(MediaSourceForDesktop().id()));
  router()->OnUserGesture();
  base::RunLoop().RunUntilIdle();
}

TEST_F(MediaRouterDesktopTest, SyncStateToMediaRouteProvider) {
  MediaSource media_source(kSource);
  std::unique_ptr<MockMediaSinksObserver> sinks_observer;
  std::unique_ptr<MockMediaRoutesObserver> routes_observer;
  std::unique_ptr<NullMessageObserver> messages_observer;
  ProvideTestRoute(mojom::MediaRouteProvider::Id::EXTENSION, kRouteId);

  router()->OnSinkAvailabilityUpdated(
      mojom::MediaRouteProvider::Id::EXTENSION,
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
  MediaSink sink("sinkId", "sinkName", SinkIconType::CAST);
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
