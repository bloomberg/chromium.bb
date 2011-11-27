#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import optparse
import os
import sys
import types
import unittest

from chromedriver_launcher import ChromeDriverLauncher
import py_unittest_util
import test_paths

# Add the PYTHON_BINDINGS first so that our 'test' module is found instead of
# Python's.
sys.path = [test_paths.PYTHON_BINDINGS] + sys.path

from selenium.webdriver.remote.webdriver import WebDriver


# Implementation inspired from unittest.main()
class Main(object):
  """Main program for running WebDriver tests."""

  _options, _args = None, None
  TESTS_FILENAME = 'WEBDRIVER_TESTS'
  _platform_map = {
    'win32':  'win',
    'darwin': 'mac',
    'linux2': 'linux',
    'linux3': 'linux',
  }
  TEST_PREFIX = 'selenium.test.selenium.webdriver.common.'

  def __init__(self):
    self._ParseArgs()
    self._Run()

  def _ParseArgs(self):
    """Parse command line args."""
    parser = optparse.OptionParser()
    parser.add_option(
        '-v', '--verbose', action='store_true', default=False,
        help='Output verbosely.')
    parser.add_option(
        '', '--log-file', type='string', default=None,
        help='Provide a path to a file to which the logger will log')
    parser.add_option(
        '', '--filter', type='string', default='*',
        help='Filter for specifying what tests to run, google test style.')
    parser.add_option(
        '', '--driver-exe', type='string', default=None,
        help='Path to the default ChromeDriver executable to use.')
    parser.add_option(
        '', '--chrome-exe', type='string', default=None,
        help='Path to the default Chrome executable to use.')
    parser.add_option(
        '', '--list', action='store_true', default=False,
        help='List tests instead of running them.')

    self._options, self._args = parser.parse_args()

    # Setup logging - start with defaults
    level = logging.WARNING
    format = None

    if self._options.verbose:
      level=logging.DEBUG
      format='%(asctime)s %(levelname)-8s %(message)s'

    logging.basicConfig(level=level, format=format,
                        filename=self._options.log_file)

  @staticmethod
  def _IsTestClass(obj):
    """Returns whether |obj| is a unittest.TestCase."""
    return isinstance(obj, (type, types.ClassType)) and \
           issubclass(obj, unittest.TestCase)

  @staticmethod
  def _GetModuleFromName(test_name):
    """Return the module from the given test name.

    Args:
      test_name: dot-separated string for a module, a test case or a test
                 method
        Examples: omnibox  (a module)
                  omnibox.OmniboxTest  (a test case)
                  omnibox.OmniboxTest.testA  (a test method)

    Returns:
      tuple with first item corresponding to the module and second item
      corresponding to the parts of the name that did not specify the module
        Example: _GetModuleFromName('my_module.MyClass.testThis') returns
                (my_module, ['MyClass', 'testThis'])
    """
    parts = test_name.split('.')
    parts_copy = parts[:]
    while parts_copy:
      try:
        module = __import__('.'.join(parts_copy))
        break
      except ImportError:
        del parts_copy[-1]
        if not parts_copy: raise

    for comp in parts[1:]:
      if type(getattr(module, comp)) is not types.ModuleType:
        break
      module = getattr(module, comp)
    return (module, parts[len(parts_copy):])

  @staticmethod
  def _GetTestsFromName(name):
    """Get a list of all test names from the given string.

    Args:
      name: dot-separated string for a module, a test case or a test method.
            Examples: omnibox  (a module)
                      omnibox.OmniboxTest  (a test case)
                      omnibox.OmniboxTest.testA  (a test method)

    Returns:
      [omnibox.OmniboxTest.testA, omnibox.OmniboxTest.testB, ...]
    """
    def _GetTestsFromTestCase(class_obj):
      """Return all test method names from given class object."""
      return [class_obj.__name__ + '.' + x for x in dir(class_obj) if
              x.startswith('test')]

    def _GetTestsFromModule(module):
      """Return all test method names from the given module object."""
      tests = []
      for name in dir(module):
        obj = getattr(module, name)
        if Main._IsTestClass(obj):
          tests.extend([module.__name__ + '.' + x for x in
                        _GetTestsFromTestCase(obj)])
      return tests
    (obj, parts) = Main._GetModuleFromName(name)
    for comp in parts:
      obj = getattr(obj, comp)

    if type(obj) == types.ModuleType:
      return _GetTestsFromModule(obj)
    elif Main._IsTestClass(obj):
      return [module.__name__ + '.' + x for x in _GetTestsFromTestCase(obj)]
    elif type(obj) == types.UnboundMethodType:
      return [name]
    else:
      logging.warn('No tests in "%s"' % name)
      return []

  @staticmethod
  def _EvalDataFrom(filename):
    """Return eval of python code from given file.

    The datastructure used in the file will be preserved.
    """
    data_file = os.path.join(filename)
    contents = open(data_file).read()
    try:
      ret = eval(contents, {'__builtins__': None}, None)
    except:
      print >>sys.stderr, '%s is an invalid data file.' % data_file
      raise
    return ret

  def _GetTestNamesFrom(self, filename):
    modules = self._EvalDataFrom(filename)
    all_names = modules.get('all', []) + \
                modules.get(self._platform_map[sys.platform], [])
    args = []
    excluded = []
    # Find all excluded tests.  Excluded tests begin with '-'.
    for name in all_names:
      if name.startswith('-'):  # Exclude
        excluded.extend(self._GetTestsFromName(self.TEST_PREFIX + name[1:]))
      else:
        args.extend(self._GetTestsFromName(self.TEST_PREFIX + name))
    for name in excluded:
      args.remove(name)
    if excluded:
      logging.debug('Excluded %d test(s): %s' % (len(excluded), excluded))
    return args

  def _FakePytestHack(self):
    """Adds a fake 'pytest' module to the system modules.

    A single test in text_handling_tests.py depends on the pytest module for
    its test skipping capabilities. Without pytest, we can not run any tests
    in the text_handling_tests.py module.

    We are not sure we want to add pytest to chrome's third party dependencies,
    so for now create a fake pytest module so that we can at least import and
    run all the tests that do not depend on it. Those depending on it are
    disabled.
    """
    import imp
    sys.modules['pytest'] = imp.new_module('pytest')
    sys.modules['pytest'].mark = imp.new_module('mark')
    sys.modules['pytest'].mark.ignore_chrome = lambda x: x

  def _Run(self):
    """Run the tests."""
    # TODO(kkania): Remove this hack.
    self._FakePytestHack()

    # In the webdriver tree, the python 'test' module is moved under the root
    # 'selenium' one for testing. Here we mimic that by setting the 'selenium'
    # module's 'test' attribute and adding 'selenium.test' to the system
    # modules.
    import selenium
    import test
    selenium.test = test
    sys.modules['selenium.test'] = test

    # Load and decide which tests to run.
    test_names = self._GetTestNamesFrom(
        os.path.join(os.path.dirname(__file__), self.TESTS_FILENAME))
    all_tests_suite = unittest.defaultTestLoader.loadTestsFromNames(test_names)
    filtered_suite = py_unittest_util.FilterTestSuite(
        all_tests_suite, self._options.filter)

    if self._options.list is True:
      print '\n'.join(py_unittest_util.GetTestNamesFromSuite(filtered_suite))
      sys.exit(0)

    # The tests expect to run with preset 'driver' and 'webserver' class
    # properties.
    driver_exe = self._options.driver_exe or test_paths.CHROMEDRIVER_EXE
    chrome_exe = self._options.chrome_exe or test_paths.CHROME_EXE
    if driver_exe is None or not os.path.exists(os.path.expanduser(driver_exe)):
      raise RuntimeError('ChromeDriver could not be found')
    if chrome_exe is None or not os.path.exists(os.path.expanduser(chrome_exe)):
      raise RuntimeError('Chrome could not be found')
    driver_exe = os.path.expanduser(driver_exe)
    chrome_exe = os.path.expanduser(chrome_exe)
    server = ChromeDriverLauncher(
        os.path.expanduser(driver_exe), test_paths.WEBDRIVER_TEST_DATA).Launch()
    driver = WebDriver(server.GetUrl(),
                       {'chrome.binary': os.path.expanduser(chrome_exe)})
    # The tests expect a webserver. Since ChromeDriver also operates as one,
    # just pass this dummy class with the right info.
    class DummyWebserver:
      pass
    webserver = DummyWebserver()
    webserver.port = server.GetPort()
    for test in py_unittest_util.GetTestsFromSuite(filtered_suite):
      test.__class__.driver = driver
      test.__class__.webserver = webserver

    verbosity = 1
    if self._options.verbose:
      verbosity = 2
    result = py_unittest_util.GTestTextTestRunner(verbosity=verbosity).run(
        filtered_suite)
    driver.quit()
    server.Kill()
    sys.exit(not result.wasSuccessful())


if __name__ == '__main__':
  Main()
