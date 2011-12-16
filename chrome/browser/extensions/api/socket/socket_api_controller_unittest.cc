// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/socket/socket_api.h"
#include "chrome/browser/extensions/api/socket/socket_api_controller.h"
#include "chrome/browser/extensions/api/socket/socket_event_notifier.h"
#include "chrome/common/extensions/extension.h"

namespace extensions {

class SocketApiControllerTest : public testing::Test {
};

TEST_F(SocketApiControllerTest, TestSocketControllerLifetime) {
  // We want to make sure that killing the controller while a bunch of
  // sockets are alive doesn't crash.
  scoped_ptr<SocketController> controller(new SocketController());

  // Create some sockets but don't do anything with them.
  Profile* profile = NULL;
  std::string extension_id;
  EXPECT_TRUE(Extension::GenerateId("e^(iπ)+1=0", &extension_id));
  const GURL url;
  for (int i = 0; i < 10; ++i) {
    SocketEventNotifier* notifier =
        new SocketEventNotifier(NULL, profile, extension_id, -1, url);
    int socket_id = controller->CreateUdp(notifier);
    ASSERT_TRUE(socket_id != 0);
  }

  // Create some more sockets and connect them. Note that because this is
  // UDP, we can happily "connect" a UDP socket without anyone listening.
  const int kPort = 38888;
  const std::string address("127.0.0.1");
  for (int i = 0; i < 10; ++i) {
    SocketEventNotifier* notifier =
        new SocketEventNotifier(NULL, profile, extension_id, -1, url);
    int socket_id = controller->CreateUdp(notifier);
    ASSERT_TRUE(socket_id != 0);
    ASSERT_TRUE(controller->ConnectUdp(socket_id, address, kPort));
  }
}

}  // namespace extensions
