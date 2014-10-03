// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "mojo/common/test/test_support_impl.h"
#include "mojo/embedder/test_embedder.h"
#include "mojo/public/tests/test_support_private.h"
#include "testing/gtest/include/gtest/gtest.h"

int main(int argc, char** argv) {
  // Silence death test thread warnings on Linux. We can afford to run our death
  // tests a little more slowly (< 10 ms per death test on a Z620).
  testing::GTEST_FLAG(death_test_style) = "threadsafe";

  base::TestSuite test_suite(argc, argv);

  mojo::embedder::test::InitWithSimplePlatformSupport();
  mojo::test::TestSupport::Init(new mojo::test::TestSupportImpl());

  return base::LaunchUnitTests(
      argc, argv, base::Bind(&base::TestSuite::Run,
                             base::Unretained(&test_suite)));
}
