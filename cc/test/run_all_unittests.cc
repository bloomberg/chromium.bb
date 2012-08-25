// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/test/test_suite.h>
#include <gmock/gmock.h>
#include <webkit/support/webkit_support.h>

int main(int argc, char** argv) {
  ::testing::InitGoogleMock(&argc, argv);
  TestSuite testSuite(argc, argv);
  webkit_support::SetUpTestEnvironmentForUnitTests();
  int result = testSuite.Run();
  webkit_support::TearDownTestEnvironment();

  return result;
}

