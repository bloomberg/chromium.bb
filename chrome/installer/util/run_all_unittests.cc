// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "base/win/scoped_com_initializer.h"
#include "chrome/common/chrome_paths.h"

int main(int argc, char** argv) {
  TestSuite test_suite(argc, argv);

  // Register Chrome Path provider so that we can get test data dir.
  chrome::RegisterPathProvider();

  base::win::ScopedCOMInitializer com_initializer;
  if (!com_initializer.succeeded())
    return -1;
  return base::LaunchUnitTests(
      argc,
      argv,
      base::Bind(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
