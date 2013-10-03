# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Setup for linker tests."""

import logging
import os
import sys
import types

import test_case
import test_runner


def Setup(options, devices):
  """Creates a list of test cases and a runner factory.

  Returns:
    A tuple of (TestRunnerFactory, tests).
  """

  all_tests = [
      test_case.LinkerTestCase('ForRegularDevice',
                               is_low_memory=False),
      test_case.LinkerTestCase('ForLowMemoryDevice',
                               is_low_memory=True) ]

  def TestRunnerFactory(device, shard_index):
    return test_runner.LinkerTestRunner(
        device, options.tool, options.push_deps,
        options.cleanup_test_files)

  return (TestRunnerFactory, all_tests)
