// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/media_router_mojo_test.h"

#include "base/run_loop.h"

namespace media_router {
namespace {

const char kInstanceId[] = "instance123";

template <typename T1, typename T2>
void ExpectAsyncResultEqual(T1 expected, T2 actual) {
  EXPECT_EQ(expected, actual);
}

}  // namespace

MockMojoMediaRouterService::MockMojoMediaRouterService() {
}

MockMojoMediaRouterService::~MockMojoMediaRouterService() {
}

MediaRouterMojoTest::MediaRouterMojoTest()
    : message_loop_(mojo::common::MessagePumpMojo::Create()),
      extension_id_("ext-123"),
      router_(new MediaRouterMojoImpl) {
  router_->set_instance_id_for_test(kInstanceId);
}

MediaRouterMojoTest::~MediaRouterMojoTest() {
}

void MediaRouterMojoTest::ConnectProviderManagerService() {
  // Bind the |mojo_media_router_observer_| interface to |router_|.
  auto request = mojo::GetProxy(&mojo_media_router_observer_);
  router_->BindToMojoRequestForTest(request.Pass(), extension_id_,
                                    &mock_event_page_tracker_);

  // Bind the Mojo MediaRouter interface used by |router_| to
  // |mock_mojo_media_router_service_|.
  interfaces::MediaRouterPtr mojo_media_router;
  binding_.reset(new mojo::Binding<interfaces::MediaRouter>(
      &mock_mojo_media_router_service_, mojo::GetProxy(&mojo_media_router)));
  mojo_media_router_observer_->ProvideMediaRouter(
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
  base::RunLoop().RunUntilIdle();
}

}  // namespace media_router
