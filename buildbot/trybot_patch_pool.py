# Copyright (c) 2011-2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module that contains trybot patch pool code."""


class TrybotPatchPool(object):
  """Represents patches specified by the user to test."""
  def __init__(self, gerrit_patches, local_patches, remote_patches):
    self.gerrit_patches = gerrit_patches
    self.local_patches = local_patches
    self.remote_patches = remote_patches

  def __nonzero__(self):
    """Returns True if the pool has no patches."""
    return any([self.gerrit_patches, self.local_patches, self.remote_patches])

  def Filter(self, **kwargs):
    """Returns a new pool with only patches that match constraints.

    **kwargs: constraints in the form of attr=value.  I.e.,
              project='chromiumos/chromite', tracking_branch='master'.
    """
    filtered = []
    for patch_list in [self.gerrit_patches, self.local_patches,
                       self.remote_patches]:
      sub_filtered = []
      for patch in patch_list:
        for key in kwargs:
          if getattr(patch, key, object()) != kwargs[key]:
            break
        else:
          sub_filtered.append(patch)

      filtered.append(sub_filtered)

    return TrybotPatchPool(*filtered)


def GetEmptyPool():
  """Return a pool with no patches."""
  return TrybotPatchPool([], [], [])
