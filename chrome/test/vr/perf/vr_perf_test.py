# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import subprocess

class VrPerfTest(object):
  """Base class for all non-Telemetry VR perf tests.

  This class is meant to be subclassed for each platform and test type to
  be run.
  """
  def __init__(self, args):
    super(VrPerfTest, self).__init__()
    self._args = args
    self._test_urls = []

  def RunTests(self):
    """Runs some test on all the URLs provided to the test on creation.

    Repeatedly runs the steps to start Chrome, measure/store metrics, and
    clean up before storing all results to a single file for dashboard
    uploading.
    """
    try:
      self._OneTimeSetup()
      for url in self._test_urls:
        try:
          self._Setup(url)
          self._Run(url)
        finally:
          # We always want to perform teardown even if an exception gets raised.
          self._Teardown()
      self._SaveResultsToFile()
      self._SaveOutput()
    finally:
      self._OneTimeTeardown()

  def _OneTimeSetup(self):
    """Performs any platform-specific setup once before any tests."""
    raise NotImplementedError(
        'Platform-specific setup must be implemented in subclass')

  def _Setup(self, url):
    """Performs any platform-specific setup before each test."""
    raise NotImplementedError(
        'Platform-specific setup must be implemented in subclass')

  def _Run(self, url):
    """Performs the actual test."""
    raise NotImplementedError('Test must be implemented in subclass')

  def _Teardown(self):
    """Performs any platform-specific teardown after each test."""
    raise NotImplementedError(
        'Platform-specific teardown must be implemented in subclass')

  def _OneTimeTeardown(self):
    """Performs any platform-specific teardown after all tests."""
    raise NotImplementedError(
        'Platform-specific teardown must be implemented in subclass')

  def _RunCommand(self, cmd):
    """Runs the given cmd list and returns its output.

    Prints the command's output and exits if any error occurs.

    Returns:
      A string containing the stdout and stderr of the command.
    """
    try:
      return subprocess.check_output(cmd, stderr=subprocess.STDOUT)
    except subprocess.CalledProcessError as e:
      logging.error('Failed command output: %s', e.output)
      raise e

  def _SetChromeCommandLineFlags(self, flags):
    """Sets the commandline flags that Chrome reads on startup."""
    raise NotImplementedError(
        'Command-line flag setting must be implemented in subclass')

  def _SaveResultsToFile(self):
    """Saves test results to a Chrome perf dashboard-compatible JSON file."""
    raise NotImplementedError(
        'Result saving must be implemented in subclass')

  def _SaveOutput(self):
    """Saves the failures and results validity if necessary."""
    if not (hasattr(self._args, 'isolated_script_test_output') and
            self._args.isolated_script_test_output):
      logging.warning('Isolated script output file not specified, not saving')
      return
    with file(self._args.isolated_script_test_output, 'w') as outfile:
      # TODO(bsheedy): Actually check failures/validity
      json.dump({'failures': [], 'valid': True}, outfile)
