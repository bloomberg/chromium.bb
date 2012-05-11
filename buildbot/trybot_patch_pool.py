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


def GetEmptyPool():
  """Return a pool with no patches."""
  return TrybotPatchPool([], [], [])
