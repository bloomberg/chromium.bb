#!python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''This script builds CEEE and dependencies

It exits with a non-zero exit status on error.
'''
try:
  import pythoncom
  import win32com.client
except ImportError:
  print "This script requires Python 2.6 with PyWin32 installed."
  raise
import datetime
import getopt
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
    # Chrome
    _AbsolutePath('chrome/chrome.vcproj'),
    # Chrome DLL
    _AbsolutePath('chrome/chrome_dll.vcproj'),
    # CEEE
    _AbsolutePath('ceee/ceee_all.vcproj'),
    # The mini installer
    _AbsolutePath('chrome_frame/npchrome_frame.vcproj'),
]


# A list of configurations we build.
_CONFIGURATIONS = [
  'Debug',
]


class CeeeBuilder(object):
  def __init__(self, solution, projects):
    self._projects = projects
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
      print 'Building project "%s" in "%s" configuration' % (project, config)
      wait_for_build = True
      # See http://goo.gl/Dy8x for documentation on this method.
      builder.BuildProject(config, project, wait_for_build)
      errors = builder.LastBuildInfo
      if errors != 0:
        return errors

    return 0


def Main(argv):
  solution = vs_solution.VsSolution(_CHROME_SOLUTION)
  solution.EnsureVisible()
  runner = CeeeBuilder(solution, _PROJECTS_TO_BUILD)

  failures = 0
  for config in _CONFIGURATIONS:
    failures += runner.BuildConfig(config)

  if failures != 0:
    print '%d build errors' % failures
    return failures

  return failures


if __name__ == '__main__':
  sys.exit(Main(sys.argv[1:]))
