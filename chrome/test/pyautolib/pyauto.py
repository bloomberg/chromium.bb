#!/usr/bin/python

# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""PyAuto: Python Interface to Chromium's Automation Proxy.

PyAuto uses swig to expose Automation Proxy interfaces to Python.
For complete documentation on the functionality available,
run pydoc on this file.

Ref: http://dev.chromium.org/developers/pyauto


Include the following in your PyAuto test script to make it run standalone.

from pyauto import Main

if __name__ == '__main__':
  Main()

This script can be used as an executable to fire off other scripts, similar
to unittest.py
  python pyauto.py test_script
"""

import logging
import optparse
import os
import re
import sys
import types
import unittest


def _LocateBinDirs():
  """Setup a few dirs where we expect to find dependency libraries."""
  script_dir = os.path.dirname(__file__)
  chrome_src = os.path.join(script_dir, os.pardir, os.pardir, os.pardir)

  bin_dirs = {
      'linux2': [ os.path.join(chrome_src, 'out', 'Debug'),
                  os.path.join(chrome_src, 'sconsbuild', 'Debug'),
                  os.path.join(chrome_src, 'out', 'Release'),
                  os.path.join(chrome_src, 'sconsbuild', 'Release')],
      'darwin': [ os.path.join(chrome_src, 'xcodebuild', 'Debug'),
                  os.path.join(chrome_src, 'xcodebuild', 'Release')],
      'win32':  [ os.path.join(chrome_src, 'chrome', 'Debug'),
                  os.path.join(chrome_src, 'chrome', 'Release')],
      'cygwin': [ os.path.join(chrome_src, 'chrome', 'Debug'),
                  os.path.join(chrome_src, 'chrome', 'Release')],
  }
  deps_dirs = [ os.path.join(script_dir, os.pardir,
                             os.pardir, os.pardir, 'third_party'),
  ]
  sys.path += bin_dirs.get(sys.platform, []) + deps_dirs

_LocateBinDirs()

try:
  import pyautolib
  # Needed so that all additional classes (like: FilePath, GURL) exposed by
  # swig interface get available in this module.
  from pyautolib import *
except ImportError:
  print >>sys.stderr, "Could not locate built libraries. Did you build?"
  raise

# Should go after sys.path is set appropriately
import bookmark_model


class PyUITest(pyautolib.PyUITestBase, unittest.TestCase):
  """Base class for UI Test Cases in Python.

  A browser is created before executing each test, and is destroyed after
  each test irrespective of whether the test passed or failed.

  You should derive from this class and create methods with 'test' prefix,
  and use methods inherited from PyUITestBase (the C++ side).

  Example:

    class MyTest(PyUITest):

      def testNavigation(self):
        self.NavigateToURL("http://www.google.com")
        self.assertEqual("Google", self.GetActiveTabTitle())
  """

  def __init__(self, methodName='runTest', **kwargs):
    """Initialize PyUITest.

    When redefining __init__ in a derived class, make sure that:
      o you make a call this __init__
      o __init__ takes methodName as an arg. this is mandated by unittest module

    Args:
      methodName: the default method name. Internal use by unittest module

      (The rest of the args can be in any order. They can even be skipped in
       which case the defaults will be used.)

      clear_profile: If True, clean the profile dir before use. Defaults to True
      homepage: the home page. Defaults to "about:blank"
    """
    # Fetch provided keyword args, or fill in defaults.
    clear_profile = kwargs.get('clear_profile', True)
    homepage = kwargs.get('homepage', 'about:blank')

    pyautolib.PyUITestBase.__init__(self, clear_profile, homepage)
    # Figure out path to chromium binaries
    browser_dir = os.path.normpath(os.path.dirname(pyautolib.__file__))
    self.Initialize(pyautolib.FilePath(browser_dir))
    unittest.TestCase.__init__(self, methodName)

  def __del__(self):
    pyautolib.PyUITestBase.__del__(self)

  def setUp(self):
    """Override this method to launch browser differently.

    Can be used to prevent launching the browser window by default in case a
    test wants to do some additional setup before firing browser.
    """
    self.SetUp()     # Fire browser

  def tearDown(self):
    self.TearDown()  # Destroy browser

  @staticmethod
  def DataDir():
    """Returns the path to the data dir chrome/test/data."""
    return os.path.join(os.path.dirname(__file__), os.pardir, "data")

  def GetBookmarkModel(self):
    """Return the bookmark model as a BookmarkModel object.

    This is a snapshot of the bookmark model; it is not a proxy and
    does not get updated as the bookmark model changes.
    """
    return bookmark_model.BookmarkModel(self._GetBookmarksAsJSON())


class PyUITestSuite(pyautolib.PyUITestSuiteBase, unittest.TestSuite):
  """Base TestSuite for PyAuto UI tests."""

  def __init__(self, args):
    pyautolib.PyUITestSuiteBase.__init__(self, args)

    # Figure out path to chromium binaries
    browser_dir = os.path.normpath(os.path.dirname(pyautolib.__file__))
    logging.debug('Loading pyauto libs from %s', browser_dir)
    self.Initialize(pyautolib.FilePath(browser_dir))
    os.environ['PATH'] = browser_dir + os.pathsep + os.environ['PATH']

    unittest.TestSuite.__init__(self)

  def __del__(self):
    # python unittest module is setup such that the suite gets deleted before
    # the test cases, which is odd because our test cases depend on
    # initializtions like exitmanager, autorelease pool provided by the
    # suite. Forcibly delete the test cases before the suite.
    del self._tests
    pyautolib.PyUITestSuiteBase.__del__(self)


# Implementation inspired from unittest.main()
class Main(object):
  """Main program for running PyAuto tests."""

  _options, _args = None, None
  _tests_filename = 'PYAUTO_TESTS'
  _platform_map = {
    'win32':  'win',
    'darwin': 'mac',
    'linux2': 'linux',
  }

  def __init__(self):
    self._ParseArgs()
    self._Run()

  def _ParseArgs(self):
    """Parse command line args."""
    parser = optparse.OptionParser()
    parser.add_option(
        '-v', '--verbose', action='store_true', default=False,
        help='Make PyAuto verbose.')
    parser.add_option(
        '-D', '--wait-for-debugger', action='store_true', default=False,
        help='Block PyAuto on startup for attaching debugger.')
    parser.add_option(
        '', '--ui-test-flags', type='string', default='',
        help='Flags passed to the UI test suite. Refer ui_test.h for options')
    parser.add_option(
        '', '--list-missing-tests', action='store_true', default=False,
        help='Print a list of tests not included in PYAUTO_TESTS, and exit')

    self._options, self._args = parser.parse_args()

    # Setup logging
    if self._options.verbose:
      logging.basicConfig(level=logging.DEBUG,
                          format='%(asctime)s %(levelname)-8s %(message)s')
    if self._options.list_missing_tests:
      self._ListMissingTests()
      sys.exit(0)

  def TestsDir(self):
    """Returns the path to dir containing tests.

    This is typically the dir containing the tests description file.
    This method should be overridden by derived class to point to other dirs
    if needed.
    """
    return os.path.dirname(__file__)

  def _ListMissingTests(self):
    """Print tests missing from PYAUTO_TESTS."""
    def _GetTestsFrom(module_string):
      try:
        module = __import__(module_string)
      except ImportError:  # Probably just a test script
        return [module_string]
      tests = []
      for name in dir(module):
        obj = getattr(module, name)
        if (isinstance(obj, (type, types.ClassType)) and
            issubclass(obj, PyUITest) and obj != PyUITest):
          tests += [module_string + "." + obj.__name__ + "." + x for x in \
                    filter(lambda x: x.startswith('test'), dir(obj))]
      return tests
    # Fetch tests from all test scripts
    all_test_files = filter(lambda x: x.endswith('.py'),
                            os.listdir(self.TestsDir()))
    all_tests_modules = [os.path.splitext(x)[0] for x in all_test_files]
    all_tests = reduce(lambda x, y: x + y,
                       map(_GetTestsFrom, all_tests_modules))
    # Fetch tests included by PYAUTO_TESTS
    pyauto_tests_file = os.path.join(self.TestsDir(), self._tests_filename)
    pyauto_tests = reduce(lambda x, y: x + y,
                          map(_GetTestsFrom,
                              self._GetTestNamesFrom(pyauto_tests_file)))
    for a_test in all_tests:
      if a_test not in pyauto_tests:
        print a_test

  def _HasTestCases(self, module_string):
    """Determines if we have any PyUITest test case classes in the module
       identified by |module_string|."""
    module = __import__(module_string)
    for name in dir(module):
      obj = getattr(module, name)
      if (isinstance(obj, (type, types.ClassType)) and
          issubclass(obj, PyUITest)):
        return True
    return False

  def _LoadTests(self, args):
    """Returns a suite of tests loaded from the given args.

    The given args can be either a module (ex: module1) or a testcase
    (ex: module2.MyTestCase) or a test (ex: module1.MyTestCase.testX)
    If empty, the tests in the already imported modules are loaded.

    Args:
      args: [module1, module2, module3.testcase, module4.testcase.testX]
            These modules or test cases or tests should be importable
    """
    if not args:  # Load tests ourselves
      if self._HasTestCases('__main__'):    # we are running a test script
        args.append('__main__')   # run the test cases found in it
      else:  # run tests from the test description file
        pyauto_tests_file = os.path.join(self.TestsDir(), self._tests_filename)
        logging.debug("Reading %s", pyauto_tests_file)
        if not os.path.exists(pyauto_tests_file):
          logging.warn("%s missing. Cannot load tests." % pyauto_tests_file)
        else:
          args = self._GetTestNamesFrom(pyauto_tests_file)
    logging.debug("Loading tests from %s", args)
    loaded_tests = unittest.defaultTestLoader.loadTestsFromNames(args)
    return loaded_tests

  def _GetTestNamesFrom(self, filename):
    contents = open(filename).read()
    modules = eval(contents, {'__builtins__': None}, None)
    args = modules.get('all', []) + \
           modules.get(self._platform_map[sys.platform], [])
    return args

  def _Run(self):
    """Run the tests."""
    if self._options.wait_for_debugger:
      raw_input('Attach debugger to process %s and hit <enter> ' % os.getpid())

    pyauto_suite = PyUITestSuite(re.split('\s+', self._options.ui_test_flags))
    loaded_tests = self._LoadTests(self._args)
    pyauto_suite.addTests(loaded_tests)
    verbosity = 1
    if self._options.verbose:
      verbosity = 2
    result = unittest.TextTestRunner(verbosity=verbosity).run(pyauto_suite)
    del loaded_tests  # Need to destroy test cases before the suite
    del pyauto_suite
    sys.exit(not result.wasSuccessful())


if __name__ == '__main__':
  Main()

