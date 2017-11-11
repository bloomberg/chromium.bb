// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/mojo/wired_display_media_route_provider.h"

#include "chrome/browser/media/router/mojo/mock_mojo_media_router.h"
#include "chrome/common/media_router/mojo/media_router.mojom.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"
#include "ui/display/test/test_screen.h"
#include "ui/gfx/geometry/rect.h"

using display::Display;
using testing::_;
using testing::Invoke;
using testing::IsEmpty;
using testing::WithArg;

namespace media_router {

namespace {

class MockCallback {
 public:
  MOCK_METHOD3(CreateRoute,
               void(const base::Optional<MediaRoute>& route,
                    const base::Optional<std::string>& error,
                    RouteRequestResult::ResultCode result));
  MOCK_METHOD2(TerminateRoute,
               void(const base::Optional<std::string>& error,
                    RouteRequestResult::ResultCode result));
};

std::string GetSinkId(const Display& display) {
  return WiredDisplayMediaRouteProvider::kSinkPrefix +
         std::to_string(display.id());
}

const char kPresentationSource[] = "https://www.example.com/presentation";
const char kNonPresentationSource[] = "not://a.valid.presentation/source";
const mojom::MediaRouteProvider::Id kProviderId =
    mojom::MediaRouteProvider::Id::WIRED_DISPLAY;

}  // namespace

class TestWiredDisplayMediaRouteProvider
    : public WiredDisplayMediaRouteProvider {
 public:
  TestWiredDisplayMediaRouteProvider(mojom::MediaRouteProviderRequest request,
                                     mojom::MediaRouterPtr media_router,
                                     Profile* profile)
      : WiredDisplayMediaRouteProvider(std::move(request),
                                       std::move(media_router),
                                       profile) {}
  ~TestWiredDisplayMediaRouteProvider() override = default;

  void set_all_displays(std::vector<Display> all_displays) {
    all_displays_ = std::move(all_displays);
  }

  void set_primary_display(Display primary_display) {
    primary_display_ = std::move(primary_display);
  }

 protected:
  // In the actual class, this method returns display::Screen::GetAllDisplays().
  // TODO(takumif): Expand display::test::TestScreen to support multiple
  // displays, so that these methods don't need to be overridden. Then
  // SystemDisplayApiTest and WindowSizerTestCommon would also be able to use
  // the class.
  std::vector<Display> GetAllDisplays() const override { return all_displays_; }

  // In the actual class, this method returns
  // display::Screen::GetPrimaryDisplay().
  Display GetPrimaryDisplay() const override { return primary_display_; }

 private:
  std::vector<Display> all_displays_;
  Display primary_display_;
};

class WiredDisplayMediaRouteProviderTest : public testing::Test {
 public:
  WiredDisplayMediaRouteProviderTest()
      : sink_display1_bounds_(0, 0, 1920, 1080),  // x, y, width, height.
        sink_display2_bounds_(1920, 0, 1920, 1080),
        primary_display_bounds_(0, 1080, 1920, 1080),
        sink_display1_(10000001, sink_display1_bounds_),
        sink_display2_(10000002, sink_display2_bounds_),
        primary_display_(10000003, primary_display_bounds_),
        mirror_display_(10000004, primary_display_bounds_) {}
  ~WiredDisplayMediaRouteProviderTest() override {}

  void SetUp() override {
    display::Screen::SetScreenInstance(&test_screen_);

    mojom::MediaRouterPtr router_pointer;
    router_binding_ = std::make_unique<mojo::Binding<mojom::MediaRouter>>(
        &router_, mojo::MakeRequest(&router_pointer));
    provider_ = std::make_unique<TestWiredDisplayMediaRouteProvider>(
        mojo::MakeRequest(&provider_pointer_), std::move(router_pointer),
        &profile_);
    provider_->set_primary_display(primary_display_);
  }

  void TearDown() override {
    provider_.reset();
    display::Screen::SetScreenInstance(nullptr);
  }

 protected:
  // A mojo pointer to |provider_|.
  mojom::MediaRouteProviderPtr provider_pointer_;
  std::unique_ptr<TestWiredDisplayMediaRouteProvider> provider_;
  MockMojoMediaRouter router_;
  std::unique_ptr<mojo::Binding<mojom::MediaRouter>> router_binding_;

  gfx::Rect sink_display1_bounds_;
  gfx::Rect sink_display2_bounds_;
  gfx::Rect primary_display_bounds_;

  Display sink_display1_;
  Display sink_display2_;
  // The displays below do not meet the criteria for being used as sinks.
  Display primary_display_;
  Display mirror_display_;  // Has the same bounds as |primary_display_|.

 private:
  content::TestBrowserThreadBundle test_thread_bundle_;
  TestingProfile profile_;
  display::test::TestScreen test_screen_;
};

TEST_F(WiredDisplayMediaRouteProviderTest, GetDisplaysAsSinks) {
  // The primary display and the display mirroring it should not be provided
  // as sinks.
  provider_->set_all_displays(
      {sink_display1_, primary_display_, mirror_display_, sink_display2_});

  const std::string sink_id1 = GetSinkId(sink_display1_);
  const std::string sink_id2 = GetSinkId(sink_display2_);
  EXPECT_CALL(router_,
              OnSinksReceived(mojom::MediaRouteProvider::Id::WIRED_DISPLAY,
                              kPresentationSource, _, IsEmpty()))
      .WillOnce(WithArg<2>(Invoke(
          [&sink_id1, &sink_id2](const std::vector<MediaSinkInternal>& sinks) {
            EXPECT_EQ(sinks.size(), 2u);
            EXPECT_TRUE(sinks[0].sink().id() == sink_id1 ||
                        sinks[1].sink().id() == sink_id1);
            EXPECT_TRUE(sinks[0].sink().id() == sink_id2 ||
                        sinks[1].sink().id() == sink_id2);
          })));
  provider_pointer_->StartObservingMediaSinks(kPresentationSource);
  base::RunLoop().RunUntilIdle();
}

TEST_F(WiredDisplayMediaRouteProviderTest, NoSinksForNonPresentationSource) {
  EXPECT_CALL(router_,
              OnSinksReceived(kProviderId, kNonPresentationSource, _, _))
      .Times(0);
  provider_pointer_->StartObservingMediaSinks(kNonPresentationSource);
  base::RunLoop().RunUntilIdle();
}

TEST_F(WiredDisplayMediaRouteProviderTest, CreateAndTerminateRoute) {
  const std::string presentation_id = "presentationId";
  MockCallback callback;

  provider_pointer_->StartObservingMediaRoutes(kPresentationSource);
  base::RunLoop().RunUntilIdle();

  // Create a route for |presentation_id|.
  EXPECT_CALL(callback, CreateRoute(_, base::Optional<std::string>(),
                                    RouteRequestResult::OK))
      .WillOnce(WithArg<0>(
          Invoke([&presentation_id](const base::Optional<MediaRoute>& route) {
            EXPECT_TRUE(route.has_value());
            EXPECT_EQ(route->media_route_id(), presentation_id);
          })));
  EXPECT_CALL(router_,
              OnRoutesUpdated(kProviderId, _, kPresentationSource, IsEmpty()))
      .WillOnce(WithArg<1>(
          Invoke([&presentation_id](const std::vector<MediaRoute>& routes) {
            EXPECT_EQ(routes.size(), 1u);
            EXPECT_EQ(routes[0].media_route_id(), presentation_id);
          })));
  provider_pointer_->CreateRoute(
      kPresentationSource, GetSinkId(sink_display1_), presentation_id,
      url::Origin::Create(GURL(kPresentationSource)), 0,
      base::TimeDelta::FromSeconds(100), false,
      base::BindOnce(&MockCallback::CreateRoute, base::Unretained(&callback)));
  base::RunLoop().RunUntilIdle();

  // Terminate the route.
  EXPECT_CALL(callback, TerminateRoute(base::Optional<std::string>(),
                                       RouteRequestResult::OK));
  EXPECT_CALL(router_, OnRoutesUpdated(kProviderId, IsEmpty(),
                                       kPresentationSource, IsEmpty()));
  provider_pointer_->TerminateRoute(
      presentation_id, base::BindOnce(&MockCallback::TerminateRoute,
                                      base::Unretained(&callback)));
  base::RunLoop().RunUntilIdle();
}

}  // namespace media_router
