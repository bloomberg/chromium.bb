# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from pylib.junit import test_runner

def Setup(options):
  """Creates a test runner factory for junit tests.

  Return:
    A (runner_factory, tests) tuple.
  """

  def TestRunnerFactory(_unused_device, _unused_shard_index):
    return test_runner.JavaTestRunner(options)

  return (TestRunnerFactory, ['JUnit tests'])

