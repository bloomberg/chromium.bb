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
import logging
import optparse
import os.path
import subprocess
import sys
import time
import verifier
import vs_solution


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
    self._solution = solution

  def __del__(self):
    self._RestoreVisibility()

  def _RestoreVisibility(self):
    if self._saved_autohides != None:
      self._output.AutoHides = self._saved_autohides

  def BuildConfig(self, config):
    '''Builds all projects for a given config.

    Args:
      config: the name of the build configuration to build projecs for.

    Returns:
      The number of failures.
    '''
    solution = self._solution.GetSolution()
    builder = solution.SolutionBuild
    for project in self._projects:
      logging.info('Building project "%s" in "%s" configuration',
                   project,
                   config)
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


def GetOptionParser():
  '''Creates and returns an option parser for this script.'''
  parser = optparse.OptionParser(usage='%prog [options]')
  parser.add_option('-s', '--solution',
                    dest='solution',
                    default=_CHROME_SOLUTION,
                    help='''\
Use a specific solution file. The solution file can be provided either as
a path to an existing solution file, or as a basename, in which case the
solution with that name is assumed to exist in the chrome directory.\
''')

  return parser


def Main():
  if not ctypes.windll.shell32.IsUserAnAdmin():
    print ("Please run the smoke tests in an admin prompt "
           "(or AppVerifier won't work).")
    return 1

  parser = GetOptionParser()
  (options, args) = parser.parse_args()
  if args:
    parser.error('This script takes no arguments')

  solution_path = options.solution
  # If the solution is a path to an existing file, we use it as-is.
  if not os.path.exists(solution_path):
    # Otherwise we assume it's the base name for a file in the
    # in the same directory as our default solution file.
    solution_path = os.path.join(os.path.dirname(_CHROME_SOLUTION),
                                 solution_path)
    solution_path = solution_path + '.sln'

  # Make the path absolute
  solution_path = os.path.abspath(solution_path)

  solution = vs_solution.VsSolution(solution_path)
  solution.EnsureVisible()

  runner = TestRunner(solution,
                      _PROJECTS_TO_BUILD,
                      _TESTS_TO_RUN)

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
