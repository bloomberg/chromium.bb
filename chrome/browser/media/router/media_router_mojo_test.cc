// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/media_router_mojo_test.h"

#include "mojo/message_pump/message_pump_mojo.h"

namespace media_router {
namespace {

const char kInstanceId[] = "instance123";

template <typename T1, typename T2>
void ExpectAsyncResultEqual(T1 expected, T2 actual) {
  EXPECT_EQ(expected, actual);
}

}  // namespace

MockMediaRouteProvider::MockMediaRouteProvider() {
}

MockMediaRouteProvider::~MockMediaRouteProvider() {
}

MediaRouterMojoTest::MediaRouterMojoTest()
    : extension_id_("ext-123"),
      mock_media_router_(new MediaRouterMojoImpl(&mock_event_page_tracker_)),
      message_loop_(mojo::common::MessagePumpMojo::Create()) {
  mock_media_router_->set_instance_id_for_test(kInstanceId);
}

MediaRouterMojoTest::~MediaRouterMojoTest() {
}

void MediaRouterMojoTest::ConnectProviderManagerService() {
  // Bind the |media_route_provider| interface to |media_route_provider_|.
  auto request = mojo::GetProxy(&media_router_proxy_);
  mock_media_router_->BindToMojoRequest(request.Pass(), extension_id_);

  // Bind the Mojo MediaRouter interface used by |mock_media_router_| to
  // |mock_media_route_provider_service_|.
  interfaces::MediaRouteProviderPtr mojo_media_router;
  binding_.reset(new mojo::Binding<interfaces::MediaRouteProvider>(
      &mock_media_route_provider_, mojo::GetProxy(&mojo_media_router)));
  media_router_proxy_->RegisterMediaRouteProvider(
      mojo_media_router.Pass(),
      base::Bind(&ExpectAsyncResultEqual<std::string, mojo::String>,
                 kInstanceId));
}

void MediaRouterMojoTest::SetUp() {
  ON_CALL(mock_event_page_tracker_, IsEventPageSuspended(extension_id_))
      .WillByDefault(testing::Return(false));
  ConnectProviderManagerService();
  message_loop_.RunUntilIdle();
}

void MediaRouterMojoTest::ProcessEventLoop() {
  message_loop_.RunUntilIdle();
}

}  // namespace media_router
