// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/test/tray_cast_test_api.h"
#include "chrome/browser/media/router/media_routes_observer.h"
#include "chrome/browser/media/router/media_sinks_observer.h"
#include "chrome/browser/media/router/media_source_helper.h"
#include "chrome/browser/media/router/mock_media_router.h"
#include "chrome/browser/ui/ash/cast_config_delegate_media_router.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/test_utils.h"

using testing::_;

namespace {

// Helper to create a MediaSink intance.
media_router::MediaSink MakeSink(const std::string& id,
                                 const std::string& name) {
  return media_router::MediaSink(id, name,
                                 media_router::MediaSink::IconType::GENERIC);
}

// Helper to create a MediaRoute instance.
media_router::MediaRoute MakeRoute(const std::string& route_id,
                                   const std::string& sink_id,
                                   bool is_local) {
  return media_router::MediaRoute(
      route_id, media_router::MediaSourceForDesktop(), sink_id,
      std::string() /*description*/, is_local,
      std::string() /*custom_controller_path*/, true /*for_display*/);
}

// Returns the cast tray instance.
ash::TrayCast* GetTrayCast() {
  ash::SystemTray* tray = ash::Shell::GetInstance()->GetPrimarySystemTray();

  // Make sure we actually popup the tray, otherwise the TrayCast instance will
  // not be created.
  tray->ShowDefaultView(ash::BubbleCreationType::BUBBLE_CREATE_NEW);

  return tray->GetTrayCastForTesting();
}

class SystemTrayTrayCastMediaRouterChromeOSTest : public InProcessBrowserTest {
 protected:
  SystemTrayTrayCastMediaRouterChromeOSTest() : InProcessBrowserTest() {}
  ~SystemTrayTrayCastMediaRouterChromeOSTest() override {}

  media_router::MediaSinksObserver* media_sinks_observer() const {
    return media_sinks_observer_;
  }

  media_router::MediaRoutesObserver* media_routes_observer() const {
    return media_routes_observer_;
  }

 private:
  bool CaptureSink(media_router::MediaSinksObserver* media_sinks_observer) {
    media_sinks_observer_ = media_sinks_observer;
    return true;
  }

  void CaptureRoutes(media_router::MediaRoutesObserver* media_routes_observer) {
    media_routes_observer_ = media_routes_observer;
  }

  void SetUpInProcessBrowserTestFixture() override {
    ON_CALL(media_router_, RegisterMediaSinksObserver(_))
        .WillByDefault(Invoke(
            this, &SystemTrayTrayCastMediaRouterChromeOSTest::CaptureSink));
    ON_CALL(media_router_, RegisterMediaRoutesObserver(_))
        .WillByDefault(Invoke(
            this, &SystemTrayTrayCastMediaRouterChromeOSTest::CaptureRoutes));

    CastConfigDelegateMediaRouter::SetMediaRouterForTest(&media_router_);
  }

  media_router::MockMediaRouter media_router_;
  media_router::MediaSinksObserver* media_sinks_observer_ = nullptr;
  media_router::MediaRoutesObserver* media_routes_observer_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(SystemTrayTrayCastMediaRouterChromeOSTest);
};

}  // namespace

// Sanity check to make sure that the media router delegate is getting used.
IN_PROC_BROWSER_TEST_F(SystemTrayTrayCastMediaRouterChromeOSTest,
                       VerifyThatMediaRouterIsBeingUsed) {
  ash::CastConfigDelegate* cast_config_delegate = ash::Shell::GetInstance()
                                                      ->system_tray_delegate()
                                                      ->GetCastConfigDelegate();

  // The media router always reports false for HasOptions(); the extension
  // always reports true.
  EXPECT_FALSE(cast_config_delegate->HasOptions());
}

// Verifies that we only show the tray view if there are available cast
// targets/sinks.
IN_PROC_BROWSER_TEST_F(SystemTrayTrayCastMediaRouterChromeOSTest,
                       VerifyCorrectVisiblityWithSinks) {
  ash::TrayCast* tray = GetTrayCast();
  ash::TrayCastTestAPI test_api(tray);
  EXPECT_TRUE(test_api.IsTrayInitialized());

  std::vector<media_router::MediaSink> zero_sinks;
  std::vector<media_router::MediaSink> one_sink;
  std::vector<media_router::MediaSink> two_sinks;
  one_sink.push_back(MakeSink("id1", "name"));
  two_sinks.push_back(MakeSink("id1", "name"));
  two_sinks.push_back(MakeSink("id2", "name"));

  // The tray should be hidden when there are no sinks.
  EXPECT_FALSE(test_api.IsTrayVisible());
  media_sinks_observer()->OnSinksReceived(zero_sinks);
  EXPECT_FALSE(test_api.IsTrayVisible());
  EXPECT_FALSE(test_api.IsTraySelectViewVisible());

  // The tray should be visible with any more than zero sinks.
  media_sinks_observer()->OnSinksReceived(one_sink);
  EXPECT_TRUE(test_api.IsTrayVisible());
  media_sinks_observer()->OnSinksReceived(two_sinks);
  EXPECT_TRUE(test_api.IsTrayVisible());
  EXPECT_TRUE(test_api.IsTraySelectViewVisible());

  // And if all of the sinks go away, it should be hidden again.
  media_sinks_observer()->OnSinksReceived(zero_sinks);
  EXPECT_FALSE(test_api.IsTrayVisible());
  EXPECT_FALSE(test_api.IsTraySelectViewVisible());

  // The CastConfigDelegate instance gets destroyed before the TrayCast
  // instance. The delegate will assert if there are still callbacks registered.
  // We need to cleanup the TrayCast callbacks to prevent the assert.
  ash::TrayCastTestAPI(GetTrayCast()).ReleaseConfigCallbacks();
}

// Verifies that we show the cast view when we start a casting session, and that
// we display the correct cast session if there are multiple active casting
// sessions.
IN_PROC_BROWSER_TEST_F(SystemTrayTrayCastMediaRouterChromeOSTest,
                       VerifyCastingShowsCastView) {
  ash::TrayCast* tray = GetTrayCast();
  ash::TrayCastTestAPI test_api(tray);
  EXPECT_TRUE(test_api.IsTrayInitialized());

  // Setup the sinks.
  std::vector<media_router::MediaSink> sinks;
  sinks.push_back(MakeSink("remote_sink", "name"));
  sinks.push_back(MakeSink("local_sink", "name"));
  media_sinks_observer()->OnSinksReceived(sinks);

  // Create route combinations. More details below.
  media_router::MediaRoute non_local_route =
      MakeRoute("remote_route", "remote_sink", false /*is_local*/);
  media_router::MediaRoute local_route =
      MakeRoute("local_route", "local_sink", true /*is_local*/);
  std::vector<media_router::MediaRoute> no_routes;
  std::vector<media_router::MediaRoute> multiple_routes;
  // We put the non-local route first to make sure that we prefer the local one.
  multiple_routes.push_back(non_local_route);
  multiple_routes.push_back(local_route);

  // If there are multiple routes active at the same time, then we need to
  // display the local route over a non-local route. This also verifies that we
  // display the cast view when we're casting.
  test_api.OnCastingSessionStartedOrStopped(true /*is_casting*/);
  media_routes_observer()->OnRoutesUpdated(multiple_routes);
  EXPECT_TRUE(test_api.IsTrayCastViewVisible());
  EXPECT_EQ("local_route", test_api.GetDisplayedCastId());

  // When a casting session stops, we shouldn't display the cast view.
  test_api.OnCastingSessionStartedOrStopped(false /*is_casting*/);
  media_routes_observer()->OnRoutesUpdated(no_routes);
  EXPECT_FALSE(test_api.IsTrayCastViewVisible());

  ash::TrayCastTestAPI(GetTrayCast()).ReleaseConfigCallbacks();
}
