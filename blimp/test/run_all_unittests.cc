// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "mojo/edk/embedder/embedder.h"

namespace {

class BlimpTestSuite : public base::TestSuite {
 public:
  BlimpTestSuite(int argc, char** argv) : base::TestSuite(argc, argv) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(BlimpTestSuite);
};

}  // namespace

int main(int argc, char** argv) {
  mojo::edk::Init();
  BlimpTestSuite test_suite(argc, argv);
  return base::LaunchUnitTests(
      argc, argv,
      base::Bind(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
