// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_content_client_initializer.h"

#include "build/build_config.h"
#include "content/browser/notification_service_impl.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/content_client.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_content_client.h"
#include "content/test/test_render_view_host_factory.h"
#include "services/network/test/test_network_connection_tracker.h"

namespace content {

TestContentClientInitializer::TestContentClientInitializer() {
  test_network_connection_tracker_ =
      network::TestNetworkConnectionTracker::CreateInstance();
  SetNetworkConnectionTrackerForTesting(
      network::TestNetworkConnectionTracker::GetInstance());

  notification_service_.reset(new NotificationServiceImpl());

  content_client_.reset(new TestContentClient);
  SetContentClient(content_client_.get());

  content_browser_client_.reset(new TestContentBrowserClient());
  content::SetBrowserClientForTesting(content_browser_client_.get());
}

TestContentClientInitializer::~TestContentClientInitializer() {
  test_render_view_host_factory_.reset();
  rph_factory_.reset();
  notification_service_.reset();

  SetContentClient(nullptr);
  content_client_.reset();

  content_browser_client_.reset();

  SetNetworkConnectionTrackerForTesting(nullptr);
  test_network_connection_tracker_.reset();
}

void TestContentClientInitializer::CreateTestRenderViewHosts() {
  rph_factory_.reset(new MockRenderProcessHostFactory());
  test_render_view_host_factory_.reset(
      new TestRenderViewHostFactory(rph_factory_.get()));
}

}  // namespace content
