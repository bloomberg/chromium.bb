#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs the WebDriver Java acceptance tests.

This script is called from chrome/test/chromedriver/run_all_tests.py and reports
results using the buildbot annotation scheme.

For ChromeDriver documentation, refer to http://code.google.com/p/chromedriver.
"""

import optparse
import os
import shutil
import sys
import xml.dom.minidom as minidom

sys.path.insert(0, os.path.join(os.path.dirname(__file__), os.pardir, 'pylib'))

from common import chrome_paths
from common import util
from continuous_archive import CHROME_26_REVISION

_THIS_DIR = os.path.abspath(os.path.dirname(__file__))


_FAILED_JAVA_TESTS_IN_CHROME_26 = [
    'UploadTest#testFileUploading',
    'AlertsTest']


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


def _Run(java_tests_src_dir, test_filter,
         chromedriver_path, chrome_path, android_package):
  """Run the WebDriver Java tests and return the test results.

  Args:
    java_tests_src_dir: the java test source code directory.
    test_filter: the filter to use when choosing tests to run. Format is
        ClassName#testMethod.
    chromedriver_path: path to ChromeDriver exe.
    chrome_path: path to Chrome exe.
    android_package: name of Chrome's Android package.

  Returns:
    A list of |TestResult|s.

  Raises:
    RuntimeError: Raised if test_filter is invalid.
  """
  test_dir = util.MakeTempDir()
  keystore_path = ('java', 'client', 'test', 'keystore')
  required_dirs = [keystore_path[:-1],
                   ('javascript',),
                   ('third_party', 'closure', 'goog')]
  for required_dir in required_dirs:
    os.makedirs(os.path.join(test_dir, *required_dir))

  test_jar = 'test-standalone.jar'
  shutil.copyfile(os.path.join(java_tests_src_dir, 'keystore'),
                  os.path.join(test_dir, *keystore_path))
  shutil.copytree(os.path.join(java_tests_src_dir, 'common'),
                  os.path.join(test_dir, 'common'))
  shutil.copyfile(os.path.join(java_tests_src_dir, test_jar),
                  os.path.join(test_dir, test_jar))

  sys_props = ['selenium.browser=chrome',
               'webdriver.chrome.driver=' + os.path.abspath(chromedriver_path),
               'java.library.path=' + test_dir]
  if chrome_path is not None:
    sys_props += ['webdriver.chrome.binary=' + os.path.abspath(chrome_path)]
  if android_package is not None:
    sys_props += ['webdriver.chrome.android_package=' + android_package]
  if test_filter and test_filter != '*':
    classes = []
    methods = []
    cases = test_filter.split(',')
    for case in cases:
      parts = case.split('#')
      if len(parts) > 2:
        raise RuntimeError('Filter should be of form: SomeClass#testMethod')
      elif len(parts) == 2:
        methods += [parts[1]]
      if parts:
        classes += [parts[0]]
    sys_props += ['only_run=' + ','.join(classes)]
    sys_props += ['method=' + ','.join(methods)]

  return _RunAntTest(
      test_dir, 'org.openqa.selenium.chrome.ChromeDriverTests',
      test_jar, sys_props)


def _RunAntTest(test_dir, test_class, class_path, sys_props):
  """Runs a single Ant JUnit test suite and returns the |TestResult|s.

  Args:
    test_dir: the directory to run the tests in.
    test_class: the name of the JUnit test suite class to run.
    class_path: the Java class path used when running the tests.
    sys_props: Java system properties to set when running the tests.

  Returns:
    A list of |TestResult|s.
  """

  def _CreateBuildConfig(test_name, results_file, class_path, junit_props,
                         sys_props):
    def _SystemPropToXml(prop):
      key, value = prop.split('=')
      return '<sysproperty key="%s" value="%s"/>' % (key, value)
    jvmarg = ''
    if util.IsMac():
      # In Mac, the chromedriver library is a 32-bit build. So run 32-bit java.
      jvmarg = '<jvmarg value="-d32"/>'
    return '\n'.join([
        '<project>',
        '  <target name="test">',
        '    <junit %s>' % ' '.join(junit_props),
        '      ' + jvmarg,
        '      <formatter type="xml"/>',
        '      <classpath>',
        '        <pathelement location="%s"/>' % class_path,
        '      </classpath>',
        '      ' + '\n      '.join(map(_SystemPropToXml, sys_props)),
        '      <test name="%s" outfile="%s"/>' % (test_name, results_file),
        '    </junit>',
        '  </target>',
        '</project>'])

  def _ProcessResults(results_path):
    doc = minidom.parse(results_path)
    tests = []
    for test in doc.getElementsByTagName('testcase'):
      name = test.getAttribute('classname') + '.' + test.getAttribute('name')
      time = test.getAttribute('time')
      failure = None
      error_nodes = test.getElementsByTagName('error')
      failure_nodes = test.getElementsByTagName('failure')
      if error_nodes:
        failure = error_nodes[0].childNodes[0].nodeValue
      elif failure_nodes:
        failure = failure_nodes[0].childNodes[0].nodeValue
      tests += [TestResult(name, time, failure)]
    return tests

  junit_props = ['printsummary="yes"',
                 'fork="yes"',
                 'haltonfailure="no"',
                 'haltonerror="no"']

  ant_file = open(os.path.join(test_dir, 'build.xml'), 'w')
  ant_file.write(_CreateBuildConfig(
      test_class, 'results', class_path, junit_props, sys_props))
  ant_file.close()

  if util.IsWindows():
    ant_name = 'ant.bat'
  else:
    ant_name = 'ant'
  code = util.RunCommand([ant_name, 'test'], cwd=test_dir)
  if code != 0:
    print 'FAILED to run java tests of %s through ant' % test_class
    return
  return _ProcessResults(os.path.join(test_dir, 'results.xml'))


def PrintTestResults(results):
  """Prints the given results in a format recognized by the buildbot."""
  failures = []
  for result in results:
    if not result.IsPass():
      failures += [result]

  print 'Ran %s tests' % len(results)
  print 'Failed %s:' % len(failures)
  for result in failures:
    print '=' * 80
    print '=' * 10, result.GetName(), '(%ss)' % result.GetTime()
    print result.GetFailureMessage()
  if failures:
    print '@@@STEP_TEXT@Failed %s tests@@@' % len(failures)
  return len(failures)


def main():
  parser = optparse.OptionParser()
  parser.add_option(
      '', '--chromedriver', type='string', default=None,
      help='Path to a build of the chromedriver library(REQUIRED!)')
  parser.add_option(
      '', '--chrome', type='string', default=None,
      help='Path to a build of the chrome binary')
  parser.add_option(
      '', '--chrome-revision', default='HEAD',
      help='Revision of chrome. Default is HEAD.')
  parser.add_option(
      '', '--android-package', type='string', default=None,
      help='Name of Chrome\'s Android package')
  parser.add_option(
      '', '--filter', type='string', default=None,
      help='Filter for specifying what tests to run, "*" will run all. E.g., '
           'AppCacheTest,ElementFindingTest#testShouldReturnTitleOfPageIfSet.')
  options, args = parser.parse_args()

  if options.chromedriver is None or not os.path.exists(options.chromedriver):
    parser.error('chromedriver is required or the given path is invalid.' +
                 'Please run "%s --help" for help' % __file__)

  # Run passed tests when filter is not provided.
  test_filter = options.filter
  if test_filter is None:
    passed_java_tests = []
    with open(os.path.join(_THIS_DIR, 'passed_java_tests.txt'), 'r') as f:
      for line in f:
        java_test = line.strip('\n')
        # Filter out failed tests for chrome 26.
        if options.chrome_revision == CHROME_26_REVISION:
          suite_name = java_test.split('#')[0]
          if (java_test in _FAILED_JAVA_TESTS_IN_CHROME_26 or
              suite_name in _FAILED_JAVA_TESTS_IN_CHROME_26):
            continue
        passed_java_tests.append(java_test)
    test_filter = ','.join(passed_java_tests)

  java_tests_src_dir = os.path.join(chrome_paths.GetSrc(), 'chrome', 'test',
                                    'chromedriver', 'third_party', 'java_tests')
  if (not os.path.exists(java_tests_src_dir) or
      not os.listdir(java_tests_src_dir)):
    print ('"%s" is empty or it doesn\'t exist.' % java_tests_src_dir +
           'Should add "deps/third_party/webdriver" to source checkout config')
    return 1

  return PrintTestResults(_Run(
      java_tests_src_dir=java_tests_src_dir,
      test_filter=test_filter,
      chromedriver_path=options.chromedriver,
      chrome_path=options.chrome,
      android_package=options.android_package))


if __name__ == '__main__':
  sys.exit(main())
