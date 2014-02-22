# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utilities for managing the toolchains in the chroot."""

import copy
import json
import os

from chromite.buildbot import constants
from chromite.buildbot import portage_utilities
from chromite.lib import cros_build_lib
from chromite.lib import gs
from chromite.lib import osutils

if cros_build_lib.IsInsideChroot():
  # Only import portage after we've checked that we're inside the chroot.
  # Outside may not have portage, in which case the above may not happen.
  # We'll check in main() if the operation needs portage.

  # pylint: disable=F0401
  import portage


def GetHostTuple():
  """Returns compiler tuple for the host system."""
  # pylint: disable=E1101
  return portage.settings['CHOST']


def GetTuplesForOverlays(overlays):
  """Returns a set of tuples for a given set of overlays."""
  targets = {}
  default_settings = {
      'sdk'      : True,
      'crossdev' : '',
      'default'  : False,
  }

  for overlay in overlays:
    config = os.path.join(overlay, 'toolchain.conf')
    if os.path.exists(config):
      first_target = None
      seen_default = False

      for line in osutils.ReadFile(config).splitlines():
        # Split by hash sign so that comments are ignored.
        # Then split the line to get the tuple and its options.
        line = line.split('#', 1)[0].split()

        if len(line) > 0:
          target = line[0]
          if not first_target:
            first_target = target
          if target not in targets:
            targets[target] = copy.copy(default_settings)
          if len(line) > 1:
            targets[target].update(json.loads(' '.join(line[1:])))
            if targets[target]['default']:
              seen_default = True

      # If the user has not explicitly marked a toolchain as default,
      # automatically select the first tuple that we saw in the conf.
      if not seen_default and first_target:
        targets[first_target]['default'] = True

  return targets


# Tree interface functions. They help with retrieving data about the current
# state of the tree:
def GetAllTargets():
  """Get the complete list of targets.

  Returns:
    The list of cross targets for the current tree
  """
  targets = GetToolchainsForBoard('all')

  # Remove the host target as that is not a cross-target. Replace with 'host'.
  del targets[GetHostTuple()]
  return targets


def GetToolchainsForBoard(board, buildroot=constants.SOURCE_ROOT):
  """Get a list of toolchain tuples for a given board name

  Returns:
    The list of toolchain tuples for the given board
  """
  overlays = portage_utilities.FindOverlays(
      constants.BOTH_OVERLAYS, None if board in ('all', 'sdk') else board,
      buildroot=buildroot)
  toolchains = GetTuplesForOverlays(overlays)
  if board == 'sdk':
    toolchains = FilterToolchains(toolchains, 'sdk', True)
  return toolchains


def FilterToolchains(targets, key, value):
  """Filter out targets based on their attributes.

  Args:
    targets: dict of toolchains
    key: metadata to examine
    value: expected value for metadata

  Returns:
    dict where all targets whose metadata |key| does not match |value|
    have been deleted
  """
  return dict((k, v) for k, v in targets.iteritems() if v[key] == value)


def GetSdkURL(for_gsutil=False, suburl=''):
  """Construct a Google Storage URL for accessing SDK related archives

  Args:
    for_gsutil: Do you want a URL for passing to `gsutil`?
    suburl: A url fragment to tack onto the end

  Returns:
    The fully constructed URL
  """
  return gs.GetGsURL(constants.SDK_GS_BUCKET, for_gsutil=for_gsutil,
                     suburl=suburl)
