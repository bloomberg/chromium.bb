// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/test/test_suite.h>
#include <gmock/gmock.h>

#if defined(USE_LIBCC_FOR_COMPOSITOR)
#include <webkit/support/webkit_support.h>
#endif

int main(int argc, char** argv) {
  ::testing::InitGoogleMock(&argc, argv);
  TestSuite testSuite(argc, argv);
#if defined(USE_LIBCC_FOR_COMPOSITOR)
  webkit_support::SetUpTestEnvironmentForUnitTests();
#endif
  int result = testSuite.Run();
#if defined(USE_LIBCC_FOR_COMPOSITOR)
  webkit_support::TearDownTestEnvironment();
#endif

  return result;
}

