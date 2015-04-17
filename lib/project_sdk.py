# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common utilities for working with Project SDK."""

from __future__ import print_function

import os
import re

from chromite.cbuildbot import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import osutils


def FindRepoRoot(sdk_dir=None):
  """Locate the SDK root directly by looking for .repo dir.

  This is very similar to constants.SOURCE_ROOT, except that it can operate
  against repo checkouts outside our current code base.

  CAUTION! Using SDKs from directories other than the default is likely to
  break assumptions that our tools are built upon.  As a rule of thumb, do not
  expose this argument externally unless you know what you're doing.

  Args:
    sdk_dir: Path of the SDK, or any dir inside it. None defaults to
      constants.SOURCE_ROOT.

  Returns:
    Root dir of SDK, or None.
  """
  if sdk_dir is None:
    return constants.SOURCE_ROOT

  # Make sure we're looking at an actual directory.
  if not os.path.isdir(sdk_dir):
    return None

  # Find the .repo directory and return the path leading up to it, if found.
  repo_dir = osutils.FindInPathParents('.repo', os.path.abspath(sdk_dir),
                                       test_func=os.path.isdir)
  return os.path.dirname(repo_dir) if repo_dir else None


def VersionFile(sdk_dir):
  return os.path.join(sdk_dir, 'SDK_VERSION')


def FindVersion(sdk_dir=None):
  """Find the version of a given SDK.

  If the SDK was fetched by any means other than "brillo sdk" then it will
  always appear to be 'non-official', even if an official manifest was used.

  Args:
    sdk_dir: path to the SDK, or any of its sub directories.

  Returns:
    The version of your SDK as a string. '6500.0.0'
    None if the directory doesn't appear to be an SDK.
  """
  sdk_root = FindRepoRoot(sdk_dir)
  if sdk_root is None:
    return None

  v_file = VersionFile(sdk_root)
  return osutils.ReadFile(v_file) if os.path.exists(v_file) else None


def _GetExecutableVersion(cmd, version_arg='--version'):
  """Gets an executable version string using |version_flag|.

  Args:
    cmd: Executable to check (for example, '/bin/bash').
    version_arg: Argument to get |cmd| to print its version.

  Returns:
    Output string or None if the program doesn't exist or gave a
    non-zero exit code.
  """
  try:
    return cros_build_lib.RunCommand(
        [cmd, version_arg], print_cmd=False, capture_output=True).output
  except cros_build_lib.RunCommandError:
    return None


def VerifyEnvironment():
  """Verify the environment we are installed to.

  Returns:
    boolean: True if the environment looks friendly.
  """
  result = True

  # Verify Python:
  #   We assume the python environment is acceptable, because we got here.
  #   However, we can add imports here to check for any required external
  #   packages.

  # Verify executables that just need to exist.
  for cmd in ('/bin/bash', 'curl'):
    if _GetExecutableVersion(cmd) is None:
      logging.warn('%s is required to use the SDK.', cmd)
      result = False

  # Verify Git version.
  git_requirement_message = 'git 1.8 or greater is required to use the SDK.'
  git_version = _GetExecutableVersion('git')
  if git_version is None:
    logging.warn(git_requirement_message)
    result = False

  # Example version string: 'git version 2.2.0.rc0.207.ga3a616c'.
  m = re.match(r'git version (\d+)\.(\d+)', git_version)
  if not m:
    logging.warn(git_requirement_message)
    logging.warn("git version not recognized from: '%s'.", git_version)
    result = False
  else:
    gv_int_list = [int(d) for d in m.groups()] # Something like [2, 3]
    if gv_int_list < [1, 8]:
      logging.warn(git_requirement_message)
      logging.warn("Current version: '%s'.", git_version)
      result = False

  return result
