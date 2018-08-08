#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
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
import stat
import sys
import xml.dom.minidom as minidom

_THIS_DIR = os.path.abspath(os.path.dirname(__file__))
sys.path.insert(1, os.path.join(_THIS_DIR, os.pardir))

import chrome_paths
import test_environment
import util
import glob
import time

if util.IsLinux():
  sys.path.insert(0, os.path.join(chrome_paths.GetSrc(), 'build', 'android'))
  from pylib import constants


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
         chromedriver_path, chrome_path, log_path, android_package_key,
         verbose, debug):
  """Run the WebDriver Java tests and return the test results.

  Args:
    java_tests_src_dir: the java test source code directory.
    test_filter: the filter to use when choosing tests to run. Format is same
        as Google C++ Test format.
    chromedriver_path: path to ChromeDriver exe.
    chrome_path: path to Chrome exe.
    log_path: path to server log.
    android_package_key: name of Chrome's Android package.
    verbose: whether the output should be verbose.
    debug: whether the tests should wait until attached by a debugger.

  Returns:
    A list of |TestResult|s.
  """

  sys_props = ['selenium.browser=chrome',
               'webdriver.chrome.driver=' + os.path.abspath(chromedriver_path)]
  if chrome_path:
    if util.IsLinux() and android_package_key is None:
      # Workaround for crbug.com/611886 and
      # https://bugs.chromium.org/p/chromedriver/issues/detail?id=1695
      chrome_wrapper_path = os.path.join(java_tests_src_dir, 'chrome-wrapper-no-sandbox')
      with open(chrome_wrapper_path, 'w') as f:
        f.write('#!/bin/sh\n')
        f.write('exec "%s" --no-sandbox --disable-gpu "$@"\n' %
            os.path.abspath(chrome_path))
      st = os.stat(chrome_wrapper_path)
      os.chmod(chrome_wrapper_path, st.st_mode | stat.S_IEXEC)
    elif util.IsMac():
      # Use srgb color profile, otherwise the default color profile on Mac
      # causes some color adjustments, so screenshots have unexpected colors.
      chrome_wrapper_path = os.path.join(java_tests_src_dir, 'chrome-wrapper')
      with open(chrome_wrapper_path, 'w') as f:
        f.write('#!/bin/sh\n')
        f.write('exec "%s" --force-color-profile=srgb "$@"\n' %
            os.path.abspath(chrome_path))
      st = os.stat(chrome_wrapper_path)
      os.chmod(chrome_wrapper_path, st.st_mode | stat.S_IEXEC)
    else:
      chrome_wrapper_path = os.path.abspath(chrome_path)
    sys_props += ['webdriver.chrome.binary=' + chrome_wrapper_path]
  if log_path:
    sys_props += ['webdriver.chrome.logfile=' + log_path]
  if android_package_key:
    android_package = constants.PACKAGE_INFO[android_package_key].package
    sys_props += ['webdriver.chrome.android_package=' + android_package]
    if android_package_key == 'chromedriver_webview_shell':
      android_activity = constants.PACKAGE_INFO[android_package_key].activity
      android_process = '%s:main' % android_package
      sys_props += ['webdriver.chrome.android_activity=' + android_activity]
      sys_props += ['webdriver.chrome.android_process=' + android_process]
  if test_filter:
    # Test jar actually takes a regex. Convert from glob.
    test_filter = test_filter.replace('*', '.*')
    sys_props += ['filter=' + test_filter]

  jvm_args = []
  if debug:
    transport = 'dt_socket'
    if util.IsWindows():
      transport = 'dt_shmem'
    jvm_args += ['-agentlib:jdwp=transport=%s,server=y,suspend=y,'
                 'address=33081' % transport]

  return _RunAntTest(java_tests_src_dir, test_filter, chromedriver_path,
                     chrome_path, log_path, android_package_key,
                     jvm_args, verbose, debug, sys_props)

def _RunAntTest(java_tests_src_dir, test_filter, chromedriver_path,
                    chrome_path, log_path, android_package_key,
                    jvm_args, verbose, debug, sys_props):
  """Runs a single Ant JUnit test suite and returns the |TestResult|s.

  Args:
    test_dir: the directory to run the tests in.
    test_class: the name of the JUnit test suite class to run.
    class_path: the Java class path used when running the tests, colon delimited
    sys_props: Java system properties to set when running the tests.
    jvm_args: Java VM command line args to use.
    verbose: whether the output should be verbose.

  Returns:
    A list of |TestResult|s.
  """
  def _CreateBuildConfig(java_tests_src_dir, jvm_args, sys_props):
    path_element = []

    for name in glob.glob(java_tests_src_dir + "/jar/*.jar"):
      path_element.append('\t<pathelement location=\"%s\" />' % name)

    build_xml = '\n'.join([
    '<project>',
    ' <property name="test.class.name" value="org.openqa.selenium.chrome.ChromeDriverTests" />',
    ' <path id="test.classpath">',
    '\n'.join(path_element),
    '</path>'])

    def _SystemPropToXml(prop):
      key, value = prop.split('=')
      return '<sysproperty key="%s" value="%s"/>' % (key, value)
    def _JvmArgToXml(arg):
      return '<jvmarg value="%s"/>' % arg
    build_xml += '\n'.join([
      '  <target name="test">',
      '    <junit %s>' % ' '.join(junit_props),
      '      <formatter type="xml"/>',
      '      ' + '\n      '.join(map(_SystemPropToXml, sys_props)),
      '      ' + '\n      '.join(map(_JvmArgToXml, jvm_args)),
      '      <test name="%s" outfile="%s"/>' % ("org.openqa.selenium.chrome.ChromeDriverTests", "results"),
      '      <classpath refid="test.classpath" />',
      '    </junit>',
      '  </target>',
      '</project>'])

    return build_xml

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
  if verbose:
    junit_props += ['showoutput="yes"']

  ant_file = open(os.path.join(java_tests_src_dir, 'build.xml'), 'w')
  file_contents = _CreateBuildConfig(java_tests_src_dir, jvm_args, sys_props)
  ant_file.write(file_contents)
  ant_file.close()

  if util.IsWindows():
    ant_name = 'ant.bat'
  else:
    ant_name = 'ant'
  code = util.RunCommand([ant_name, 'test'], java_tests_src_dir)
  if code != 0:
    print 'FAILED to run java tests of ChromeDriverTests through ant'
    return
  return _ProcessResults(os.path.join(java_tests_src_dir, 'results.xml'))


def PrintTestResults(results):
  """Prints the given results in a format recognized by the buildbot."""

  # If no results that means something went wrong and we should return non zero value
  if len(results) == 0:
    print 'No tests were run'
    return 1

  failures = []
  failure_names = []
  for result in results:
    if not result.IsPass():
      failures += [result]
      failure_names += ['.'.join(result.GetName().split('.')[-2:])]

  print 'Ran %s tests' % len(results)
  print 'Failed %s:' % len(failures)
  util.AddBuildStepText('failed %s/%s' % (len(failures), len(results)))
  for result in failures:
    print '=' * 80
    print '=' * 10, result.GetName(), '(%ss)' % result.GetTime()
    print result.GetFailureMessage()
    if len(failures) < 10:
      util.AddBuildStepText('.'.join(result.GetName().split('.')[-2:]))
  print 'Rerun failing tests with filter:', ':'.join(failure_names)
  return len(failures)


def main():
  parser = optparse.OptionParser()
  parser.add_option(
      '', '--verbose', action='store_true', default=False,
      help='Whether output should be verbose')
  parser.add_option(
      '', '--debug', action='store_true', default=False,
      help='Whether to wait to be attached by a debugger')
  parser.add_option(
      '', '--chromedriver', type='string', default=None,
      help='Path to a build of the chromedriver library(REQUIRED!)')
  parser.add_option(
      '', '--chrome', type='string', default=None,
      help='Path to a build of the chrome binary')
  parser.add_option(
      '', '--log-path',
      help='Output verbose server logs to this file')
  parser.add_option(
      '', '--chrome-version', default='HEAD',
      help='Version of chrome. Default is \'HEAD\'')
  parser.add_option(
      '', '--android-package', help='Android package key')
  parser.add_option(
      '', '--filter', type='string', default=None,
      help='Filter for specifying what tests to run, "*" will run all. E.g., '
           '*testShouldReturnTitleOfPageIfSet')
  parser.add_option(
      '', '--also-run-disabled-tests', action='store_true', default=False,
      help='Include disabled tests while running the tests')
  parser.add_option(
      '', '--isolate-tests', action='store_true', default=False,
      help='Relaunch the jar test harness after each test')
  options, _ = parser.parse_args()

  options.chromedriver = util.GetAbsolutePathOfUserPath(options.chromedriver)
  if options.chromedriver is None or not os.path.exists(options.chromedriver):
    parser.error('chromedriver is required or the given path is invalid.' +
                 'Please run "%s --help" for help' % __file__)

  if options.android_package:
    if options.android_package not in constants.PACKAGE_INFO:
      parser.error('Invalid --android-package')
    if options.chrome_version != 'HEAD':
      parser.error('Android does not support the --chrome-version argument.')
    environment = test_environment.AndroidTestEnvironment(
        options.android_package)
  else:
    environment = test_environment.DesktopTestEnvironment(
        options.chrome_version)

  try:
    environment.GlobalSetUp()
    # Run passed tests when filter is not provided.
    if options.isolate_tests:
      test_filters = environment.GetPassedJavaTests()
    else:
      if options.filter:
        test_filter = options.filter
      else:
        test_filter = '*'
      if not options.also_run_disabled_tests:
        if '-' in test_filter:
          test_filter += ':'
        else:
          test_filter += '-'
        test_filter += ':'.join(environment.GetDisabledJavaTestMatchers())
      test_filters = [test_filter]

    java_tests_src_dir = os.path.join(chrome_paths.GetSrc(), 'chrome', 'test',
                                      'chromedriver', 'third_party',
                                      'java_tests')
    if (not os.path.exists(java_tests_src_dir) or
        not os.listdir(java_tests_src_dir)):
      java_tests_url = ('https://chromium.googlesource.com/chromium/deps'
                        '/webdriver')
      print ('"%s" is empty or it doesn\'t exist. ' % java_tests_src_dir +
             'Need to map ' + java_tests_url + ' to '
             'chrome/test/chromedriver/third_party/java_tests in .gclient.\n'
             'Alternatively, do:\n'
             '  $ cd chrome/test/chromedriver/third_party\n'
             '  $ git clone %s java_tests' % java_tests_url)
      return 1

    results = []
    for filter in test_filters:
      results += _Run(
          java_tests_src_dir=java_tests_src_dir,
          test_filter=filter,
          chromedriver_path=options.chromedriver,
          chrome_path=util.GetAbsolutePathOfUserPath(options.chrome),
          log_path=options.log_path,
          android_package_key=options.android_package,
          verbose=options.verbose,
          debug=options.debug)
    return PrintTestResults(results)
  finally:
    environment.GlobalTearDown()
    if(os.path.exists(os.path.join(java_tests_src_dir, "build.xml"))):
      os.remove(os.path.join(java_tests_src_dir, "build.xml"))
    if(os.path.exists(os.path.join(java_tests_src_dir, "results.xml"))):
      os.remove(os.path.join(java_tests_src_dir, "results.xml"))
    if(os.path.exists(os.path.join(java_tests_src_dir, "chrome-wrapper-no-sandbox"))):
      os.remove(os.path.join(java_tests_src_dir, "chrome-wrapper-no-sandbox"))
    if(os.path.exists(os.path.join(java_tests_src_dir, "chrome-wrapper"))):
      os.remove(os.path.join(java_tests_src_dir, "chrome-wrapper"))

if __name__ == '__main__':
  sys.exit(main())
