# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Implements test sharding logic."""

import logging
import sys
import threading

from pylib import android_commands
from pylib import forwarder

import test_result


class _Worker(threading.Thread):
  """Runs tests from the test_queue using the given runner in a separate thread.

  Places results in the out_results.
  """
  def __init__(self, runner, test_queue, out_results, out_retry):
    """Initializes the worker.

    Args:
      runner: A TestRunner object used to run the tests.
      test_queue: A list from which to get tests to run.
      out_results: A list to add TestResults to.
      out_retry: A list to add tests to retry.
    """
    super(_Worker, self).__init__()
    self.daemon = True
    self._exc_info = None
    self._runner = runner
    self._test_queue = test_queue
    self._out_results = out_results
    self._out_retry = out_retry

  #override
  def run(self):
    """Run tests from the queue in a seperate thread until it is empty.

    Adds TestResults objects to the out_results list and may add tests to the
    out_retry list.
    """
    try:
      while True:
        test = self._test_queue.pop()
        result, retry = self._runner.Run(test)
        self._out_results.append(result)
        if retry:
          self._out_retry.append(retry)
    except IndexError:
      pass
    except:
      self._exc_info = sys.exc_info()
      raise

  def ReraiseIfException(self):
    """Reraise exception if an exception was raised in the thread."""
    if self._exc_info:
      raise self._exc_info[0], self._exc_info[1], self._exc_info[2]


def _RunAllTests(runners, tests):
  """Run all tests using the given TestRunners.

  Args:
    runners: a list of TestRunner objects.
    tests: a list of Tests to run using the given TestRunners.

  Returns:
    Tuple: (list of TestResults, list of tests to retry)
  """
  tests_queue = list(tests)
  workers = []
  results = []
  retry = []
  for r in runners:
    worker = _Worker(r, tests_queue, results, retry)
    worker.start()
    workers.append(worker)
  while workers:
    for w in workers[:]:
      # Allow the main thread to periodically check for keyboard interrupts.
      w.join(0.1)
      if not w.isAlive():
        w.ReraiseIfException()
        workers.remove(w)
  return (results, retry)


def _CreateRunners(runner_factory, devices):
  """Creates a test runner for each device.

  Note: if a device is unresponsive the corresponding TestRunner will not be
    included in the returned list.

  Args:
    runner_factory: callable that takes a device and returns a TestRunner.
    devices: list of device serial numbers as strings.

  Returns:
    A list of TestRunner objects.
  """
  test_runners = []
  for index, device in enumerate(devices):
    logging.warning('*' * 80)
    logging.warning('Creating shard %d for %s', index, device)
    logging.warning('*' * 80)
    try:
      test_runners.append(runner_factory(device))
    except android_commands.errors.DeviceUnresponsiveError as e:
      logging.warning('****Failed to create a shard: [%s]', e)
  return test_runners


def ShardAndRunTests(runner_factory, devices, tests, build_type='Debug',
                     tries=3):
  """Run all tests on attached devices, retrying tests that don't pass.

  Args:
    runner_factory: callable that takes a device and returns a TestRunner.
    devices: list of attached device serial numbers as strings.
    tests: list of tests to run.
    build_type: either 'Debug' or 'Release'.
    tries: number of tries before accepting failure.

  Returns:
    A test_result.TestResults object.
  """
  final_results = test_result.TestResults()
  results = test_result.TestResults()
  forwarder.Forwarder.KillHost(build_type)
  try_count = 0
  while tests:
    devices = set(devices).intersection(android_commands.GetAttachedDevices())
    if not devices:
      # There are no visible devices attached, this is unrecoverable.
      msg = 'No devices attached and visible to run tests!'
      logging.critical(msg)
      raise Exception(msg)
    if try_count >= tries:
      # We've retried too many times, return the TestResults up to this point.
      results.ok = final_results.ok
      final_results = results
      break
    try_count += 1
    runners = _CreateRunners(runner_factory, devices)
    try:
      results_list, tests = _RunAllTests(runners, tests)
      results = test_result.TestResults.FromTestResults(results_list)
      final_results.ok += results.ok
    except android_commands.errors.DeviceUnresponsiveError as e:
      logging.warning('****Failed to run test: [%s]', e)
  forwarder.Forwarder.KillHost(build_type)
  return final_results
