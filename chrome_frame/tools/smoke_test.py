#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Builds and runs the Chrome Frame unit and integration tests,
the exit status of the scrip is the number of failed tests.
"""

import os.path
import sys
import win32com.client


# The top-level source directory.
# All other paths in this file are relative to this directory.
_SRC_DIR = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '../..'))


def AbsolutePath(path):
  '''Convert path to absolute.

  Args:
    path: a path relative to _SRC_DIR.

  Returns: Path as an absolute, normalized path.
  '''
  return os.path.abspath(os.path.join(_SRC_DIR, path))


# Solution path.
_CHROME_SOLUTION = AbsolutePath('chrome/chrome.sln')

# List of project files to build.
_PROJECTS_TO_BUILD = [
    # Chrome.exe, which in turn causes chrome.dll etc to get built.
    AbsolutePath('chrome/chrome.vcproj'),

    # Chrome Frame.
    AbsolutePath('chrome_frame/npchrome_frame.vcproj'),

    # Chrome Frame unittests.
    AbsolutePath('chrome_frame/chrome_frame_unittests.vcproj'),
    AbsolutePath('chrome_frame/crash_reporting/vectored_handler_tests.vcproj'),

    # Chrome Frame integration tests.
    AbsolutePath('chrome_frame/chrome_frame_tests.vcproj'),
    AbsolutePath('chrome_frame/chrome_frame_net_tests.vcproj'),
    AbsolutePath('chrome_frame/chrome_frame_perftests.vcproj'),
]

# List of test executables to run.
_TESTS_TO_RUN = [
    'chrome_frame_unittests.exe',
    'vectored_handler_tests.exe',

    'chrome_frame_tests.exe',
    'chrome_frame_net_tests.exe',
    # 'chrome_frame_perftests.exe',
]

def BuildProjectConfig(builder, config, project):
  '''Builds a given project in a given configuration, exits on error.

  Args:
    builder: a Visual Studio SolutionBuild object.
    config: the name of the configuration to build, f.ex. "Release".
    project: the path of a project to build, either absolute or else relative
        to the builder's solution directory.
  '''
  print 'Building project "%s" in configuration "%s" ' % (project, config)
  project = os.path.normpath(project)
  builder.BuildProject(config, project, True)
  errors = builder.LastBuildInfo

  if errors != 0:
    print '%d errors while building config %s.' % (errors, config)
    sys.exit(errors)


def Main():
  '''Builds Chrome Frame Tests and all their dependencies,
  then runs the tests.'''
  v = {}
  solution = win32com.client.GetObject(_CHROME_SOLUTION)
  builder = solution.SolutionBuild

  for project in _PROJECTS_TO_BUILD:
    BuildProjectConfig(builder, 'Debug', project)

  # Ok, everything's built, run the tests.
  failed_tests = 0
  for test in _TESTS_TO_RUN:
    test_path = os.path.abspath(os.path.join(_SRC_DIR, 'chrome/Debug', test))
    exit_status = os.system(test_path)
    if exit_status != 0:
      failed_tests += 1
      print "Test \"%s\" failed with status %d" % (test, exit_status)

  return failed_tests


if __name__ == "__main__":
  sys.exit(Main())
