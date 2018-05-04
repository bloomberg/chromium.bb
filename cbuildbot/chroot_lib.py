# -*- coding: utf-8 -*-
# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Functions for managing chroots.

Currently this just contains functions for reusing chroots for incremental
building.
"""

from __future__ import print_function
import os

from chromite.lib import constants
from chromite.lib import cros_logging as logging
from chromite.lib import cros_sdk_lib
from chromite.lib import osutils
from chromite.lib import sudo


CHROOT_VERSION_FILE = 'etc/cros_manifest_version'


class ChrootManager(object):
  """Class for managing chroots and chroot versions."""

  def __init__(self, build_root):
    """Constructor.

    Args:
      build_root: The root of the checkout.
    """
    self._build_root = build_root

  def _ChrootVersionPath(self, chroot=None):
    """Get the path to the chroot version file for |chroot|.

    Args:
      chroot: Path to chroot. Defaults to 'chroot' under build root.

    Returns:
      The path to the chroot version file.
    """
    if chroot is None:
      chroot = os.path.join(self._build_root, constants.DEFAULT_CHROOT_DIR)
    return os.path.join(chroot, CHROOT_VERSION_FILE)

  def GetChrootVersion(self, chroot=None):
    """Get the version of the checkout used to create |chroot|.

    Args:
      chroot: Path to chroot. Defaults to 'chroot' under build root.

    Returns:
      The version of Chrome OS used to build |chroot|. E.g. 6394.0.0-rc3.
      If the chroot does not exist, or there is no version file, returns None.
    """
    chroot_version_file = self._ChrootVersionPath(chroot)
    if not os.path.exists(chroot_version_file):
      return None
    return osutils.ReadFile(chroot_version_file).strip()

  def EnsureChrootAtVersion(self, version):
    """Ensure the current chroot is at version |version|.

    If our chroot has version, use it. Otherwise, blow away the chroot.

    Args:
      version: Version of the chroot to look for. E.g. 6394.0.0-rc3

    Returns:
      Whether or not we wiped the existing chroot. True if yes (fresh chroot),
      False otherwise (using existing chroot).
    """
    chroot = os.path.join(self._build_root, constants.DEFAULT_CHROOT_DIR)
    if version and self.GetChrootVersion(chroot) == version:
      logging.PrintBuildbotStepText('(Using existing chroot)')
      return False
    else:
      logging.PrintBuildbotStepText('(Using fresh chroot)')
      cros_sdk_lib.CleanupChrootMount(chroot, delete_image=True)
      osutils.RmDir(chroot, ignore_missing=True, sudo=True)
      return True

  def ClearChrootVersion(self, chroot=None):
    """Clear the version in the specified |chroot|.

    Args:
      chroot: Path to chroot. Defaults to 'chroot' under build root.
    """
    chroot_version_file = self._ChrootVersionPath(chroot)
    osutils.RmDir(chroot_version_file, ignore_missing=True, sudo=True)

  def SetChrootVersion(self, version, chroot=None):
    """Update the version file in the chroot to |version|.

    Args:
      version: Version to use. E.g. 6394.0.0-rc3
      chroot: Path to chroot. Defaults to 'chroot' under build root.
    """
    chroot_version_file = self._ChrootVersionPath(chroot)
    if os.path.exists(os.path.dirname(chroot_version_file)):
      sudo.SetFileContents(chroot_version_file, version)
