# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common utilities for working with Project SDK."""

from __future__ import print_function

import os
import re

from chromite.cbuildbot import constants
from chromite.lib import osutils


def FindSourceRoot(sdk_dir=None):
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


def FindVersion(sdk_dir=None):
  """Find the version of a given SDK.

  This really uses the name of the manifest this SDK was based on, that means
  there are a number of special cases.

  Official pinned SDKs should always use a ChromeOS release number like
  '6500.0.0'. However, SDKs checked out from other manifests can return
  all sorts of values like 'official', 'full', or '6500.0.0-r12'.

  This command does not yes distinguish between official and non-official
  SDKs (brbug.com/262).

  Args:
    sdk_dir: path to the SDK, or any of its sub directories.

  Returns:
    The version of your SDK as a string. '6500.0.0'
    None if the directory doesn't appear to be an SDK.
  """
  sdk_root = FindSourceRoot(sdk_dir)
  if sdk_root is None:
    return None

  # Find the path to the manifest symlink 'repo' uses.
  manifest_path = os.path.join(sdk_root, '.repo', 'manifest.xml')

  # e.g. SDK/.repo/manifest/project-sdk/6759.0.0.xml
  expanded_manifest = os.path.realpath(manifest_path)

  # e.g. 6759.0.0.xml
  manifest_file = os.path.basename(expanded_manifest)

  # e.g. 6759.0.0
  version = os.path.splitext(manifest_file)[0]

  # Make sure this is an official SDK version.
  # TODO(garnold) Remove once we stabilize version number extraction
  # (brillo:262).
  if not re.match(r'^\d+\.\d+\.\d+$', version):
    return None

  return version
