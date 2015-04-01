# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utilities to work with blueprints."""

from __future__ import print_function

import json
import os

from chromite.lib import osutils
from chromite.lib import workspace_lib


class BlueprintNotFound(Exception):
  """The blueprint does not exist."""


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
    """
    path = (workspace_lib.LocatorToPath(blueprint_loc)
            if workspace_lib.IsLocator(blueprint_loc) else blueprint_loc)
    if not os.path.exists(path):
      if initial_config:
        osutils.WriteFile(path, json.dumps(initial_config, sort_keys=True,
                                           indent=4, separators=(',', ': ')),
                          makedirs=True)
      else:
        raise BlueprintNotFound('blueprint %s not found.' % path)

    self.config = json.loads(osutils.ReadFile(path))

  def GetBricks(self):
    """Returns the bricks field of a blueprint."""
    return self.config.get('bricks', [])

  def GetBSP(self):
    """Returns the BSP field of a blueprint."""
    return self.config.get('bsp')

  def GetMainPackage(self):
    """Returns the main_package field of a blueprint."""
    return self.config.get('main_package')
