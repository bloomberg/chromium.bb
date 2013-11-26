# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import mojom_test
import sys

EXPECT_EQ = mojom_test.EXPECT_EQ
EXPECT_TRUE = mojom_test.EXPECT_TRUE
RunTest = mojom_test.RunTest
ModulesAreEqual = mojom_test.ModulesAreEqual
BuildTestModule = mojom_test.BuildTestModule
TestTestModule = mojom_test.TestTestModule


def BuildAndTestModule():
  return TestTestModule(BuildTestModule())


def TestModulesEqual():
  return EXPECT_TRUE(ModulesAreEqual(BuildTestModule(), BuildTestModule()))


def Main(args):
  errors = 0
  errors += RunTest(BuildAndTestModule)
  errors += RunTest(TestModulesEqual)

  return errors


if __name__ == '__main__':
  sys.exit(Main(sys.argv[1:]))
