#!python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''This scripts builds and runs the CEEE unittests and medium tests that
make our smoke tests.

It exits with a non-zero exit status on error.
'''
try:
  import ctypes
  import pythoncom
  import win32com.client
except ImportError:
  print "This script requires Python 2.6 with PyWin32 installed."
  raise

import datetime
import os.path
import subprocess
import sys
import time
import verifier

# The top-level source directory.
# All other paths in this file are relative to this directory.
_SRC_DIR = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '../..'))


def _AbsolutePath(path):
  '''Convert path to an absolute path.

  Args:
    path: a path relative to _SRC_DIR.

  Returns: Path as an absolute, normalized path.
  '''
  return os.path.abspath(os.path.join(_SRC_DIR, path))


_CHROME_SOLUTION = _AbsolutePath('chrome/chrome.sln')


# List of project files to build.
_PROJECTS_TO_BUILD = [
    # Common unittest
    _AbsolutePath('ceee/common/ceee_common_unittests.vcproj'),

    # IE unittests and medium tests.
    _AbsolutePath('ceee/ie/ie_unittests.vcproj'),
    _AbsolutePath('ceee/ie/mediumtest_ie.vcproj'),
]

# List of test executables to run.
_TESTS_TO_RUN = [
  'ceee_common_unittests',
  'ie_unittests',
  'mediumtest_ie',
]

# A list of configurations we build and run.
_CONFIGURATIONS = [
  'Debug',
  'Release',
]

class TestRunner(object):
  def __init__(self, solution, projects, tests):
    self._projects = projects
    self._tests = tests
    self._saved_autohides = None
    self._solution = win32com.client.GetObject(solution)
    self._dte = self._GetDte(self._solution)

    self._EnsureVisible()

  def __del__(self):
    self._RestoreVisibility()

  def _EnsureVisible(self):
    dte = self._dte
    # Make sure the UI is visible.
    dte.MainWindow.Visible = True

    # Get the output window, disable its autohide and give it focus.
    self._output = dte.Windows['Output']
    self._saved_autohides = self._output.AutoHides
    self._output.AutoHides = False
    self._output.SetFocus()

  def _RestoreVisibility(self):
    if self._saved_autohides != None:
      self._output.AutoHides = self._saved_autohides

  def _GetDte(self, solution):
    '''Sometimes invoking solution.DTE will fail with an exception during
    Visual Studio initialization. To work around this, we try a couple of
    times with an intervening sleep to give VS time to settle.'''
    # Attempt ten times under a try block.
    for i in range(0, 10):
      try:
        return solution.DTE
      except pythoncom.com_error, e:
        # Sleep for 2 seconds
        print 'Exception from solution.DTE "%s", retrying: ' % str(e)
        time.sleep(2.0)

    # Final try, pass the exception to the caller on failure here.
    return solution.DTE

  def BuildConfig(self, config):
    '''Builds all projects for a given config.

    Args:
      config: the name of the build configuration to build projecs for.

    Returns:
      The number of failures.
    '''
    builder = self._solution.SolutionBuild
    for project in self._projects:
      print 'Building project "%s" in "%s" configuration' % (project, config)
      wait_for_build = True
      # See http://goo.gl/Dy8x for documentation on this method.
      builder.BuildProject(config, project, wait_for_build)
      errors = builder.LastBuildInfo
      if errors != 0:
        return errors

    return 0

  def RunConfigTests(self, config):
    '''Runs all tests for a given config and writes success files.

    For each succeeding test '<path>/foo.exe', this function writes a
    corresponding success file '<path>/foo.success'. If the corresponding
    success file is newer than the test executable, the test executable will
    not run again.
    If all tests succeed, this function writes an overall success file
    named 'ceee.success'.

    Args:
      config: the name of the build configuration to run tests for.

    Returns:
      The number of failures.
    '''
    failures = 0
    for test in self._tests:
      test_exe = os.path.join(_SRC_DIR,
                              'chrome',
                              config,
                              test) + '.exe'
      success_file = os.path.join(_SRC_DIR,
                                  'chrome',
                                  config,
                                  test) + '.success'
      if (os.path.exists(success_file) and
          os.stat(test_exe).st_mtime <= os.stat(success_file).st_mtime):
        print '"%s" is unchanged since last successful run' % test_exe
      else:
        if verifier.HasAppVerifier():
          (exit, errors) = verifier.RunTestWithVerifier(test_exe)
        else:
          exit = subprocess.call(test_exe)

        # Create a success file if the test succeded.
        if exit == 0:
          open(success_file, 'w').write(str(datetime.datetime.now()))
        else:
          failures = failures + 1
          print '"%s" failed with exit status %d' % (test_exe, exit)

    # Create the overall success file if everything succeded.
    if failures == 0:
      success_file = os.path.join(_SRC_DIR,
                                  'chrome',
                                  config,
                                  'ceee') + '.success'
      open(success_file, 'w').write(str(datetime.datetime.now()))

    return failures


def Main():
  if not ctypes.windll.shell32.IsUserAnAdmin():
    print ("Please run the smoke tests in an admin prompt "
           "(or AppVerifier won't work).")
    return 1

  runner = TestRunner(_CHROME_SOLUTION, _PROJECTS_TO_BUILD, _TESTS_TO_RUN)

  failures = 0
  for config in _CONFIGURATIONS:
    failures += runner.BuildConfig(config)

  if failures != 0:
    print '%d build errors' % failures
    return failures

  for config in _CONFIGURATIONS:
    failures += runner.RunConfigTests(config)

  if failures != 0:
    print '%d unittests failed' % failures

  return failures


if __name__ == '__main__':
  sys.exit(Main())
