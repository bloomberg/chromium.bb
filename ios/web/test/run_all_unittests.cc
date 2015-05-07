// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "ios/web/test/web_test_suite.h"

int main(int argc, char** argv) {
  web::WebTestSuite test_suite(argc, argv);

  return base::LaunchUnitTests(
      argc, argv, base::Bind(&web::WebTestSuite::Run,
                             base::Unretained(&test_suite)));
}
