// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "chrome/browser/media/router/media_route.h"
#include "chrome/browser/media/router/mock_media_router.h"
#include "chrome/browser/ui/webui/media_router/media_router_ui.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_web_ui.h"
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

class MockRoutesUpdatedCallback {
 public:
  MOCK_METHOD1(OnRoutesUpdated, void(const std::vector<MediaRoute>& routes));
};

class MediaRouterUITest : public ::testing::Test {
 public:
  MediaRouterUITest() {
    web_contents_.reset(content::WebContents::Create(
        content::WebContents::CreateParams(&profile_)));
    web_ui_.set_web_contents(web_contents_.get());
    media_router_ui_.reset(new MediaRouterUI(&web_ui_));
  }

  ~MediaRouterUITest() override = default;

 protected:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  content::TestWebUI web_ui_;
  scoped_ptr<content::WebContents> web_contents_;
  scoped_ptr<MediaRouterUI> media_router_ui_;
};

TEST_F(MediaRouterUITest, SortedSinks) {
  std::vector<MediaSinkWithCastModes> unsorted_sinks;
  std::string sink_id1("sink3");
  std::string sink_name1("B sink");
  MediaSinkWithCastModes sink1(
      MediaSink(sink_id1, sink_name1, MediaSink::IconType::CAST));
  unsorted_sinks.push_back(sink1);

  std::string sink_id2("sink1");
  std::string sink_name2("A sink");
  MediaSinkWithCastModes sink2(
      MediaSink(sink_id2, sink_name2, MediaSink::IconType::CAST));
  unsorted_sinks.push_back(sink2);

  std::string sink_id3("sink2");
  std::string sink_name3("B sink");
  MediaSinkWithCastModes sink3(
      MediaSink(sink_id3, sink_name3, MediaSink::IconType::CAST));
  unsorted_sinks.push_back(sink3);

  // Sorted order is 2, 3, 1.
  media_router_ui_->OnResultsUpdated(unsorted_sinks);
  const auto& sorted_sinks = media_router_ui_->sinks_;
  EXPECT_EQ(sink_name2, sorted_sinks[0].sink.name());
  EXPECT_EQ(sink_id3, sorted_sinks[1].sink.id());
  EXPECT_EQ(sink_id1, sorted_sinks[2].sink.id());
}

TEST_F(MediaRouterUITest, UIMediaRoutesObserverFiltersNonDisplayRoutes) {
  MockMediaRouter mock_router;
  EXPECT_CALL(mock_router, RegisterMediaRoutesObserver(_)).Times(1);
  MockRoutesUpdatedCallback mock_callback;
  scoped_ptr<MediaRouterUI::UIMediaRoutesObserver> observer(
      new MediaRouterUI::UIMediaRoutesObserver(
          &mock_router, base::Bind(&MockRoutesUpdatedCallback::OnRoutesUpdated,
                                   base::Unretained(&mock_callback))));

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
  EXPECT_CALL(mock_callback, OnRoutesUpdated(_))
      .WillOnce(SaveArg<0>(&filtered_routes));
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
