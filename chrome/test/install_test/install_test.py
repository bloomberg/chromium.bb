# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test fixture for tests involving installing/updating Chrome.

Provides an interface to install or update chrome from within a testcase, and
allows users to run tests using installed version of Chrome. User and system
level installations are supported, and either one can be used for running the
tests. Currently the only platform it supports is Windows.
"""

import atexit
import logging
import optparse
import os
import platform
import re
import shutil
import stat
import sys
import tempfile
import unittest
import urllib

import chrome_installer_win

_DIRECTORY = os.path.dirname(os.path.abspath(__file__))
sys.path.append(os.path.join(os.path.dirname(_DIRECTORY), 'pyautolib'))
sys.path.append(os.path.join(_DIRECTORY, os.path.pardir, os.path.pardir,
                             os.path.pardir, 'third_party', 'webdriver',
                             'pylib'))

# This import should go after sys.path is set appropriately.
import pyauto_utils
import selenium.webdriver.chrome.service as service
from selenium import webdriver


def MakeTempDir(parent_dir=None):
  """Creates a temporary directory and returns an absolute path to it.

  The temporary directory is automatically deleted when the python interpreter
  exits normally.

  Args:
    parent_dir: the directory to create the temp dir in. If None, the system
                temp dir is used.

  Returns:
    The absolute path to the temporary directory.
  """
  path = tempfile.mkdtemp(dir=parent_dir)
  def DeleteDir():
    # Don't use shutil.rmtree because it can't delete read-only files on Win.
    for root, dirs, files in os.walk(path, topdown=False):
      for name in files:
        filename = os.path.join(root, name)
        os.chmod(filename, stat.S_IWRITE)
        os.remove(filename)
      for name in dirs:
        os.rmdir(os.path.join(root, name))
  atexit.register(DeleteDir)
  return path


class InstallTest(unittest.TestCase):
  """Base updater test class.

  All dependencies, like Chrome installers and ChromeDriver, are downloaded at
  the beginning of the test. Dependencies are downloaded in the temp directory.
  This download occurs only once, before the first test is executed. Each test
  case starts an instance of ChromeDriver and terminates it upon completion.
  All updater tests should derive from this class.

  Example:

  class SampleUpdater(InstallTest):

    def testCanOpenGoogle(self):
      self._driver.get('http://www.google.com/')
      self.UpdateBuild()
      self._driver.get('http://www.msn.com/')

  Include the following in your updater test script to make it run standalone.

  from install_test import Main

  if __name__ == '__main__':
    Main()

  To fire off an updater test, use the command below.
    python test_script.py --url=<URL> --builds=22.0.1230.0,22.0.1231.0
  """

  _DIR_PREFIX = '__CHRBLD__'
  _INSTALLER_NAME = 'mini_installer.exe'
  _installer_paths = []
  _chrome_driver = ''
  _installer_options = []

  def __init__(self, methodName='runTest'):
    unittest.TestCase.__init__(self, methodName)
    self._counter = 0
    current_version = chrome_installer_win.ChromeInstallation.GetCurrent()
    if current_version:
      current_version.Uninstall()
    self._install_type = ('system-level' in self._installer_options and
                          chrome_installer_win.InstallationType.SYSTEM or
                          chrome_installer_win.InstallationType.USER)

  def setUp(self):
    """Called before each unittest to prepare the test fixture."""
    self._InstallNext()
    self._StartChromeDriver()

  def tearDown(self):
    """Called at the end of each unittest to do any test related cleanup."""
    self._driver.quit()
    self._service.stop()
    self._installation.Uninstall()

  def _StartChromeDriver(self):
    """Starts ChromeDriver."""
    self._service = service.Service(InstallTest._chrome_driver)
    self._service.start()
    self._driver = webdriver.Remote(
        self._service.service_url,
        {'chrome.binary' : self._installation.GetExePath()}

  def _InstallNext(self):
    """Helper method that installs Chrome."""
    self._installation = chrome_installer_win.Install(
        self._installer_paths[self._counter],
        self._install_type,
        self._builds[self._counter],
        self._installer_options)
    self._counter += 1

  def UpdateBuild(self):
    """Updates Chrome by installing a newer version."""
    self._driver.quit()
    self._InstallNext()
    self._driver = webdriver.Remote(self._service.service_url,
                                    self._capabilities)

  @staticmethod
  def _Download(url, path):
    """Downloads a file from the specified URL.

    Args:
      url: URL where the file is located.
      path: Location where file will be downloaded.
    """
    if not pyauto_utils.DoesUrlExist(url):
      raise RuntimeError('Either the URL or the file name is invalid.')
    urllib.urlretrieve(url, path)

  @staticmethod
  def InitTestFixture(builds, base_url, options):
    """Static method for passing command options to InstallTest.

    We do not instantiate InstallTest. Therefore, command arguments cannot
    be passed to its constructor. Since InstallTest needs to use these options
    and using globals is not an option, this method can be used by the Main
    class to pass the arguments it parses onto InstallTest.
    """
    builds = builds.split(',') if builds else []
    system = ({'Windows': 'win',
               'Darwin': 'mac',
               'Linux': 'linux'}).get(platform.system())
    InstallTest._installer_options = options.split(',') if options else []
    InstallTest._builds = builds
    tempdir = MakeTempDir()
    for build in builds:
      url = '%s%s/%s/mini_installer.exe' % (base_url, build, system)
      installer_path = os.path.join(tempdir, 'mini_installer_%s.exe' % build)
      InstallTest._installer_paths.append(installer_path)
      InstallTest._Download(url, installer_path)
    InstallTest._chrome_driver = os.path.join(tempdir, 'chromedriver.exe')
    url = '%s%s/%s/%s/chromedriver.exe' % (base_url, build, system,
                                           'chrome-win32.test')
    InstallTest._Download(url, InstallTest._chrome_driver)


class Main(object):
  """Main program for running Updater tests."""

  def __init__(self):
    self._SetLoggingConfiguration()
    self._ParseArgs()
    self._Run()

  def _ParseArgs(self):
    """Parses command line arguments."""
    parser = optparse.OptionParser()
    parser.add_option(
        '-b', '--builds', type='string', default='', dest='builds',
        help='Specifies the two builds needed for testing.')
    parser.add_option(
        '-u', '--url', type='string', default='', dest='url',
        help='Specifies the build url, without the build number.')
    parser.add_option(
        '-o', '--options', type='string', default='',
        help='Specifies any additional Chrome options (i.e. --system-level).')
    opts, args = parser.parse_args()
    self._ValidateArgs(opts)
    InstallTest.InitTestFixture(opts.builds, opts.url, opts.options)

  def _ValidateArgs(self, opts):
    """Verifies the sanity of the command arguments.

    Confirms that all specified builds have a valid version number, and the
    build urls are valid.

    Args:
      opts: An object containing values for all command args.
    """
    builds = opts.builds.split(',')
    for build in builds:
      if not re.match('\d+\.\d+\.\d+\.\d+', build):
        raise RuntimeError('Invalid build number: %s' % build)
      if not pyauto_utils.DoesUrlExist('%s/%s/' % (opts.url, build)):
        raise RuntimeError('Could not locate build no. %s' % build)

  def _SetLoggingConfiguration(self):
    """Sets the basic logging configuration."""
    log_format = '%(asctime)s %(levelname)-8s %(message)s'
    logging.basicConfig(level=logging.INFO, format=log_format)

  def _GetTests(self):
    """Returns a list of unittests from the calling script."""
    mod_name = [os.path.splitext(os.path.basename(sys.argv[0]))[0]]
    if os.path.dirname(sys.argv[0]) not in sys.path:
      sys.path.append(os.path.dirname(sys.argv[0]))
    return unittest.defaultTestLoader.loadTestsFromNames(mod_name)

  def _Run(self):
    """Runs the unit tests."""
    tests = self._GetTests()
    result = pyauto_utils.GTestTextTestRunner(verbosity=1).run(tests)
    del(tests)
    if not result.wasSuccessful():
      print >>sys.stderr, ('Not all tests were successful.')
      sys.exit(1)
    sys.exit(0)
