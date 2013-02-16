# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Sharder for the instrumentation tests."""

from pylib.base import base_test_sharder
from pylib.base import sharded_tests_queue

import test_runner


class TestSharder(base_test_sharder.BaseTestSharder):
  """Responsible for sharding the tests on the connected devices."""

  def __init__(self, attached_devices, options, tests, apks):
    super(TestSharder, self).__init__(attached_devices,
                                      options.build_type)
    self.options = options
    self.tests = tests
    self.apks = apks

  def SetupSharding(self, tests):
    """Called before starting the shards."""
    base_test_sharder.SetTestsContainer(sharded_tests_queue.ShardedTestsQueue(
        len(self.attached_devices), tests))

  def CreateShardedTestRunner(self, device, index):
    """Creates a sharded test runner.

    Args:
      device: Device serial where this shard will run.
      index: Index of this device in the pool.

    Returns:
      A TestRunner object.
    """
    return test_runner.TestRunner(self.options, device, None, False, index,
                                  self.apks, [])
