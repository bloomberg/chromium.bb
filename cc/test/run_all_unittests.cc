// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "cc/test/cc_test_suite.h"
#include "mojo/core/embedder/embedder.h"

int main(int argc, char** argv) {
  cc::CCTestSuite test_suite(argc, argv);

  mojo::core::Init();

  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&cc::CCTestSuite::Run, base::Unretained(&test_suite)));
}
