# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs the WebDriver Java tests."""

import os
import shutil
import xml.dom.minidom as minidom

from common import util


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


def Run(src_dir, test_filter, chromedriver_path, chrome_path):
  """Run the WebDriver Java tests and return the test results.

  Args:
    src_dir: the chromium source checkout directory.
    test_filter: the filter to use when choosing tests to run. Format is
        ClassName#testMethod.
    chromedriver_path: path to ChromeDriver exe.
    chrome_path: path to Chrome exe.

  Returns:
    A list of |TestResult|s.
  """
  test_dir = util.MakeTempDir()
  keystore_path = ('java', 'client', 'test', 'keystore')
  required_dirs = [keystore_path[:-1],
                   ('javascript',),
                   ('third_party', 'closure', 'goog')]
  for required_dir in required_dirs:
    os.makedirs(os.path.join(test_dir, *required_dir))

  test_jar = 'test-standalone.jar'
  java_tests_src_dir = os.path.join(
      src_dir, 'third_party', 'webdriver', 'java_tests')
  shutil.copyfile(os.path.join(java_tests_src_dir, 'keystore'),
                  os.path.join(test_dir, *keystore_path))
  shutil.copytree(os.path.join(java_tests_src_dir, 'common'),
                  os.path.join(test_dir, 'common'))
  shutil.copyfile(os.path.join(java_tests_src_dir, test_jar),
                  os.path.join(test_dir, test_jar))

  sys_props = ['selenium.browser=chrome',
               'webdriver.chrome.driver=' + chromedriver_path]
  if chrome_path is not None:
    sys_props += ['webdriver.chrome.binary=' + chrome_path]
  if test_filter is not None:
    parts = test_filter.split('#')
    if len(parts) > 2:
      raise RuntimeError('Filter should be of form: SomeClass#testMethod')
    elif len(parts) == 2:
      sys_props += ['method=' + parts[1]]
    if len(parts[0]) > 0:
      sys_props += ['only_run=' + parts[0]]

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
  """

  def _CreateBuildConfig(test_name, results_file, class_path, junit_props,
                         sys_props):
    def _SystemPropToXml(prop):
      key, value = prop.split('=')
      return '<sysproperty key="%s" value="%s"/>' % (key, value)
    return '\n'.join([
        '<project>',
        '  <target name="test">',
        '    <junit %s>' % ' '.join(junit_props),
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
      if len(error_nodes) > 0:
        failure = error_nodes[0].childNodes[0].nodeValue
      elif len(failure_nodes) > 0:
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

  util.RunCommand(['ant', 'test'], cwd=test_dir)
  return _ProcessResults(os.path.join(test_dir, 'results.xml'))
