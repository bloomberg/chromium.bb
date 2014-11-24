// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/launcher/test_launcher.h"

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/test/launcher/unit_test_launcher.h"

namespace {

int DummyRunTestSuite(void) {
  return -1;
}

}  // namespace

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  return base::LaunchUnitTests(argc, argv, base::Bind(&DummyRunTestSuite));
}
