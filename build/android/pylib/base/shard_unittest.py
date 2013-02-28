# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for shard.py."""

import os
import sys
import unittest

sys.path.append(os.path.join(os.path.dirname(os.path.realpath(__file__)),
                os.pardir, os.pardir))

# Mock out android_commands.GetAttachedDevices().
from pylib import android_commands
android_commands.GetAttachedDevices = lambda: ['0', '1']

import shard
import test_result


class TestException(Exception):
  pass


class MockRunner(object):
  """A mock TestRunner."""
  def __init__(self, device='0'):
    self.device = device
    self.setups = 0
    self.teardowns = 0

  def RunTest(self, test):
    return (test_result.TestResults.FromRun(
                ok=[test_result.BaseTestResult(test, '')]),
            None)

  def SetUp(self):
    self.setups += 1

  def TearDown(self):
    self.teardowns += 1


class MockRunnerFail(MockRunner):
  def RunTest(self, test):
    return (test_result.TestResults.FromRun(
                failed=[test_result.BaseTestResult(test, '')]),
            test)


class MockRunnerFailTwice(MockRunner):
  def __init__(self, device='0'):
    super(MockRunnerFailTwice, self).__init__(device)
    self._fails = 0

  def RunTest(self, test):
    self._fails += 1
    if self._fails <= 2:
      return (test_result.TestResults.FromRun(
                  failed=[test_result.BaseTestResult(test, '')]),
              test)
    else:
      return (test_result.TestResults.FromRun(
                  ok=[test_result.BaseTestResult(test, '')]),
            None)


class MockRunnerException(MockRunner):
  def RunTest(self, test):
    raise TestException


class TestFunctions(unittest.TestCase):
  """Tests for shard._RunTestsFromQueue."""
  @staticmethod
  def _RunTests(mock_runner, tests):
    results = []
    tests = shard._TestCollection([shard._Test(t) for t in tests])
    shard._RunTestsFromQueue(mock_runner, tests, results)
    return test_result.TestResults.FromTestResults(results)

  def testRunTestsFromQueue(self):
    results = TestFunctions._RunTests(MockRunner(), ['a', 'b'])
    self.assertEqual(len(results.ok), 2)
    self.assertEqual(len(results.GetAllBroken()), 0)

  def testRunTestsFromQueueRetry(self):
    results = TestFunctions._RunTests(MockRunnerFail(), ['a', 'b'])
    self.assertEqual(len(results.ok), 0)
    self.assertEqual(len(results.failed), 2)

  def testRunTestsFromQueueFailTwice(self):
    results = TestFunctions._RunTests(MockRunnerFailTwice(), ['a', 'b'])
    self.assertEqual(len(results.ok), 2)
    self.assertEqual(len(results.GetAllBroken()), 0)

  def testSetUp(self):
    runners = []
    shard._SetUp(MockRunner, '0', runners)
    self.assertEqual(len(runners), 1)
    self.assertEqual(runners[0].setups, 1)


class TestThreadGroupFunctions(unittest.TestCase):
  """Tests for shard._RunAllTests and shard._CreateRunners."""
  def setUp(self):
    self.tests = ['a', 'b', 'c', 'd', 'e', 'f', 'g']

  def testCreate(self):
    runners = shard._CreateRunners(MockRunner, ['0', '1'])
    for runner in runners:
      self.assertEqual(runner.setups, 1)

  def testRun(self):
    runners = [MockRunner('0'), MockRunner('1')]
    results = shard._RunAllTests(runners, self.tests)
    self.assertEqual(len(results.ok), len(self.tests))

  def testTearDown(self):
    runners = [MockRunner('0'), MockRunner('1')]
    shard._TearDownRunners(runners)
    for runner in runners:
      self.assertEqual(runner.teardowns, 1)

  def testRetry(self):
    runners = shard._CreateRunners(MockRunnerFail, ['0', '1'])
    results = shard._RunAllTests(runners, self.tests)
    self.assertEqual(len(results.failed), len(self.tests))

  def testReraise(self):
    runners = shard._CreateRunners(MockRunnerException, ['0', '1'])
    with self.assertRaises(TestException):
      shard._RunAllTests(runners, self.tests)


class TestShard(unittest.TestCase):
  """Tests for shard.Shard."""
  @staticmethod
  def _RunShard(runner_factory):
    return shard.ShardAndRunTests(runner_factory, ['0', '1'], ['a', 'b', 'c'])

  def testShard(self):
    results = TestShard._RunShard(MockRunner)
    self.assertEqual(len(results.ok), 3)

  def testFailing(self):
    results = TestShard._RunShard(MockRunnerFail)
    self.assertEqual(len(results.ok), 0)
    self.assertEqual(len(results.failed), 3)


if __name__ == '__main__':
  unittest.main()
