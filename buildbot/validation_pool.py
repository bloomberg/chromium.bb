# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module that handles interactions with a Validation Pool.

The validation pool is the set of commits that are ready to be validated i.e.
ready for the commit queue to try.
"""

import json
import logging
import time
import urllib
from xml.dom import minidom

from chromite.buildbot import gerrit_helper
from chromite.buildbot import lkgm_manager
from chromite.buildbot import patch as cros_patch
from chromite.lib import cros_build_lib

_BUILD_DASHBOARD = 'http://build.chromium.org/p/chromiumos'
_BUILD_INT_DASHBOARD = 'http://chromegw/i/chromeos'


class TreeIsClosedException(Exception):
  """Raised when the tree is closed and we wanted to submit changes."""
  def __init__(self):
    super(TreeIsClosedException, self).__init__(
        'TREE IS CLOSED.  PLEASE SET TO OPEN OR THROTTLED TO COMMIT')


class FailedToSubmitAllChangesException(Exception):
  """Raised if we fail to submit any changes."""
  def __init__(self, changes):
    super(FailedToSubmitAllChangesException, self).__init__(
        'FAILED TO SUBMIT ALL CHANGES:  Could not verify that changes %s were '
        'submitted' % ' '.join(str(c) for c in changes))


class ValidationPool(object):
  """Class that handles interactions with a validation pool.

  This class can be used to acquire a set of commits that form a pool of
  commits ready to be validated and committed.

  Usage:  Use ValidationPoo.AcquirePool -- a static
  method that grabs the commits that are ready for validation.
  """

  GLOBAL_DRYRUN = False

  def __init__(self, internal, build_number, builder_name, is_master, dryrun):
    """Initializes an instance by setting default valuables to instance vars.

    Generally use AcquirePool as an entry pool to a pool rather than this
    method.

    Args:
      internal:  Set to True if this is an internal validation pool.
      build_number:  Build number for this validation attempt.
      builder_name:  Builder name on buildbot dashboard.
      is_master: True if this is the master builder for the Commit Queue.
      dryrun: If set to True, do not submit anything to Gerrit.
    """
    build_dashboard = _BUILD_DASHBOARD if not internal else _BUILD_INT_DASHBOARD
    self.build_log = (build_dashboard + '/builders/%s/builds/' % builder_name +
                      str(build_number))
    self.changes = []
    self.changes_that_failed_to_apply_earlier = []
    self.gerrit_helper = None
    self.is_master = is_master
    self.dryrun = dryrun | self.GLOBAL_DRYRUN

  @classmethod
  def _IsTreeOpen(cls):
    """Returns True if the tree is open or throttled."""

    def _SleepWithExponentialBackOff(current_sleep):
      """Helper function to sleep with exponential backoff."""
      time.sleep(current_sleep)
      return current_sleep * 2

    max_attempts = 5
    response_dict = None
    status_url = 'https://chromiumos-status.appspot.com/current?format=json'
    current_sleep = 1

    for _ in range(max_attempts):
      try:
        response = urllib.urlopen(status_url)
        # Check for valid response code.
        if response.getcode() == 200:
          response_dict = json.load(response)
          break

        current_sleep = _SleepWithExponentialBackOff(current_sleep)
      except IOError:
        # We continue if we can't reach appspot.com
        current_sleep = _SleepWithExponentialBackOff(current_sleep)

    else:
      # We go ahead and say the tree is open if we can't tree the status page.
      logging.warn('Could not get a status from %s', status_url)
      return True

    tree_open = response_dict['general_state'] in ['open', 'throttled']
    return tree_open

  @classmethod
  def AcquirePool(cls, internal, buildroot, build_number, builder_name, dryrun):
    """Acquires the current pool from Gerrit.

    Polls Gerrit and checks for which change's are ready to be committed.

    Args:
      internal: If True, use gerrit-int.
      buildroot: The location of the buildroot used to filter projects.
      build_number: Corresponding build number for the build.
      builder_name:  Builder name on buildbot dashboard.
      dryrun: Don't submit anything to gerrit.
    Returns:
      ValidationPool object.
    Raises:
      TreeIsClosedException: if the tree is closed.
    """
    if cls._IsTreeOpen() or dryrun:
      # Only master configurations should call this method.
      pool = ValidationPool(internal, build_number, builder_name, True, dryrun)
      pool.gerrit_helper = gerrit_helper.GerritHelper(internal)
      raw_changes = pool.gerrit_helper.GrabChangesReadyForCommit()
      pool.changes = pool.gerrit_helper.FilterNonCrosProjects(raw_changes,
                                                              buildroot)
      return pool
    else:
      raise TreeIsClosedException()

  @classmethod
  def AcquirePoolFromManifest(cls, manifest, internal, build_number,
                              builder_name, is_master, dryrun):
    """Acquires the current pool from a given manifest.

    Args:
      manifest: path to the manifest where the pool resides.
      internal: if true, assume gerrit-int.
      build_number: Corresponding build number for the build.
      builder_name:  Builder name on buildbot dashboard.
      is_master: Boolean that indicates whether this is a pool for a master.
        config or not.
      dryrun: Don't submit anything to gerrit.
    Returns:
      ValidationPool object.
    """
    pool = ValidationPool(internal, build_number, builder_name, is_master,
                          dryrun)
    pool.gerrit_helper = gerrit_helper.GerritHelper(internal)
    manifest_dom = minidom.parse(manifest)
    pending_commits = manifest_dom.getElementsByTagName(
        lkgm_manager.PALADIN_COMMIT_ELEMENT)
    for pending_commit in pending_commits:
      project = pending_commit.getAttribute(lkgm_manager.PALADIN_PROJECT_ATTR)
      change = pending_commit.getAttribute(lkgm_manager.PALADIN_CHANGE_ID_ATTR)
      commit = pending_commit.getAttribute(lkgm_manager.PALADIN_COMMIT_ATTR)
      pool.changes.append(pool.gerrit_helper.GrabPatchFromGerrit(
          project, change, commit))

    return pool

  def ApplyPoolIntoRepo(self, buildroot):
    """Applies changes from pool into the directory specified by the buildroot.

    This method applies changes in the order specified.  It also respects
    dependency order.

    Returns:
      True if we managed to apply any changes.
    """
    # Sets are used for performance reasons where changes_list is used to
    # maintain ordering when applying changes.
    changes_that_failed_to_apply_against_other_changes = set()
    changes_that_failed_to_apply_to_tot = set()
    changes_applied = set()
    changes_list = []

    # Maps Change numbers to GerritPatch object for lookup of dependent
    # changes.
    change_map = dict((change.id, change) for change in self.changes)
    for change in self.changes:
      logging.debug('Trying change %s', change.id)
      # We've already attempted this change because it was a dependent change
      # of another change that was ready.
      if (change in changes_that_failed_to_apply_to_tot or
          change in changes_applied):
        continue

      # Change stacks consists of the change plus its dependencies in the order
      # that they should be applied.
      change_stack = [change]
      apply_chain = True
      deps = []
      try:
        deps.extend(change.GerritDependencies(buildroot))
        deps.extend(change.PaladinDependencies(buildroot))
      except cros_patch.MissingChangeIDException as me:
        change.apply_error_message = (
            'Could not apply change %s because change has a Gerrit Dependency '
            'that does not contain a ChangeId.  Please remove this dependency '
            'or update the dependency with a ChangeId.' % change.id)
        logging.error(change.apply_error_message)
        logging.error(str(me))
        changes_that_failed_to_apply_to_tot.add(change)
        apply_chain = False

      for dep in deps:
        dep_change = change_map.get(dep)
        if not dep_change:
          # The dep may have been committed already.
          if not self.gerrit_helper.IsChangeCommitted(dep):
            message = ('Could not apply change %s because dependent '
                       'change %s is not ready to be committed.' % (
                        change.id, dep))
            logging.info(message)
            change.apply_error_message = message
            apply_chain = False
            break
        else:
          change_stack.insert(0, dep_change)

      # Should we apply the chain -- i.e. all deps are ready.
      if not apply_chain:
        continue

      # Apply changes in change_stack.  For chains that were aborted early,
      # we still want to apply changes in change_stack because they were
      # ready to be committed (o/w wouldn't have been in the change_map).
      for change in change_stack:
        try:
          if change in changes_applied:
            continue
          elif change in changes_that_failed_to_apply_to_tot:
            break
          else:
            change.Apply(buildroot, trivial=not self.dryrun)

        except cros_patch.ApplyPatchException as e:
          if e.type == cros_patch.ApplyPatchException.TYPE_REBASE_TO_TOT:
            changes_that_failed_to_apply_to_tot.add(change)
          else:
            change.apply_error_message = (
                'Your change conflicted with another change being tested '
                'in the last validation pool.  Please re-sync, rebase and '
                're-upload.')
            changes_that_failed_to_apply_against_other_changes.add(change)

          break
        else:
          # We applied the change successfully.
          changes_applied.add(change)
          changes_list.append(change)
          lkgm_manager.PrintLink(str(change), change.url)
          if self.is_master:
            change.HandleApplied(self.gerrit_helper, self.build_log,
                                 dryrun=self.dryrun)

    if changes_applied:
      logging.debug('Done investigating changes.  Applied %s',
                    ' '.join([c.id for c in changes_applied]))

    if changes_that_failed_to_apply_to_tot:
      logging.info('Changes %s could not be applied cleanly.',
                  ' '.join([c.id for c in changes_that_failed_to_apply_to_tot]))
      self.HandleApplicationFailure(changes_that_failed_to_apply_to_tot)

    self.changes = changes_list
    self.changes_that_failed_to_apply_earlier = list(
        changes_that_failed_to_apply_against_other_changes)
    return len(self.changes) > 0

  def SubmitPool(self):
    """Commits changes to Gerrit from Pool.  This is only called by a master.

    Raises:
      TreeIsClosedException: if the tree is closed.
    """
    assert self.is_master, 'Non-master builder calling SubmitPool'
    changes_that_failed_to_submit = []
    if ValidationPool._IsTreeOpen() or self.dryrun:
      for change in self.changes:
        was_change_submitted = False
        logging.info('Change %s will be submitted', change)
        try:
          change.Submit(self.gerrit_helper, dryrun=self.dryrun)
          was_change_submitted = self.gerrit_helper.IsChangeCommitted(
              change.id)
        except cros_build_lib.RunCommandError:
          logging.error('gerrit review --submit failed for change.')
        finally:
          if not was_change_submitted:
            logging.error('Could not submit %s', str(change))
            change.HandleCouldNotSubmit(self.gerrit_helper, self.build_log,
                                        dryrun=self.dryrun)
            changes_that_failed_to_submit.append(change)

      if self.changes_that_failed_to_apply_earlier:
        self.HandleApplicationFailure(self.changes_that_failed_to_apply_earlier)

      if changes_that_failed_to_submit:
        raise FailedToSubmitAllChangesException(changes_that_failed_to_submit)

    else:
      raise TreeIsClosedException()

  def HandleApplicationFailure(self, changes):
    """Handles changes that were not able to be applied cleanly."""
    for change in changes:
      logging.info('Change %s did not apply cleanly.', change.id)
      if self.is_master:
        change.HandleCouldNotApply(self.gerrit_helper, self.build_log,
                                   dryrun=self.dryrun)

  def HandleValidationFailure(self):
    """Handles failed changes by removing them from next Validation Pools."""
    logging.info('Validation failed for all changes.')
    for change in self.changes:
      logging.info('Validation failed for change %s.', change)
      change.HandleCouldNotVerify(self.gerrit_helper, self.build_log,
                                  dryrun=self.dryrun)

