// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "mojo/common/test/test_support_impl.h"
#include "mojo/public/tests/test_support_private.h"
#include "mojo/system/embedder/embedder.h"

int main(int argc, char** argv) {
  base::TestSuite test_suite(argc, argv);

  mojo::embedder::Init();
  mojo::test::TestSupport::Init(new mojo::test::TestSupportImpl());

  return base::LaunchUnitTests(
      argc, argv, base::Bind(&base::TestSuite::Run,
                             base::Unretained(&test_suite)));
}
