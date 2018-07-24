// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "chrome/chrome_cleaner/os/initializer.h"
#include "chrome/chrome_cleaner/pup_data/pup_data.h"
#include "chrome/chrome_cleaner/test/test_uws_catalog.h"

int main(int argc, char** argv) {
  base::TestSuite test_suite(argc, argv);

  chrome_cleaner::PUPData::InitializePUPData(
      {&chrome_cleaner::TestUwSCatalog::GetInstance()});
  chrome_cleaner::InitializeOSUtils();

  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
