// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/test/test_suite.h"
#include "testing/gmock/include/gmock/gmock.h"

int main(int argc, char** argv) {
  ::testing::InitGoogleMock(&argc, argv);
  TestSuite test_suite(argc, argv);
  MessageLoop message_loop;
  int result = test_suite.Run();

  return result;
}
