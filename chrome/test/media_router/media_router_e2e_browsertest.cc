// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/media_router/media_router_e2e_browsertest.h"

#include "base/command_line.h"
#include "base/stl_util.h"
#include "chrome/browser/media/router/media_router.h"
#include "chrome/browser/media/router/media_router_factory.h"
#include "chrome/browser/media/router/media_router_mojo_impl.h"
#include "chrome/browser/media/router/media_source.h"
#include "chrome/browser/media/router/media_source_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

// Use the following command to run e2e browser tests:
// ./out/Debug/browser_tests --user-data-dir=<empty user data dir>
//   --extension-unpacked=<mr extension dir>
//   --receiver=<chromecast device name>
//   --enable-pixel-output-in-tests --run-manual
//   --gtest_filter=MediaRouterE2EBrowserTest.<test case name>
//   --enable-logging=stderr
//   --whitelisted-extension-id=enhhojjnijigcajfphajepfemndkmdlo
//   --ui-test-action-timeout=200000
//   --media-router=1

namespace {
// Command line argument to specify receiver,
const char kReceiver[] = "receiver";
// URL to launch Castv2Player_Staging app on Chromecast
const char kCastAppPresentationUrl[] =
    "https://google.com/cast#__castAppId__=BE6E4473/"
    "__castClientId__=143692175507258981";
}  // namespace


namespace media_router {

MediaRouterE2EBrowserTest::MediaRouterE2EBrowserTest()
    : media_router_(nullptr) {
}

MediaRouterE2EBrowserTest::~MediaRouterE2EBrowserTest() {
}

void MediaRouterE2EBrowserTest::SetUpOnMainThread() {
  MediaRouterBaseBrowserTest::SetUpOnMainThread();
  media_router_ =
      MediaRouterFactory::GetApiForBrowserContext(browser()->profile());
  DCHECK(media_router_);
}

void MediaRouterE2EBrowserTest::TearDownOnMainThread() {
  MediaRouterBaseBrowserTest::TearDownOnMainThread();
  media_router_ = nullptr;
}

void MediaRouterE2EBrowserTest::OnRouteResponseReceived(
    const MediaRoute* route,
    const std::string& presentation_id,
    const std::string& error) {
  ASSERT_TRUE(route);
  route_id_ = route->media_route_id();
}

void MediaRouterE2EBrowserTest::CreateMediaRoute(
    const MediaSource& source,
    const GURL& origin,
    content::WebContents* web_contents) {
  DCHECK(media_router_);
  observer_.reset(new TestMediaSinksObserver(media_router_, source));
  observer_->Init();

  DVLOG(1) << "Receiver name: " << receiver_;
  // Wait for MediaSinks compatible with |source| to be discovered.
  ASSERT_TRUE(ConditionalWait(
      base::TimeDelta::FromSeconds(30), base::TimeDelta::FromSeconds(1),
      base::Bind(&MediaRouterE2EBrowserTest::IsSinkDiscovered,
                 base::Unretained(this))));

  const auto& sink_map = observer_->sink_map;
  const auto it = sink_map.find(receiver_);
  const MediaSink& sink = it->second;

  // The callback will set route_id_ when invoked.
  std::vector<MediaRouteResponseCallback> route_response_callbacks;
  route_response_callbacks.push_back(
      base::Bind(&MediaRouterE2EBrowserTest::OnRouteResponseReceived,
                 base::Unretained(this)));
  media_router_->CreateRoute(source.id(), sink.id(), origin, web_contents,
                             route_response_callbacks);

  // Wait for the route request to be fulfilled (and route to be started).
  ASSERT_TRUE(ConditionalWait(
      base::TimeDelta::FromSeconds(30), base::TimeDelta::FromSeconds(1),
      base::Bind(&MediaRouterE2EBrowserTest::IsRouteCreated,
                 base::Unretained(this))));
}

void MediaRouterE2EBrowserTest::StopMediaRoute() {
  ASSERT_FALSE(route_id_.empty());

  media_router_->CloseRoute(route_id_);

  observer_.reset();
  route_id_.clear();
}

void MediaRouterE2EBrowserTest::ParseCommandLine() {
  MediaRouterBaseBrowserTest::ParseCommandLine();
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  receiver_ = command_line->GetSwitchValueASCII(kReceiver);
  ASSERT_FALSE(receiver_.empty());
}

bool MediaRouterE2EBrowserTest::IsSinkDiscovered() const {
  return ContainsKey(observer_->sink_map, receiver_);
}

bool MediaRouterE2EBrowserTest::IsRouteCreated() const {
  return !route_id_.empty();
}

// Test cases

IN_PROC_BROWSER_TEST_F(MediaRouterE2EBrowserTest, MANUAL_TabMirroring) {
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), GURL("about:blank"), 1);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  int tab_id = SessionTabHelper::IdForTab(web_contents);

  // Wait for 30 seconds to make sure the route is stable.
  CreateMediaRoute(
      MediaSourceForTab(tab_id), GURL("http://origin/"), web_contents);
  Wait(base::TimeDelta::FromSeconds(30));

  // Wait for 10 seconds to make sure route has been stopped.
  StopMediaRoute();
  Wait(base::TimeDelta::FromSeconds(10));
}

IN_PROC_BROWSER_TEST_F(MediaRouterE2EBrowserTest, MANUAL_CastApp) {
  // Wait for 30 seconds to make sure the route is stable.
  CreateMediaRoute(MediaSourceForPresentationUrl(kCastAppPresentationUrl),
                   GURL("http://origin/"), nullptr);
  Wait(base::TimeDelta::FromSeconds(30));

  // Wait for 10 seconds to make sure route has been stopped.
  StopMediaRoute();
  Wait(base::TimeDelta::FromSeconds(10));
}

}  // namespace media_router
