// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/mojo/media_router_mojo_test.h"

#include <utility>

#include "base/run_loop.h"
#include "extensions/common/test_util.h"

namespace media_router {
namespace {

const char kInstanceId[] = "instance123";

template <typename T1, typename T2>
void ExpectAsyncResultEqual(T1 expected, T2 actual) {
  EXPECT_EQ(expected, actual);
}

}  // namespace

MockMediaRouteProvider::MockMediaRouteProvider() {}

MockMediaRouteProvider::~MockMediaRouteProvider() {}

MockEventPageTracker::MockEventPageTracker() {}

MockEventPageTracker::~MockEventPageTracker() {}

MediaRouterMojoTest::MediaRouterMojoTest()
    : mock_media_router_(new MediaRouterMojoImpl(&mock_event_page_tracker_)) {
  mock_media_router_->Initialize();
  mock_media_router_->set_instance_id_for_test(kInstanceId);
  extension_ = extensions::test_util::CreateEmptyExtension();
}

MediaRouterMojoTest::~MediaRouterMojoTest() {}

void MediaRouterMojoTest::ConnectProviderManagerService() {
  // Bind the |media_route_provider| interface to |media_route_provider_|.
  auto request = mojo::GetProxy(&media_router_proxy_);
  mock_media_router_->BindToMojoRequest(std::move(request), *extension_);

  // Bind the Mojo MediaRouter interface used by |mock_media_router_| to
  // |mock_media_route_provider_service_|.
  interfaces::MediaRouteProviderPtr mojo_media_router;
  binding_.reset(new mojo::Binding<interfaces::MediaRouteProvider>(
      &mock_media_route_provider_, mojo::GetProxy(&mojo_media_router)));
  media_router_proxy_->RegisterMediaRouteProvider(
      std::move(mojo_media_router),
      base::Bind(&ExpectAsyncResultEqual<std::string, mojo::String>,
                 kInstanceId));
}

void MediaRouterMojoTest::SetUp() {
  ON_CALL(mock_event_page_tracker_, IsEventPageSuspended(extension_id()))
      .WillByDefault(testing::Return(false));
  ConnectProviderManagerService();
  base::RunLoop().RunUntilIdle();
}

void MediaRouterMojoTest::TearDown() {
  mock_media_router_->Shutdown();
}

void MediaRouterMojoTest::ProcessEventLoop() {
  base::RunLoop().RunUntilIdle();
}

}  // namespace media_router
