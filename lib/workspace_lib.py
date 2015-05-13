# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utilities for discovering the directories associated with workspaces.

Workspaces have a variety of important concepts:

* The bootstrap repository. BOOTSTRAP/chromite/bootstrap is expected to be in
the user's path. Most commands run from here redirect to the active SDK.

* The workspace directory. This directory (identified by presence of
WORKSPACE_CONFIG), contains code, and is associated with exactly one SDK
instance. It is normally discovered based on CWD.

* The SDK root. This directory contains a specific SDK version, and is stored in
BOOTSTRAP/sdk_checkouts/<version>.

This library contains helper methods for finding all of the relevant directories
here.
"""

from __future__ import print_function

import json
import os

from chromite.cbuildbot import constants
from chromite.lib import cros_build_lib
from chromite.lib import osutils

MAIN_CHROOT_DIR_IN_VM = '/chroots'

# The presence of this file signifies the root of a workspace.
WORKSPACE_CONFIG = 'workspace-config.json'
WORKSPACE_LOCAL_CONFIG = '.local.json'
WORKSPACE_CHROOT_DIR = '.chroot'
WORKSPACE_IMAGES_DIR = 'build/images'
WORKSPACE_LOGS_DIR = 'build/logs'

# Prefixes used by locators.
_BOARD_LOCATOR_PREFIX = 'board:'
_WORKSPACE_LOCATOR_PREFIX = '//'


class LocatorNotResolved(Exception):
  """Given locator could not be resolved."""


class ConfigFileError(Exception):
  """Configuration file writing or reading failed."""


def WorkspacePath(workspace_reference_dir=None):
  """Returns the path to the current workspace.

  This method works both inside and outside the chroot, though results will
  be different.

  Args:
    workspace_reference_dir: Any directory inside the workspace. If None,
      will use CWD (outside chroot), or bind mount location (inside chroot).
      You should normally use the default.

  Returns:
    Path to root directory of the workspace (if valid), or None.
  """
  if workspace_reference_dir is None:
    if cros_build_lib.IsInsideChroot():
      workspace_reference_dir = constants.CHROOT_WORKSPACE_ROOT
    else:
      workspace_reference_dir = os.getcwd()

  workspace_config = osutils.FindInPathParents(
      WORKSPACE_CONFIG,
      os.path.abspath(workspace_reference_dir))

  return os.path.dirname(workspace_config) if workspace_config else None


def ChrootPath(workspace_path):
  """Returns the path to the chroot associated with the given workspace.

  Each workspace has its own associated chroot. This method returns the chroot
  path set in the workspace config if present, or else the default location,
  which varies depending on whether or not we run in a VM.

  Args:
    workspace_path: Root directory of the workspace (WorkspacePath()).

  Returns:
    Path to where the chroot is, or where it should be created.
  """
  config_value = GetChrootDir(workspace_path)
  if config_value:
    # If the config value is a relative path, we base it in the workspace path.
    # Otherwise, it is an absolute path and will be returned as is.
    return os.path.join(workspace_path, config_value)

  # The default for a VM.
  if osutils.IsInsideVm():
    return os.path.join(MAIN_CHROOT_DIR_IN_VM, os.path.basename(workspace_path))

  # The default for all other cases.
  return os.path.join(workspace_path, WORKSPACE_CHROOT_DIR)


def SetChrootDir(workspace_path, chroot_dir):
  """Set which chroot directory a workspace uses.

  This value will overwrite the default value, if set. This is normally only
  used if the user overwrites the default value. This method is NOT atomic.

  Args:
    workspace_path: Root directory of the workspace (WorkspacePath()).
    chroot_dir: Directory in which this workspaces chroot should be created.
  """
  # Read the config, update its chroot_dir, and write it.
  config = _ReadLocalConfig(workspace_path)
  config['chroot_dir'] = chroot_dir
  _WriteLocalConfig(workspace_path, config)


def GetChrootDir(workspace_path):
  """Get override of chroot directory for a workspace.

  You should normally call ChrootPath so that the default value will be
  found if no explicit value has been set.

  Args:
    workspace_path: Root directory of the workspace (WorkspacePath()).

  Returns:
    version string or None.
  """
  # Config should always return a dictionary.
  config = _ReadLocalConfig(workspace_path)

  # If version is present, use it, else return None.
  return config.get('chroot_dir')


def GetActiveSdkVersion(workspace_path):
  """Find which SDK version a workspace is associated with.

  This SDK may or may not exist in the bootstrap cache. There may be no
  SDK version associated with a workspace.

  Args:
    workspace_path: Root directory of the workspace (WorkspacePath()).

  Returns:
    version string or None.
  """
  # Config should always return a dictionary.
  config = _ReadLocalConfig(workspace_path)

  # If version is present, use it, else return None.
  return config.get('version')


def SetActiveSdkVersion(workspace_path, version):
  """Set which SDK version a workspace is associated with.

  This method is NOT atomic.

  Args:
    workspace_path: Root directory of the workspace (WorkspacePath()).
    version: Version string of the SDK. (Eg. 1.2.3)
  """
  # Read the config, update its version, and write it.
  config = _ReadLocalConfig(workspace_path)
  config['version'] = version
  _WriteLocalConfig(workspace_path, config)


def _ReadLocalConfig(workspace_path):
  """Read a local config for a workspace.

  Args:
    workspace_path: Root directory of the workspace (WorkspacePath()).

  Returns:
    Local workspace config as a Python dictionary.
  """
  try:
    return ReadConfigFile(os.path.join(workspace_path, WORKSPACE_LOCAL_CONFIG))
  except IOError:
    # If the file doesn't exist, it's an empty dictionary.
    return {}


def _WriteLocalConfig(workspace_path, config):
  """Save out a new local config for a workspace.

  Args:
    workspace_path: Root directory of the workspace (WorkspacePath()).
    config: New local workspace config contents as a Python dictionary.
  """
  WriteConfigFile(os.path.join(workspace_path, WORKSPACE_LOCAL_CONFIG), config)


def IsLocator(name):
  """Returns True if name is a specific locator."""
  if not name:
    raise ValueError('Locator is empty')
  return (name.startswith(_WORKSPACE_LOCATOR_PREFIX)
          or name.startswith(_BOARD_LOCATOR_PREFIX))


def LocatorToPath(locator):
  """Returns the absolute path for this locator.

  Args:
    locator: a locator.

  Returns:
    The absolute path defined by this locator.

  Raises:
    ValueError: If |locator| is invalid.
    LocatorNotResolved: If |locator| is valid but could not be resolved.
  """
  if locator.startswith(_WORKSPACE_LOCATOR_PREFIX):
    workspace_path = WorkspacePath()
    if workspace_path is None:
      raise LocatorNotResolved(
          'Workspace not found while trying to resolve %s' % locator)
    return os.path.join(workspace_path,
                        locator[len(_WORKSPACE_LOCATOR_PREFIX):])

  if locator.startswith(_BOARD_LOCATOR_PREFIX):
    return os.path.join(constants.SOURCE_ROOT, 'src', 'overlays',
                        'overlay-%s' % locator[len(_BOARD_LOCATOR_PREFIX):])

  raise ValueError('Invalid locator %s' % locator)


def PathToLocator(path):
  """Converts a path to a locator.

  This does not raise error if the path does not map to a locator. Some valid
  (legacy) brick path do not map to any locator: chromiumos-overlay,
  private board overlays, etc...

  Args:
    path: absolute or relative to CWD path to a workspace object or board
      overlay.

  Returns:
    The locator for this path if it exists, None otherwise.
  """
  workspace_path = WorkspacePath()
  path = os.path.abspath(path)

  if workspace_path is None:
    return None

  # If path is in the current workspace, return the relative path prefixed with
  # the workspace prefix.
  if os.path.commonprefix([path, workspace_path]) == workspace_path:
    return _WORKSPACE_LOCATOR_PREFIX + os.path.relpath(path, workspace_path)

  # If path is in the src directory of the checkout, this is a board overlay.
  # Encode it as board locator.
  src_path = os.path.join(constants.SOURCE_ROOT, 'src')
  if os.path.commonprefix([path, src_path]) == src_path:
    parts = os.path.split(os.path.relpath(path, src_path))
    if parts[0] == 'overlays':
      board_name = '-'.join(parts[1].split('-')[1:])
      return _BOARD_LOCATOR_PREFIX + board_name

  return None


def LocatorToFriendlyName(locator):
  """Returns a friendly name for a given locator.

  Args:
    locator: a locator.
  """
  if IsLocator(locator) and locator.startswith(_WORKSPACE_LOCATOR_PREFIX):
    return locator[len(_WORKSPACE_LOCATOR_PREFIX):].replace('/', '.')

  raise ValueError('Not a valid workspace locator: %s' % locator)


def WriteConfigFile(path, config):
  """Writes |config| to a file at |path|.

  Configuration files in a workspace should all use the same format
  whenever possible. Currently it's JSON, but centralizing config
  read/write makes it easier to change when needed.

  Args:
    path: path to write.
    config: configuration dictionary to write.

  Raises:
    ConfigFileError: |config| cannot be written as JSON.
  """
  # TODO(dpursell): Add support for comments in config files.
  try:
    osutils.WriteFile(
        path,
        json.dumps(config, sort_keys=True, indent=4, separators=(',', ': ')),
        makedirs=True)
  except TypeError as e:
    raise ConfigFileError('Writing config file %s failed: %s', path, e)


def ReadConfigFile(path):
  """Reads a configuration file at |path|.

  For use with WriteConfigFile().

  Args:
    path: file path.

  Returns:
    Result of parsing the JSON file.

  Raises:
    ConfigFileError: JSON parsing failed.
  """
  try:
    return json.loads(osutils.ReadFile(path))
  except ValueError as e:
    raise ConfigFileError('%s is not in valid JSON format: %s' % (path, e))
