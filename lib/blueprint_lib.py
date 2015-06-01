# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utilities to work with blueprints."""

from __future__ import print_function

import os

from chromite.lib import brick_lib
from chromite.lib import workspace_lib


# Field names for specifying initial configuration.
APP_ID_FIELD = 'buildTargetId'
BRICKS_FIELD = 'bricks'
BSP_FIELD = 'bsp'

# Those packages are implicitly built for all blueprints.
# - target-os is needed to build any image.
# - target-os-dev and target-os-test are needed to build a developer friendly
#   image. They should not be included in any production images.
_IMPLICIT_PACKAGES = (
    'virtual/target-os',
    'virtual/target-os-dev',
    'virtual/target-os-test',
)


class BlueprintNotFoundError(Exception):
  """The blueprint does not exist."""


class BlueprintCreationError(Exception):
  """Blueprint creation failed."""


class Blueprint(object):
  """Encapsulates the interaction with a blueprint."""

  def __init__(self, blueprint_loc, initial_config=None):
    """Instantiates a blueprint object.

    Args:
      blueprint_loc: blueprint locator.  This can be a relative path to CWD, an
        absolute path, or a relative path to the root of the workspace prefixed
        with '//'.
      initial_config: A dictionary of key-value pairs to seed a new blueprint
        with if the specified blueprint doesn't already exist.

    Raises:
      BlueprintNotFoundError: No blueprint exists at |blueprint_loc| and no
        |initial_config| was given to create a new one.
      BlueprintCreationError: |initial_config| was specified but a file
        already exists at |blueprint_loc|.
    """
    self._path = (workspace_lib.LocatorToPath(blueprint_loc)
                  if workspace_lib.IsLocator(blueprint_loc) else blueprint_loc)
    self._locator = workspace_lib.PathToLocator(self._path)

    if initial_config is not None:
      self._CreateBlueprintConfig(initial_config)

    try:
      self.config = workspace_lib.ReadConfigFile(self._path)
    except IOError:
      raise BlueprintNotFoundError('Blueprint %s not found.' % self._path)

  @property
  def path(self):
    return self._path

  @property
  def locator(self):
    return self._locator

  def _CreateBlueprintConfig(self, config):
    """Create an initial blueprint config file.

    Converts all brick paths in |config| into locators then saves the
    configuration file to |self._path|.

    Currently fails if |self._path| already exists, but could be
    generalized to allow re-writing config files if needed.

    Args:
      config: configuration dictionary.

    Raises:
      BlueprintCreationError: A brick in |config| doesn't exist or an
        error occurred while saving the config file.
    """
    if os.path.exists(self._path):
      raise BlueprintCreationError('File already exists at %s.' % self._path)

    try:
      # Turn brick specifications into locators. If bricks or BSPs are
      # unspecified, assign default values so the config file has the proper
      # structure for easy manual editing.
      if config.get(BRICKS_FIELD):
        config[BRICKS_FIELD] = [brick_lib.Brick(b).brick_locator
                                for b in config[BRICKS_FIELD]]
      else:
        config[BRICKS_FIELD] = []
      if config.get(BSP_FIELD):
        config[BSP_FIELD] = brick_lib.Brick(config[BSP_FIELD]).brick_locator
      else:
        config[BSP_FIELD] = None

      # Create the config file.
      workspace_lib.WriteConfigFile(self._path, config)
    except (brick_lib.BrickNotFound, workspace_lib.ConfigFileError) as e:
      raise BlueprintCreationError('Blueprint creation failed. %s' % e)

  def GetBricks(self):
    """Returns the bricks field of a blueprint."""
    return self.config.get(BRICKS_FIELD, [])

  def GetBSP(self):
    """Returns the BSP field of a blueprint."""
    return self.config.get(BSP_FIELD)

  def GetAppId(self):
    """Returns the APP_ID from a blueprint."""
    app_id = self.config.get(APP_ID_FIELD)
    return app_id

  def FriendlyName(self):
    """Returns the friendly name for this blueprint."""
    return workspace_lib.LocatorToFriendlyName(self._locator)

  def GetUsedBricks(self):
    """Returns the set of bricks used by this blueprint."""
    brick_map = {}
    for top_brick in self.GetBricks() + [self.GetBSP()]:
      for b in brick_lib.Brick(top_brick).BrickStack():
        brick_map[b.brick_locator] = b

    return brick_map.values()

  def GetPackages(self, with_implicit=True):
    """Returns the list of packages needed by this blueprint.

    This includes the main packages for the bricks and the bsp of this
    blueprint. We don't add the main packages of the bricks dependencies to
    allow inheriting a brick without inheriting its required packages.

    Args:
      with_implicit: If True, include packages that are implicitly required by
        the core system.
    """
    packages = []
    for locator in self.GetBricks() + [self.GetBSP()]:
      packages.extend(brick_lib.Brick(locator).MainPackages())

    if with_implicit:
      packages.extend(_IMPLICIT_PACKAGES)
    return packages
