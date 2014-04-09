// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/macros.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"

namespace {

class ExtensionsTestSuite : public base::TestSuite {
 public:
  ExtensionsTestSuite(int argc, char** argv);

 protected:
  // base::TestSuite:
  virtual void Initialize() OVERRIDE;
  virtual void Shutdown() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionsTestSuite);
};

ExtensionsTestSuite::ExtensionsTestSuite(int argc, char** argv)
    : base::TestSuite(argc, argv) {}

void ExtensionsTestSuite::Initialize() {
  base::TestSuite::Initialize();
}

void ExtensionsTestSuite::Shutdown() {
  base::TestSuite::Shutdown();
}

}  // namespace

int main(int argc, char** argv) {
  ExtensionsTestSuite test_suite(argc, argv);

  return base::LaunchUnitTests(argc,
                               argv,
                               base::Bind(&ExtensionsTestSuite::Run,
                                          base::Unretained(&test_suite)));
}
