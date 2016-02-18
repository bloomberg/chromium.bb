// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "build/build_config.h"
#include "content/public/test/unittest_test_suite.h"
#include "content/test/content_test_suite.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  base::TestSuite* suite = new content::ContentTestSuite(argc, argv);
#if defined(OS_MACOSX)
  base::mac::ScopedNSAutoreleasePool pool;
#endif

  return content::UnitTestTestSuite(suite).Run();
}
