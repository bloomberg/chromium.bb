// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "ios/chrome/test/ios_chrome_unit_test_suite.h"
#include "ios/public/test/test_keyed_service_provider.h"

int main(int argc, char** argv) {
  ios::TestKeyedServiceProvider test_keyed_service_provider;
  ios::SetKeyedServiceProvider(&test_keyed_service_provider);

  IOSChromeUnitTestSuite test_suite(argc, argv);

  return base::LaunchUnitTests(
      argc, argv,
      base::Bind(&IOSChromeUnitTestSuite::Run, base::Unretained(&test_suite)));
}
