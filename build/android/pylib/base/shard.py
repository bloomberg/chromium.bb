# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Implements test sharding logic."""

import logging
import threading

from pylib import android_commands
from pylib import forwarder
from pylib.utils import reraiser_thread

import test_result


class _Test(object):
  """Holds a test with additional metadata."""
  def __init__(self, test, tries=0):
    """Initializes the _Test object.

    Args:
      test: the test.
      tries: number of tries so far.
    """
    self.test = test
    self.tries = tries


class _TestCollection(object):
  """A threadsafe collection of tests.

  Args:
    tests: list of tests to put in the collection.
  """
  def __init__(self, tests=[]):
    self._lock = threading.Lock()
    self._tests = []
    self._tests_in_progress = 0
    # Used to signal that an item is avaliable or all items have been handled.
    self._item_avaliable_or_all_done = threading.Event()
    for t in tests:
      self.add(t)

  def _pop(self):
    """Pop a test from the collection.

    Waits until a test is avaliable or all tests have been handled.

    Returns:
      A test or None if all tests have been handled.
    """
    while True:
      # Wait for a test to be avaliable or all tests to have been handled.
      self._item_avaliable_or_all_done.wait()
      with self._lock:
        # Check which of the two conditions triggered the signal.
        if self._tests_in_progress == 0:
          return None
        try:
          return self._tests.pop()
        except IndexError:
          # Another thread beat us to the avaliable test, wait again.
          self._item_avaliable_or_all_done.clear()

  def add(self, test):
    """Add an test to the collection.

    Args:
      item: A test to add.
    """
    with self._lock:
      self._tests.append(test)
      self._item_avaliable_or_all_done.set()
      self._tests_in_progress += 1

  def test_completed(self):
    """Indicate that a test has been fully handled."""
    with self._lock:
      self._tests_in_progress -= 1
      if self._tests_in_progress == 0:
        # All tests have been handled, signal all waiting threads.
        self._item_avaliable_or_all_done.set()

  def __iter__(self):
    """Iterate through tests in the collection until all have been handled."""
    while True:
      r = self._pop()
      if r is None:
        break
      yield r


def _RunTestsFromQueue(runner, test_collection, out_results):
  """Runs tests from the test_collection until empty using the given runner.

  Adds TestResults objects to the out_results list and may add tests to the
  out_retry list.

  Args:
    runner: A TestRunner object used to run the tests.
    test_collection: A _TestCollection from which to get _Test objects to run.
    out_results: A list to add TestResults to.
  """
  for test in test_collection:
    try:
      if not android_commands.IsDeviceAttached(runner.device):
        # Device is unresponsive, stop handling tests on this device.
        msg = 'Device %s is unresponsive.' % runner.device
        logging.warning(msg)
        raise android_commands.errors.DeviceUnresponsiveError(msg)
      result, retry = runner.RunTest(test.test)
      test.tries += 1
      if retry and test.tries <= 3:
        # Retry non-passing results, only record passing results.
        out_results.append(test_result.TestResults.FromRun(ok=result.ok))
        logging.warning('****Retrying test, try #%s.' % test.tries)
        test_collection.add(_Test(test=retry, tries=test.tries))
      else:
        # All tests passed or retry limit reached. Either way, record results.
        out_results.append(result)
    except android_commands.errors.DeviceUnresponsiveError:
      # Device is unresponsive, stop handling tests on this device and ensure
      # current test gets runs by another device. Don't reraise this exception
      # on the main thread.
      test_collection.add(test)
      return
    except:
      # An unhandleable exception, ensure tests get run by another device and
      # reraise this exception on the main thread.
      test_collection.add(test)
      raise
    finally:
      # Retries count as separate tasks so always mark the popped test as done.
      test_collection.test_completed()


def _SetUp(runner_factory, device, out_runners):
  """Creates a test runner for each device and calls SetUp() in parallel.

  Note: if a device is unresponsive the corresponding TestRunner will not be
    added to out_runners.

  Args:
    runner_factory: callable that takes a device and returns a TestRunner.
    device: the device serial number to set up.
    out_runners: list to add the successfully set up TestRunner object.
  """
  try:
    logging.warning('*****Creating shard for %s.', device)
    runner = runner_factory(device)
    runner.SetUp()
    out_runners.append(runner)
  except android_commands.errors.DeviceUnresponsiveError as e:
    logging.warning('****Failed to create shard for %s: [%s]', (device, e))


def _RunAllTests(runners, tests):
  """Run all tests using the given TestRunners.

  Args:
    runners: a list of TestRunner objects.
    tests: a list of Tests to run using the given TestRunners.

  Returns:
    A TestResults object.
  """
  logging.warning('****Running %s tests with %s test runners.' %
                  (len(tests), len(runners)))
  tests_collection = _TestCollection([_Test(t) for t in tests])
  results = []
  workers = reraiser_thread.ReraiserThreadGroup([reraiser_thread.ReraiserThread(
      _RunTestsFromQueue, [r, tests_collection, results]) for r in runners])
  workers.StartAll()
  workers.JoinAll()
  return test_result.TestResults.FromTestResults(results)


def _CreateRunners(runner_factory, devices):
  """Creates a test runner for each device and calls SetUp() in parallel.

  Note: if a device is unresponsive the corresponding TestRunner will not be
    included in the returned list.

  Args:
    runner_factory: callable that takes a device and returns a TestRunner.
    devices: list of device serial numbers as strings.

  Returns:
    A list of TestRunner objects.
  """
  logging.warning('****Creating %s test runners.' % len(devices))
  test_runners = []
  threads = reraiser_thread.ReraiserThreadGroup(
      [reraiser_thread.ReraiserThread(_SetUp, [runner_factory, d, test_runners])
       for d in devices])
  threads.StartAll()
  threads.JoinAll()
  return test_runners


def _TearDownRunners(runners):
  """Calls TearDown() for each test runner in parallel.
  Args:
    runners: a list of TestRunner objects.
  """
  threads = reraiser_thread.ReraiserThreadGroup(
      [reraiser_thread.ReraiserThread(runner.TearDown)
       for runner in runners])
  threads.StartAll()
  threads.JoinAll()


def ShardAndRunTests(runner_factory, devices, tests, build_type='Debug'):
  """Run all tests on attached devices, retrying tests that don't pass.

  Args:
    runner_factory: callable that takes a device and returns a TestRunner.
    devices: list of attached device serial numbers as strings.
    tests: list of tests to run.
    build_type: either 'Debug' or 'Release'.

  Returns:
    A test_result.TestResults object.
  """
  forwarder.Forwarder.KillHost(build_type)
  runners = _CreateRunners(runner_factory, devices)
  try:
    return _RunAllTests(runners, tests)
  finally:
    try:
      _TearDownRunners(runners)
    except android_commands.errors.DeviceUnresponsiveError as e:
      logging.warning('****Device unresponsive during TearDown: [%s]', e)
    finally:
      forwarder.Forwarder.KillHost(build_type)
