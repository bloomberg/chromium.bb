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
#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

class MediaRouterMojoImpl;
class MessagePump;

// Tests the API call flow between the MediaRouterMojoImpl and the Media Router
// Mojo service in both directions.
class MediaRouterMojoTest : public ::testing::Test {
 public:
  MediaRouterMojoTest();
  ~MediaRouterMojoTest() override;

 protected:
  void SetUp() override;

  void ProcessEventLoop();

  void ConnectProviderManagerService();

  const std::string& extension_id() const { return extension_id_; }

  MediaRouterMojoImpl* router() const { return mock_media_router_.get(); }

  // Mock objects.
  MockMediaRouteProvider mock_media_route_provider_;
  testing::NiceMock<MockEventPageTracker> mock_event_page_tracker_;

  // Mojo proxy object for |mock_media_router_|
  media_router::interfaces::MediaRouterPtr media_router_proxy_;

 private:
  std::string extension_id_;
  scoped_ptr<MediaRouterMojoImpl> mock_media_router_;
  scoped_ptr<mojo::Binding<interfaces::MediaRouteProvider>> binding_;
  base::MessageLoop message_loop_;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterMojoTest);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_MOJO_TEST_H_
