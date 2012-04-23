# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs the WebDriver Java tests."""

import os
import xml.dom.minidom as minidom

import util


class TestResult(object):
  """A result for an attempted single test case."""

  def __init__(self, name, time, failure):
    """Initializes a test result.

    Args:
      name: the full name of the test.
      time: the amount of time the test ran, in seconds.
      failure: the test error or failure message, or None if the test passed.
    """
    self._name = name
    self._time = time
    self._failure = failure

  def GetName(self):
    """Returns the test name."""
    return self._name

  def GetTime(self):
    """Returns the time it took to run the test."""
    return self._time

  def IsPass(self):
    """Returns whether the test passed."""
    return self._failure is None

  def GetFailureMessage(self):
    """Returns the test failure message, or None if the test passed."""
    return self._failure


class _TestTarget(object):
  """A WebDriver test target."""

  def __init__(self, target_path, target_name, test_class):
    """Initializes a test target.

    Args:
      target_path: the full target name that should be passed to WebDriver's go
                   script.
      target_name: the name of the target. This is equal to the 'name' field of
                   the target in the 'build.desc' file.
      test_class: the name of the test class.
    """
    self._target_path = target_path
    self._target_name = target_name
    self._test_class = test_class

  def GetTargetPath(self):
    """Returns the full target name, e.g., //java/somepackage:test:run."""
    return self._target_path

  def GetResultFileName(self):
    """Returns the name of the file which will contain the test results."""
    return 'TEST-%s-%s.xml' % (self._target_name, self._test_class)


# Contains all standard selenium tests that should pass with stable Chrome.
CHROME_TESTS = _TestTarget(
    '//java/client/test/org/openqa/selenium/chrome:test:run',
    'test',
    'org.openqa.selenium.chrome.ChromeDriverTests')


#TODO(kkania): Add target for running all ignored tests.


def Run(test_target, test_filter, webdriver_dir, chromedriver_path,
        chrome_path):
  """Runs the WebDriver Java test target and returns the results.

  Args:
    test_target: a |TestTarget| to run.
    test_filter: the Java test filter to use. Should take the form
                 MyClass#testMethod.
    webdriver_dir: the path to a WebDriver source checkout.
    chromedriver_path: the path to the ChromeDriver to use.
    chrome_path: the path to the Chrome binary to use. If None, ChromeDriver
                 will use the system Chrome installation.

  Returns:
    List of |TestResult|s.
  """
  results_path = os.path.join(webdriver_dir, 'build', 'test_logs',
                              test_target.GetResultFileName())
  if os.path.exists(results_path):
    os.remove(results_path)
  if util.IsWin():
    go_name = 'go.bat'
  else:
    go_name = 'go'
  go = os.path.join(webdriver_dir, go_name)
  command = [go, test_target.GetTargetPath()]
  command += ['haltonerror=false', 'haltonfailure=false', 'log=true']
  command += ['chromedriver=' + chromedriver_path]
  if chrome_path is not None:
    command += ['chrome=' + chrome_path]
  if test_filter is not None:
    parts = test_filter.split('#')
    if len(parts) > 2:
      raise RuntimeError('Filter should be of form: SomeClass#testMethod')
    elif len(parts) == 2:
      command += ['method=' + parts[1]]
    if len(parts[0]) > 0:
      command += ['onlyrun=' + parts[0]]
  print "Running ", ' '.join(command)
  util.RunCommand(command, cwd=webdriver_dir)
  return _ProcessResults(results_path)


def _ProcessResults(results_path):
  """Returns a list of |TestResult|s from the given result file."""
  doc = minidom.parse(results_path)
  tests = []
  for test in doc.getElementsByTagName('testcase'):
    name = test.getAttribute('classname') + '.' + test.getAttribute('name')
    time = test.getAttribute('time')
    failure = None
    error_nodes = test.getElementsByTagName('error')
    failure_nodes = test.getElementsByTagName('failure')
    if len(error_nodes) > 0:
      failure = error_nodes[0].childNodes[0].nodeValue
    elif len(failure_nodes) > 0:
      failure = failure_nodes[0].childNodes[0].nodeValue
    tests += [TestResult(name, time, failure)]
  return tests
