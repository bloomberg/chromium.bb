#!/usr/bin/python

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
import chromedriver_paths
from gtest_text_test_runner import GTestTextTestRunner

# Add the PYTHON_BINDINGS first so that our 'test' module is found instead of
# Python's.
sys.path = [chromedriver_paths.PYTHON_BINDINGS] + sys.path

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
  }
  TEST_PREFIX = 'test.selenium.webdriver.common.'

  def __init__(self):
    self._tests_path = os.path.join(os.path.dirname(__file__),
                                    self.TESTS_FILENAME)
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
        '', '--driver-exe', type='string', default=None,
        help='Path to the ChromeDriver executable.')

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
  def _GetTestClassFromName(test_name):
    """Return the class for this test or None."""
    (obj, parts) = Main._GetModuleFromName(test_name)
    for comp in parts:
      if not Main._IsTestClass(getattr(obj, comp)):
        break
      obj = getattr(obj, comp)
    if Main._IsTestClass(obj):
      return obj
    return None

  @staticmethod
  def _SetTestClassAttributes(test_name, attribute_name, value):
    """Sets attributes for all the test classes found from |test_name|.

    Args:
      test_name:      name of the test
      attribute_name: name of the attribute to set
      value:          value to set the attribute to
    """
    class_obj = Main._GetTestClassFromName(test_name)
    if class_obj is not None:
      class_objs = [class_obj]
    else:
      class_objs = []
      module, = Main._GetModuleFromName(class_obj)
      for name in dir(module):
        item = getattr(module, name)
        if type(item) is type.TypeType:
          class_objs += [item]
    for c in class_objs:
      setattr(c, attribute_name, value)

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

  def _HasTestCases(self, module_string):
    """Determines if we have any test case classes in the module
       identified by |module_string|."""
    module = __import__(module_string)
    for name in dir(module):
      obj = getattr(module, name)
      if Main._IsTestClass(obj):
        return True
    return False

  def _GetTestNames(self, args):
    """Returns a suite of tests loaded from the given args.

    The given args can be either a module (ex: module1) or a testcase
    (ex: module2.MyTestCase) or a test (ex: module1.MyTestCase.testX)
    If empty, the tests in the already imported modules are loaded.

    Args:
      args: [module1, module2, module3.testcase, module4.testcase.testX]
            These modules or test cases or tests should be importable
    """
    if not args:  # Load tests ourselves
      logging.debug("Reading %s", self._tests_path)
      if not os.path.exists(self._tests_path):
        logging.warn("%s missing. Cannot load tests." % self._tests_path)
      else:
        args = self._GetTestNamesFrom(self._tests_path)
    return args

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

  def _Run(self):
    """Run the tests."""
    test_names = self._GetTestNames(self._args)

    # The tests expect to run with preset 'driver' and 'webserver' class
    # properties.
    launcher = ChromeDriverLauncher(self._options.driver_exe,
                                    chromedriver_paths.WEBDRIVER_TEST_DATA)
    driver = WebDriver(launcher.GetURL(), {})
    # The tests expect a webserver. Since ChromeDriver also operates as one,
    # just pass this dummy class with the right info.
    class DummyWebserver:
      pass
    webserver = DummyWebserver()
    webserver.port = launcher.GetPort()
    for test in test_names:
      Main._SetTestClassAttributes(test, 'driver', driver)
      Main._SetTestClassAttributes(test, 'webserver', webserver)

    # Load and run the tests.
    logging.debug('Loading tests from %s', test_names)
    loaded_tests = unittest.defaultTestLoader.loadTestsFromNames(test_names)
    test_suite = unittest.TestSuite()
    test_suite.addTests(loaded_tests)
    verbosity = 1
    if self._options.verbose:
      verbosity = 2
    result = GTestTextTestRunner(verbosity=verbosity).run(test_suite)
    launcher.Kill()
    sys.exit(not result.wasSuccessful())


if __name__ == '__main__':
  Main()
