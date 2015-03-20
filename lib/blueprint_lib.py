# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utilities to work with blueprints."""

from __future__ import print_function

import json

from chromite.lib import osutils
from chromite.lib import workspace_lib


def _GetBlueprintContent(blueprint_loc):
  """Returns a dictionary representation of a blueprint.

  Args:
    blueprint_loc: locator or path to a blueprint.

  Returns:
    A python dictionary.
  """
  path = (workspace_lib.LocatorToPath(blueprint_loc)
          if workspace_lib.IsLocator(blueprint_loc) else blueprint_loc)
  return json.loads(osutils.ReadFile(path))


def GetBricks(blueprint_loc):
  """Returns the bricks field of a blueprint."""
  return _GetBlueprintContent(blueprint_loc).get('bricks', [])


def GetBSP(blueprint_loc):
  """Returns the BSP field of a blueprint."""
  return _GetBlueprintContent(blueprint_loc).get('bsp')
