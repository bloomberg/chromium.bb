# Copyright (c) 2011-2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module that contains trybot patch pool code."""

import constants
import functools


def ChromiteFilter(patch):
  """Used with FilterFn to isolate patches to chromite."""
  return patch.project == constants.CHROMITE_PROJECT


def ExtManifestFilter(patch):
  """Used with FilterFn to isolate patches to the external manifest."""
  return patch.project == constants.MANIFEST_PROJECT


def IntManifestFilter(patch):
  """Used with FilterFn to isolate patches to the internal manifest."""
  return patch.project == constants.MANIFEST_INT_PROJECT


def ManifestFilter(patch):
  """Used with FilterFn to isolate patches to the manifest."""
  return ExtManifestFilter(patch) or IntManifestFilter(patch)


def BranchFilter(branch, patch):
  """Used with FilterFn to isolate patches based on a specific upstream."""
  return patch.tracking_branch == branch


class TrybotPatchPool(object):
  """Represents patches specified by the user to test."""
  def __init__(self, gerrit_patches=(), local_patches=(), remote_patches=()):
    self.gerrit_patches = tuple(gerrit_patches)
    self.local_patches = tuple(local_patches)
    self.remote_patches = tuple(remote_patches)

  def __nonzero__(self):
    """Returns True if the pool has no patches."""
    return any([self.gerrit_patches, self.local_patches, self.remote_patches])

  def Filter(self, **kwargs):
    """Returns a new pool with only patches that match constraints.

    Args:
      **kwargs: constraints in the form of attr=value.  I.e.,
                project='chromiumos/chromite', tracking_branch='master'.
    """
    def AttributeFilter(patch):
      for key in kwargs:
        if getattr(patch, key, object()) != kwargs[key]:
          return False
      else:
        return True

    return self.FilterFn(AttributeFilter)

  def FilterFn(self, filter_fn, negate=False):
    """Returns a new pool with only patches that match constraints.

    Args:
      filter_fn: Functor that accepts a 'patch' argument, and returns whether to
                 include the patch in the results.
      negate: Return patches that don't pass the filter_fn.
    """
    f = filter_fn
    if negate:
      f = lambda p : not filter_fn(p)

    return self.__class__(
        gerrit_patches=filter(f, self.gerrit_patches),
        local_patches=filter(f, self.local_patches),
        remote_patches=filter(f, self.remote_patches))

  def FilterManifest(self, negate=False):
    """Return a patch pool with only patches to the manifest."""
    return self.FilterFn(ManifestFilter, negate=negate)

  def FilterIntManifest(self, negate=False):
    """Return a patch pool with only patches to the internal manifest."""
    return self.FilterFn(IntManifestFilter, negate=negate)

  def FilterExtManifest(self, negate=False):
    """Return a patch pool with only patches to the external manifest."""
    return self.FilterFn(ExtManifestFilter, negate=negate)

  def FilterBranch(self, branch, negate=False):
    """Return a patch pool with only patches based on a particular branch."""
    return self.FilterFn(functools.partial(BranchFilter, branch), negate=negate)

  def __iter__(self):
    for source in [self.local_patches, self.remote_patches,
                   self.gerrit_patches]:
      for patch in source:
        yield patch
