# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module that handles interactions with a Validation Pool.

The validation pool is the set of commits that are ready to be validated i.e.
ready for the commit queue to try.
"""

import logging

from chromite.buildbot import gerrit_helper
from chromite.buildbot import patch as cros_patch


class ValidationPool(object):
  """Class that handles interactions with a validation pool.

  This class can be used to acquire a set of commits that form a pool of
  commits ready to be validated and committed.

  Usage:  Use ValidationPoo.AcquirePool -- a static
  method that grabs the commits that are ready for validation.
  """

  def __init__(self):
    """Initializes an instance by setting default valuables to instance vars.

    Generally use AcquirePool as an entry pool to a pool rather than this
    method.
    """
    self._changes = None
    self._gerrit_helper = None

  @classmethod
  def _IsTreeOpen(cls):
    """Returns True if the tree is open for submitting changes."""
    # TODO(sosa): crosbug.com/17709.
    return True

  @classmethod
  def AcquirePool(cls, branch, internal):
    """Acquires the current pool from Gerrit.

    Polls Gerrit and checks for which change's are ready to be committed.

    Returns:  ValidationPool object.
    """
    if cls._IsTreeOpen():
      pool = ValidationPool()
      pool._gerrit_helper = gerrit_helper.GerritHelper(internal)
      raw_changes = pool._gerrit_helper.GrabChangesReadyForCommit(branch)
      pool._changes = pool._gerrit_helper.FilterProjectsNotInSourceTree(
          raw_changes)
      return pool
    else:
      return None

  def ApplyPoolIntoRepo(self, repo):
    """Cherry picks changes from pool into repository."""
    non_applied_changes = []
    for change in self._changes:
      try:
        change.Apply(repo.directory)
      except cros_patch.ApplyPatchException:
        non_applied_changes.add(change)

    if non_applied_changes:
      logging.debug('Some changes could not be applied cleanly.')
      self.HandleApplicationFailure(non_applied_changes)
      self._changes = list(set(self._changes) - set(non_applied_changes))

  def SubmitPool(self):
    """Commits changes to Gerrit from Pool."""
    for change in self._changes:
      logging.info('Change %s will be submitted' % change)
      change.Submit(gerrit_helper)

  def HandleApplicationFailure(self, changes):
    """Handles changes that were not able to be applied cleanly."""
    # TODO(sosa): crosbug.com/17708.
    for change in changes:
      logging.info('Change %s did not apply cleanly.' % change)

  def HandleValidationFailure(self):
    """Handles failed changes by removing them from next Validation Pools."""
    # TODO(sosa): crosbug.com/17708.
    for change in self._changes:
      logging.info('Validation failed for change %s.' % change)

