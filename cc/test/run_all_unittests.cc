// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/test/test_suite.h"
#include "cc/test/test_webkit_platform.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/WebKit/Source/Platform/chromium/public/Platform.h"

int main(int argc, char** argv) {
  ::testing::InitGoogleMock(&argc, argv);
  TestSuite test_suite(argc, argv);
  cc::TestWebKitPlatform platform;
  MessageLoop message_loop;
  WebKit::Platform::initialize(&platform);
  int result = test_suite.Run();
  WebKit::Platform::shutdown();

  return result;
}

