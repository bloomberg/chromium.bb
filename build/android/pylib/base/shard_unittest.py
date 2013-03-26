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

import base_test_result
import shard


class TestException(Exception):
  pass


class MockRunner(object):
  """A mock TestRunner."""
  def __init__(self, device='0', shard_index=0):
    self.device = device
    self.shard_index = shard_index
    self.setups = 0
    self.teardowns = 0

  def RunTest(self, test):
    results = base_test_result.TestRunResults()
    results.AddResult(
        base_test_result.BaseTestResult(test, base_test_result.ResultType.PASS))
    return (results, None)

  def SetUp(self):
    self.setups += 1

  def TearDown(self):
    self.teardowns += 1


class MockRunnerFail(MockRunner):
  def RunTest(self, test):
    results = base_test_result.TestRunResults()
    results.AddResult(
        base_test_result.BaseTestResult(test, base_test_result.ResultType.FAIL))
    return (results, test)


class MockRunnerFailTwice(MockRunner):
  def __init__(self, device='0', shard_index=0):
    super(MockRunnerFailTwice, self).__init__(device, shard_index)
    self._fails = 0

  def RunTest(self, test):
    self._fails += 1
    results = base_test_result.TestRunResults()
    if self._fails <= 2:
      results.AddResult(base_test_result.BaseTestResult(
          test, base_test_result.ResultType.FAIL))
      return (results, test)
    else:
      results.AddResult(base_test_result.BaseTestResult(
          test, base_test_result.ResultType.PASS))
      return (results, None)


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
    run_results = base_test_result.TestRunResults()
    for r in results:
      run_results.AddTestRunResults(r)
    return run_results

  def testRunTestsFromQueue(self):
    results = TestFunctions._RunTests(MockRunner(), ['a', 'b'])
    self.assertEqual(len(results.GetPass()), 2)
    self.assertEqual(len(results.GetNotPass()), 0)

  def testRunTestsFromQueueRetry(self):
    results = TestFunctions._RunTests(MockRunnerFail(), ['a', 'b'])
    self.assertEqual(len(results.GetPass()), 0)
    self.assertEqual(len(results.GetFail()), 2)

  def testRunTestsFromQueueFailTwice(self):
    results = TestFunctions._RunTests(MockRunnerFailTwice(), ['a', 'b'])
    self.assertEqual(len(results.GetPass()), 2)
    self.assertEqual(len(results.GetNotPass()), 0)

  def testSetUp(self):
    runners = []
    counter = shard._ThreadSafeCounter()
    shard._SetUp(MockRunner, '0', runners, counter)
    self.assertEqual(len(runners), 1)
    self.assertEqual(runners[0].setups, 1)

  def testThreadSafeCounter(self):
    counter = shard._ThreadSafeCounter()
    for i in xrange(5):
      self.assertEqual(counter.GetAndIncrement(), i)


class TestThreadGroupFunctions(unittest.TestCase):
  """Tests for shard._RunAllTests and shard._CreateRunners."""
  def setUp(self):
    self.tests = ['a', 'b', 'c', 'd', 'e', 'f', 'g']

  def testCreate(self):
    runners = shard._CreateRunners(MockRunner, ['0', '1'])
    for runner in runners:
      self.assertEqual(runner.setups, 1)
    self.assertEqual(set([r.device for r in runners]),
                     set(['0', '1']))
    self.assertEqual(set([r.shard_index for r in runners]),
                     set([0, 1]))

  def testRun(self):
    runners = [MockRunner('0'), MockRunner('1')]
    results = shard._RunAllTests(runners, self.tests)
    self.assertEqual(len(results.GetPass()), len(self.tests))

  def testTearDown(self):
    runners = [MockRunner('0'), MockRunner('1')]
    shard._TearDownRunners(runners)
    for runner in runners:
      self.assertEqual(runner.teardowns, 1)

  def testRetry(self):
    runners = shard._CreateRunners(MockRunnerFail, ['0', '1'])
    results = shard._RunAllTests(runners, self.tests)
    self.assertEqual(len(results.GetFail()), len(self.tests))

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
    self.assertEqual(len(results.GetPass()), 3)

  def testFailing(self):
    results = TestShard._RunShard(MockRunnerFail)
    self.assertEqual(len(results.GetPass()), 0)
    self.assertEqual(len(results.GetFail()), 3)


if __name__ == '__main__':
  unittest.main()
