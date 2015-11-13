// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "chrome/browser/media/router/media_route.h"
#include "chrome/browser/media/router/mock_media_router.h"
#include "chrome/browser/ui/webui/media_router/media_router_ui.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/test_util.h"
#include "extensions/common/value_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::SaveArg;

namespace media_router {

class MediaRouterUITest : public ::testing::Test {
 public:
  MediaRouterUITest() {}
  ~MediaRouterUITest() override {}

  MOCK_METHOD1(OnRoutesUpdated, void(const std::vector<MediaRoute>& routes));
};

TEST_F(MediaRouterUITest, UIMediaRoutesObserverFiltersNonDisplayRoutes) {
  MockMediaRouter mock_router;
  EXPECT_CALL(mock_router, RegisterMediaRoutesObserver(_)).Times(1);
  scoped_ptr<MediaRouterUI::UIMediaRoutesObserver> observer(
      new MediaRouterUI::UIMediaRoutesObserver(
          &mock_router, base::Bind(&MediaRouterUITest::OnRoutesUpdated,
                                   base::Unretained(this))));

  MediaRoute display_route_1("routeId1", MediaSource("mediaSource"), "sinkId1",
                             "desc 1", true, "", true);
  MediaRoute non_display_route_1("routeId2", MediaSource("mediaSource"),
                                 "sinkId2", "desc 2", true, "", false);
  MediaRoute display_route_2("routeId2", MediaSource("mediaSource"), "sinkId2",
                             "desc 2", true, "", true);
  std::vector<MediaRoute> routes;
  routes.push_back(display_route_1);
  routes.push_back(non_display_route_1);
  routes.push_back(display_route_2);

  std::vector<MediaRoute> filtered_routes;
  EXPECT_CALL(*this, OnRoutesUpdated(_)).WillOnce(SaveArg<0>(&filtered_routes));
  observer->OnRoutesUpdated(routes);

  ASSERT_EQ(2u, filtered_routes.size());
  EXPECT_TRUE(display_route_1.Equals(filtered_routes[0]));
  EXPECT_TRUE(filtered_routes[0].for_display());
  EXPECT_TRUE(display_route_2.Equals(filtered_routes[1]));
  EXPECT_TRUE(filtered_routes[1].for_display());

  EXPECT_CALL(mock_router, UnregisterMediaRoutesObserver(_)).Times(1);
  observer.reset();
}

TEST_F(MediaRouterUITest, GetExtensionNameExtensionPresent) {
  std::string id = "extensionid";
  GURL url = GURL("chrome-extension://" + id);
  scoped_ptr<extensions::ExtensionRegistry> registry =
      make_scoped_ptr(new extensions::ExtensionRegistry(nullptr));
  scoped_refptr<extensions::Extension> app =
      extensions::test_util::BuildApp(extensions::ExtensionBuilder().Pass())
          .MergeManifest(
              extensions::DictionaryBuilder().Set("name", "test app name"))
          .SetID(id)
          .Build();

  ASSERT_TRUE(registry->AddEnabled(app));
  EXPECT_EQ("test app name",
            MediaRouterUI::GetExtensionName(url, registry.get()));
}

TEST_F(MediaRouterUITest, GetExtensionNameEmptyWhenNotInstalled) {
  std::string id = "extensionid";
  GURL url = GURL("chrome-extension://" + id);
  scoped_ptr<extensions::ExtensionRegistry> registry =
      make_scoped_ptr(new extensions::ExtensionRegistry(nullptr));

  EXPECT_EQ("", MediaRouterUI::GetExtensionName(url, registry.get()));
}

TEST_F(MediaRouterUITest, GetExtensionNameEmptyWhenNotExtensionURL) {
  GURL url = GURL("https://www.google.com");
  scoped_ptr<extensions::ExtensionRegistry> registry =
      make_scoped_ptr(new extensions::ExtensionRegistry(nullptr));

  EXPECT_EQ("", MediaRouterUI::GetExtensionName(url, registry.get()));
}
}  // namespace media_router
