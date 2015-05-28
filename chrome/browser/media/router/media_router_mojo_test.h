// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_MOJO_TEST_H_
#define CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_MOJO_TEST_H_

#include <string>

#include "base/message_loop/message_loop.h"
#include "chrome/browser/media/router/media_router_mojo_impl.h"
#include "chrome/browser/media/router/mock_media_router.h"
#include "chrome/browser/media/router/test_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "mojo/common/message_pump_mojo.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

class MediaRouterMojoImpl;

// Tests the API call flow between the MediaRouterMojoImpl and the Media Router
// Mojo service in both directions.
// Calls are made through Mojo service bindings backed by mock objects.
// |router_| delegates Media Router calls to |mock_mojo_media_router_service_|,
// which represents the service provided by the component extension.
// Calls from the component extension back to |router_| via the
// MediaRouterObserver interface can be simulated with
// |mojo_media_router_observer_|.
class MediaRouterMojoTest : public ::testing::Test {
 public:
  MediaRouterMojoTest();
  ~MediaRouterMojoTest() override;

 protected:
  void SetUp() override;

  void ProcessEventLoop();

  void ConnectProviderManagerService();

  const std::string& extension_id() const { return extension_id_; }

  MediaRouterMojoImpl* router() const { return router_.get(); }

  // Mock objects.
  MockMojoMediaRouterService mock_mojo_media_router_service_;
  testing::NiceMock<MockEventPageTracker> mock_event_page_tracker_;

  // MediaRouterObserver interface that is bound to |router_|.
  media_router::interfaces::MediaRouterObserverPtr mojo_media_router_observer_;

 private:
  base::MessageLoop message_loop_;
  std::string extension_id_;
  scoped_ptr<MediaRouterMojoImpl> router_;
  scoped_ptr<mojo::Binding<interfaces::MediaRouter>> binding_;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterMojoTest);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_MOJO_TEST_H_
