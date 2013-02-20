# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for shard.py."""

import os
import sys
import unittest

sys.path.append(os.path.join(os.path.dirname(os.path.realpath(__file__)),
                os.pardir, os.pardir))
from pylib import android_commands

import shard
import test_result


class TestException(Exception):
  pass


class MockRunner(object):
  """A mock TestRunner."""
  def __init__(self, device='0'):
    self.device = device

  def Run(self, test):
    return (test_result.TestResults.FromRun(
                ok=[test_result.BaseTestResult(test, '')]),
            None)


class MockRunnerRetry(MockRunner):
  def Run(self, test):
    return (test_result.TestResults.FromRun(
                failed=[test_result.BaseTestResult(test, '')]),
            test)


class MockRunnerException(MockRunner):
  def Run(self, test):
    raise TestException


class TestWorker(unittest.TestCase):
  """Tests for shard._Worker."""
  @staticmethod
  def _RunRunner(mock_runner, tests):
    results = []
    retry = []
    worker = shard._Worker(mock_runner, tests, results, retry)
    worker.start()
    worker.join()
    worker.Reraise()
    return (results, retry)

  def testRun(self):
    results, retry = TestWorker._RunRunner(MockRunner(), ['a', 'b'])
    self.assertEqual(len(results), 2)
    self.assertEqual(len(retry), 0)

  def testRetry(self):
    results, retry = TestWorker._RunRunner(MockRunnerRetry(), ['a', 'b'])
    self.assertEqual(len(results), 2)
    self.assertEqual(len(retry), 2)

  def testReraise(self):
    with self.assertRaises(TestException):
      TestWorker._RunRunner(MockRunnerException(), ['a', 'b'])


class TestRunAllTests(unittest.TestCase):
  """Tests for shard._RunAllTests and shard._CreateRunners."""
  def setUp(self):
    self.tests = ['a', 'b', 'c', 'd', 'e', 'f', 'g']

  def testRun(self):
    runners = shard._CreateRunners(MockRunner, ['0', '1'])
    results, retry = shard._RunAllTests(runners, self.tests)
    self.assertEqual(len(results), len(self.tests))
    self.assertEqual(len(retry), 0)

  def testRetry(self):
    runners = shard._CreateRunners(MockRunnerRetry, ['0', '1'])
    results, retry = shard._RunAllTests(runners, self.tests)
    self.assertEqual(len(results), len(self.tests))
    self.assertEqual(len(retry), len(self.tests))

  def testReraise(self):
    runners = shard._CreateRunners(MockRunnerException, ['0', '1'])
    with self.assertRaises(TestException):
      shard._RunAllTests(runners, self.tests)


class TestShard(unittest.TestCase):
  """Tests for shard.Shard."""
  @staticmethod
  def _RunShard(runner_factory):
    devices = ['0', '1']
    # Mock out android_commands.GetAttachedDevices().
    android_commands.GetAttachedDevices = lambda: devices
    return shard.ShardAndRunTests(runner_factory, devices, ['a', 'b', 'c'])

  def testShard(self):
    results = TestShard._RunShard(MockRunner)
    self.assertEqual(len(results.ok), 3)

  def testFailing(self):
    results = TestShard._RunShard(MockRunnerRetry)
    self.assertEqual(len(results.ok), 0)
    self.assertEqual(len(results.failed), 3)


if __name__ == '__main__':
  unittest.main()
