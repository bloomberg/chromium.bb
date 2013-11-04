// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "mojo/shell/context.h"

class ShellTestSuite : public base::TestSuite {
 public:
  ShellTestSuite(int argc, char** argv) : base::TestSuite(argc, argv) {}

 private:
  base::MessageLoop message_loop_;
  // Context is currently a singleton and requires a MessageLoop.
  mojo::shell::Context context_;

  DISALLOW_COPY_AND_ASSIGN(ShellTestSuite);
};

int main(int argc, char** argv) {
  ShellTestSuite test_suite(argc, argv);

  return base::LaunchUnitTests(
      argc, argv, base::Bind(&ShellTestSuite::Run,
                             base::Unretained(&test_suite)));
}
