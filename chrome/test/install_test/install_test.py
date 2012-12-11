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
import os
import platform
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
from chrome import Chrome
from selenium import webdriver
import selenium.webdriver.chrome.service as service
from selenium.webdriver.chrome.service import WebDriverException

import pyauto_utils


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
    # Delete parent directory after its contents have been removed.
    os.rmdir(path)
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
      self.Install(self.GetUpdateBuilds()[0])
      self.StartChrome()
      self._driver.get('http://www.google.com/')
      self.Install(self.GetUpdateBuilds()[1])
      self.StartChrome()
      self._driver.get('http://www.google.org/')

  Include the following in your updater test script to make it run standalone.

  from install_test import Main

  if __name__ == '__main__':
    Main()

  To fire off an updater test, use the command below.
    python test_script.py --url=<URL> --update-builds=24.0.1299.0,24.0.1300.0
  """

  _installer_paths = {}
  _chrome_driver = ''
  _installer_options = []
  _install_type = chrome_installer_win.InstallationType.USER

  def __init__(self, methodName='runTest'):
    unittest.TestCase.__init__(self, methodName)
    self._driver = None
    current_version = chrome_installer_win.ChromeInstallation.GetCurrent()
    if current_version:
      current_version.Uninstall()

  def setUp(self):
    """Called before each unittest to prepare the test fixture."""
    self._StartService()

  def tearDown(self):
    """Called at the end of each unittest to do any test related cleanup."""
    # Confirm ChromeDriver was instantiated, before attempting to quit.
    if self._driver != None:
      try:
        self._driver.quit()
      except WebDriverException:
        pass
    self._service.stop()
    self._installation.Uninstall()

  def _StartService(self):
    """Starts ChromeDriver service."""
    self._service = service.Service(InstallTest._chrome_driver)
    self._service.start()

  def StartChrome(self, caps={}):
    """Creates a ChromeDriver instance.

    Args:
      caps: Capabilities that will be passed to ChromeDriver.
    """
    self._driver = Chrome(self._service.service_url, caps)

  def Install(self, build, master_pref=None):
    """Helper method that installs the specified Chrome build.

    Args:
      build: Chrome version number that will be used for installation.
      master_pref: Location of the master preferences file.
    """
    if self._driver:
      try:
        self._driver.quit()
      except WebDriverException:
        pass
    options = []
    options.extend(self._installer_options)
    if self._install_type == chrome_installer_win.InstallationType.SYSTEM:
      options.append('--system-level')
    if master_pref:
      options.append('--installerdata="%s"' % master_pref)
    self._installation = chrome_installer_win.Install(
        self._installer_paths[build],
        self._install_type,
        build,
        options)

  def GetInstallBuild(self):
    """Returns Chorme build to be used for install test scenarios."""
    return self._install_build

  def GetUpdateBuilds(self):
    """Returns Chrome builds to be used for update test scenarios."""
    return self._update_builds

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
  def SetInstallType(install_type):
    """Sets Chrome installation type.

    Args:
      install_type: Type of installation(i.e., user or system).
    """
    InstallTest._install_type = install_type

  @staticmethod
  def InitTestFixture(install_build, update_builds, base_url, options):
    """Static method for passing command options to InstallTest.

    We do not instantiate InstallTest. Therefore, command arguments cannot be
    passed to its constructor. Since InstallTest needs to use these options,
    and using globals is not an option, this method can be used by the Main
    class to pass the arguments it parses onto InstallTest.

    Args:
      install_build: A string representing the Chrome build to be used for
                     install testing. Pass this argument only if testing
                     fresh install scenarios.
      update_builds: A list that contains the Chrome builds to be used for
                     testing update scenarios. Pass this argument only if
                     testing upgrade scenarios.
      base_url: Base url of the 'official chrome builds' page.
      options: A list that contains options to be passed to Chrome installer.
    """
    system = ({'Windows': 'win',
               'Darwin': 'mac',
               'Linux': 'linux'}).get(platform.system())
    InstallTest._install_build = install_build
    InstallTest._update_builds = update_builds
    InstallTest._installer_options = options
    tempdir = MakeTempDir()
    builds = []
    if InstallTest._install_build:
      builds.append(InstallTest._install_build)
    if InstallTest._update_builds:
      builds.extend(InstallTest._update_builds)
    # Remove any duplicate build numbers.
    builds = list(frozenset(builds))
    for build in builds:
      url = '%s%s/%s/mini_installer.exe' % (base_url, build, system)
      installer_path = os.path.join(tempdir, 'mini_installer_%s.exe' % build)
      InstallTest._installer_paths[build] = installer_path
      InstallTest._Download(url, installer_path)
    InstallTest._chrome_driver = os.path.join(tempdir, 'chromedriver.exe')
    url = '%s%s/%s/%s/chromedriver.exe' % (base_url, build, system,
                                           'chrome-win32.test')
    InstallTest._Download(url, InstallTest._chrome_driver)
