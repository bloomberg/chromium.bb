# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module that handles interactions with a Validation Pool.

The validation pool is the set of commits that are ready to be validated i.e.
ready for the commit queue to try.
"""

import constants
import json
import logging

from chromite.buildbot import patch as cros_patch
from chromite.lib import cros_build_lib


class _GerritServer():
  """Helper class to manage interaction with Gerrit server."""
  def __init__(self, branch, internal):
    """Initializes variables for interaction with a gerrit server."""
    self.internal = internal
    if self.internal:
      ssh_port = constants.GERRIT_INT_PORT
      ssh_host = constants.GERRIT_INT_HOST
    else:
      ssh_port = constants.GERRIT_PORT
      ssh_host = constants.GERRIT_HOST

    ssh_prefix = ['ssh', '-p', ssh_port, ssh_host]
    ready_for_commit_query = ('gerrit query --format=json status:open '
                              'AND CodeReview=+2 AND Verified=1 AND '
                              'branch:%s' % branch).split()
    self.ssh_query_cmd = ssh_prefix + ready_for_commit_query

  def GrabChangesReadyForCommit(self):
    """Returns the list of changes to try.

    This methods returns an array of GerritPatch's that are ready to be
    committed.
    """
    changes_to_commit = []
    raw_results = cros_build_lib.RunCommand(self.ssh_query_cmd,
                                            redirect_stdout=True)

    # Each line return is a json dict.
    for raw_result in raw_results.output.splitlines():
      result_dict = json.loads(raw_result)
      if not 'number' in result_dict:
        logging.debug('No change number found in %s' % result_dict)
        continue

      change_number = result_dict['number']
      logging.info('Found change %s ready and pending' % change_number)
      if self.internal:
        changes_to_commit.append('*' + change_number)
      else:
        changes_to_commit.append(change_number)

    return cros_patch.GetGerritPatchInfo(changes_to_commit)


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
      pool._changes = _GerritServer(
          branch, internal).GrabChangesReadyForCommit()
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
    # TODO(sosa): crosbug.com/17707.
    for change in self._changes:
      logging.info('Change %s will be submitted' % change)

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

