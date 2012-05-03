# Copyright (c) 2011-2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module that handles interactions with a Validation Pool.

The validation pool is the set of commits that are ready to be validated i.e.
ready for the commit queue to try.
"""

import json
import logging
import os
import time
import urllib
from xml.dom import minidom

from chromite.buildbot import constants
from chromite.buildbot import gerrit_helper
from chromite.buildbot import lkgm_manager
from chromite.buildbot import patch as cros_patch
from chromite.lib import cros_build_lib

_BUILD_DASHBOARD = 'http://build.chromium.org/p/chromiumos'
_BUILD_INT_DASHBOARD = 'http://chromegw/i/chromeos'


def _RunCommand(cmd, dryrun):
  """Runs the specified shell cmd if dryrun=False."""
  if dryrun:
    logging.info('Would have run: %s', ' '.join(cmd))
  else:
    cros_build_lib.RunCommand(cmd, error_ok=True)


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


class NoMatchingChangeFoundException(Exception):
  """Raised if we try to apply a non-existent change."""
  pass


def MarkChangeFailedInflight(change):
  """Set an appropriate error message for an inflight apply failure."""
  change.apply_error_message = (
      'Your change conflicted with another change being tested '
      'in the last validation pool.  Please re-sync, rebase and '
      're-upload.')
  return change


def MarkChangeFailedToT(change):
  """Set an appropriate error message for a ToT apply failure."""
  change.apply_error_message = (
      'Your change no longer cleanly applies against ToT.  '
      'Please re-sync, rebase, and re-upload your change.')
  return change



class ValidationPool(object):
  """Class that handles interactions with a validation pool.

  This class can be used to acquire a set of commits that form a pool of
  commits ready to be validated and committed.

  Usage:  Use ValidationPoo.AcquirePool -- a static
  method that grabs the commits that are ready for validation.
  """

  GLOBAL_DRYRUN = False

  def __init__(self, overlays, build_number, builder_name, is_master, dryrun,
               changes=None, non_os_changes=None,
               conflicting_changes=None):
    """Initializes an instance by setting default valuables to instance vars.

    Generally use AcquirePool as an entry pool to a pool rather than this
    method.

    Args:
      overlays:  One of constants.VALID_OVERLAYS.
      build_number:  Build number for this validation attempt.
      builder_name:  Builder name on buildbot dashboard.
      is_master: True if this is the master builder for the Commit Queue.
      dryrun: If set to True, do not submit anything to Gerrit.
    Optional Args:
      changes: List of changes for this validation pool.
      non_manifest_changes: List of changes that are part of this validation
        pool but aren't part of the cros checkout.
      changes_that_failed_to_apply_earlier: Changes that failed to apply but
        we're keeping around because they conflict with other changes in
        flight.
    """
    build_dashboard = _BUILD_DASHBOARD

    self._public_helper = None
    self._private_helper = None

    # These instances can be instantiated via both older, or newer pickle
    # dumps.  Thus we need to assert the given args since we may be getting
    # a value we no longer like (nor work with).
    if overlays not in constants.VALID_OVERLAYS:
      raise ValueError("Unknown/unsupported overlay: %r" % (overlays,))

    if not isinstance(build_number, int):
      raise ValueError("Invalid build_number: %r" % (build_number,))

    if not isinstance(builder_name, basestring):
      raise ValueError("Invalid builder_name: %r" % (builder_name,))

    for changes_name, changes_value in (
        ('changes', changes), ('non_os_changes', non_os_changes),
        ('conflicting_changes', conflicting_changes)):
      if changes_value is None:
        continue
      if not all(isinstance(x, cros_patch.GitRepoPatch) for x in changes_value):
        raise ValueError(
            'Invalid %s: all elements must be a GitRepoPatch derivative, got %r'
            % (changes_name, changes_value))


    # TODO(sosa): Remove False case once overlays logic has stabilized on TOT.
    if overlays in [constants.PUBLIC_OVERLAYS, constants.BOTH_OVERLAYS, False]:
      self._public_helper = gerrit_helper.GerritHelper(internal=False)

    if overlays in [constants.PRIVATE_OVERLAYS, constants.BOTH_OVERLAYS]:
      build_dashboard = _BUILD_INT_DASHBOARD
      self._private_helper = gerrit_helper.GerritHelper(internal=True)

    self.build_log = '%s/builders/%s/builds/%s' % (
        build_dashboard, builder_name, str(build_number))

    self.is_master = bool(is_master)
    self.dryrun = bool(dryrun) or self.GLOBAL_DRYRUN

    # See optional args for types of changes.
    self.changes = changes or []
    self.non_manifest_changes = non_os_changes or []
    self.changes_that_failed_to_apply_earlier = conflicting_changes or []

    # Private vars only used for pickling.
    self._overlays = overlays
    self._build_number = build_number
    self._builder_name = builder_name
    self._content_merging_projects = None

  def __reduce__(self):
    """Used for pickling to re-create validation pool."""
    return (
        self.__class__,
        (
            self._overlays, self._build_number, self._builder_name,
            self.is_master, self.dryrun, self.changes,
            self.non_manifest_changes,
            self.changes_that_failed_to_apply_earlier))

  @property
  def gerrit_helpers(self):
    return [ h for h in self._public_helper, self._private_helper if h ]

  def _HelperFor(self, change):
    """Returns the Gerrit helper for the |change|."""
    if change.internal:
      return self._private_helper
    else:
      return self._public_helper

  @classmethod
  def _IsTreeOpen(cls, max_timeout=600):
    """Returns True if the tree is open or throttled.

    At the highest level this function checks to see if the Tree is Open.
    However, it also does a robustified wait as the server hosting the tree
    status page is known to be somewhat flaky and these errors can be handled
    with multiple retries.  In addition, it waits around for the Tree to Open
    based on |max_timeout| to give a greater chance of returning True as it
    expects callees to want to do some operation based on a True value.
    If a caller is not interested in this feature they should set |max_timeout|
    to 0.
    """

    # Limit sleep interval to the set of 1-30
    sleep_timeout = min(max(max_timeout / 5, 1), 30)

    def _SleepWithExponentialBackOff(current_sleep):
      """Helper function to sleep with exponential backoff."""
      time.sleep(current_sleep)
      return current_sleep * 2

    def _CanSubmit(status_url):
      """Returns the JSON dictionary response from the status url."""
      max_attempts = 5
      current_sleep = 1
      for _ in range(max_attempts):
        try:
          # Check for successful response code.
          response = urllib.urlopen(status_url)
          if response.getcode() == 200:
            data = json.load(response)
            return data['general_state'] in ('open', 'throttled')

        # We remain robust against IOError's and retry.
        except IOError:
          pass

        current_sleep = _SleepWithExponentialBackOff(current_sleep)
      else:
        # We go ahead and say the tree is open if we can't get the status.
        logging.warn('Could not get a status from %s', status_url)
        return True

    # Check before looping with timeout.
    status_url = 'https://chromiumos-status.appspot.com/current?format=json'
    start_time = time.time()

    if _CanSubmit(status_url):
      return True
    # Loop until either we run out of time or the tree is open.
    while time.time() - start_time < max_timeout:
      if _CanSubmit(status_url):
        return True
      time.sleep(sleep_timeout)

    return False

  @classmethod
  def AcquirePool(cls, overlays, buildroot, build_number, builder_name, dryrun):
    """Acquires the current pool from Gerrit.

    Polls Gerrit and checks for which change's are ready to be committed.

    Args:
      overlays:  One of constants.VALID_OVERLAYS.
      buildroot: The location of the buildroot used to filter projects.
      build_number: Corresponding build number for the build.
      builder_name:  Builder name on buildbot dashboard.
      dryrun: Don't submit anything to gerrit.
    Returns:
      ValidationPool object.
    Raises:
      TreeIsClosedException: if the tree is closed.
    """
    # We choose a longer wait here as we haven't committed to anything yet. By
    # doing this here we can reduce the number of builder cycles.
    if dryrun or cls._IsTreeOpen(max_timeout=3600):
      # Only master configurations should call this method.
      pool = ValidationPool(overlays, build_number, builder_name, True, dryrun)
      # Iterate through changes from all gerrit instances we care about.
      for helper in pool.gerrit_helpers:
        raw_changes = helper.GrabChangesReadyForCommit()
        changes, non_manifest_changes = ValidationPool._FilterNonCrosProjects(
            raw_changes, buildroot)
        pool.changes.extend(changes)
        pool.non_manifest_changes.extend(non_manifest_changes)

      return pool
    else:
      raise TreeIsClosedException()

  @classmethod
  def AcquirePoolFromManifest(cls, manifest, overlays, build_number,
                              builder_name, is_master, dryrun):
    """Acquires the current pool from a given manifest.

    Args:
      manifest: path to the manifest where the pool resides.
      overlays:  One of constants.VALID_OVERLAYS.
      build_number: Corresponding build number for the build.
      builder_name:  Builder name on buildbot dashboard.
      is_master: Boolean that indicates whether this is a pool for a master.
        config or not.
      dryrun: Don't submit anything to gerrit.
    Returns:
      ValidationPool object.
    """
    pool = ValidationPool(overlays, build_number, builder_name, is_master,
                          dryrun)
    manifest_dom = minidom.parse(manifest)
    pending_commits = manifest_dom.getElementsByTagName(
        lkgm_manager.PALADIN_COMMIT_ELEMENT)
    for pending_commit in pending_commits:
      project = pending_commit.getAttribute(lkgm_manager.PALADIN_PROJECT_ATTR)
      change = pending_commit.getAttribute(lkgm_manager.PALADIN_CHANGE_ID_ATTR)
      commit = pending_commit.getAttribute(lkgm_manager.PALADIN_COMMIT_ATTR)

      for helper in pool.gerrit_helpers:
        try:
          patch = helper.GrabPatchFromGerrit(project, change, commit)
          pool.changes.append(patch)
          break
        except gerrit_helper.QueryHasNoResults:
          pass
      else:
        raise NoMatchingChangeFoundException(
            'Could not find change defined by %s' % pending_commit)

    return pool

  @property
  def ContentMergingProjects(self):
    val = self._content_merging_projects
    if val is None:
      val = set()
      for helper in self.gerrit_helpers:
        val.update(helper.FindContentMergingProjects())

      self._content_merging_projects = frozenset(val)

    return val

  @staticmethod
  def _FilterNonCrosProjects(changes, buildroot):
    """Filters changes to a tuple of relevant changes.

    There are many code reviews that are not part of Chromium OS and/or
    only relevant on a different branch. This method returns a tuple of (
    relevant reviews in a manifest, relevant reviews not in the manifest). Note
    that this function must be run while chromite is checked out in a
    repo-managed checkout.

    Args:
      changes:  List of GerritPatch objects.
      buildroot:  Buildroot containing manifest to filter against.

    Returns tuple of
      relevant reviews in a manifest, relevant reviews not in the manifest.
    """

    def IsCrosReview(change):
      return (change.project.startswith('chromiumos') or
              change.project.startswith('chromeos'))

    # First we filter to only Chromium OS repositories.
    changes = [c for c in changes if IsCrosReview(c)]

    manifest_path = os.path.join(buildroot, '.repo', 'manifests/full.xml')
    handler = cros_build_lib.ManifestHandler.ParseManifest(manifest_path)
    projects = handler.projects

    changes_in_manifest = []
    changes_not_in_manifest = []
    for change in changes:
      branch = handler.default.get('revision')
      patch_branch = 'refs/heads/%s' % change.tracking_branch
      project = projects.get(change.project)
      if project:
        branch = project.get('revision') or branch

      if branch == patch_branch:
        if project:
          changes_in_manifest.append(change)
        else:
          changes_not_in_manifest.append(change)
      else:
        logging.info('Filtered change %s', change)

    return changes_in_manifest, changes_not_in_manifest

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

    # Maps internal ID numbers to GerritPatch object for lookup of dependent
    # changes.  All code w/in this function needs to use .id rather than
    # .change_id.
    change_map = dict((change.id, change) for change in self.changes)
    for change in self.changes:
      logging.debug('Trying change %s', change.id)
      helper = self._HelperFor(change)
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
      # TODO(sosa): Modify helper logic to allows deps to be specified across
      # different gerrit instances.

      dependency_exc = None
      try:
        deps.extend(change.GerritDependencies(buildroot))
        deps.extend(change.PaladinDependencies(buildroot))
      except cros_patch.MissingChangeIDException as dependency_exc:
        change.apply_error_message = (
            'Could not apply change %s because change has a Gerrit Dependency '
            'that does not contain a ChangeId.  Please remove this dependency '
            'or update the dependency with a ChangeId.' % change.id)
      except cros_patch.BrokenCQDepends as dependency_exc:
        change.apply_error_message = (
            'Could not apply change %s because change has a malformed '
            'CQ-DEPEND. CQ-DEPEND must either be the long form Change-ID, '
            'or the change number.' % change.id)
      except cros_patch.BrokenChangeID as dependency_exc:
        change.apply_error_message = (
            'Could not apply change %s because a parent change has a malformed '
            'Change-ID.  Please fix and retry.' % change.id)

      if dependency_exc:
        logging.error(change.apply_error_message)
        logging.error(str(dependency_exc))
        changes_that_failed_to_apply_to_tot.add(change)
        apply_chain = False
        continue

      for dep in deps:
        dep_change = change_map.get(dep)
        if not dep_change:
          # The dep may have been committed already.
          try:
            if not helper.IsChangeCommitted(dep, must_match=False):
              message = ('Could not apply change %s because dependent '
                         'change %s is not ready to be committed.' % (
                          change.id, dep))
              logging.info(message)
              change.apply_error_message = message
              changes_that_failed_to_apply_to_tot.add(change)
              apply_chain = False
              break
          except gerrit_helper.QueryNotSpecific:
            message = ('Change %s could not be handled due to its dependency '
                       '%s matching multiple branches.'
                       % (change.id, dep))
            logging.info(message)
            change.apply_error_message = message
            changes_that_failed_to_apply_to_tot.add(change)
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

          # If we're in dryrun mode, then 3way is always allowed.
          # Otherwise, allow 3way only if the gerrit project allows it.
          if self.dryrun:
            trivial = False
          else:
            trivial = change.project not in self.ContentMergingProjects

          change.Apply(buildroot, trivial=trivial)

        except cros_patch.ApplyPatchException as e:
          if e.inflight:
            changes_that_failed_to_apply_against_other_changes.add(
                MarkChangeFailedInflight(change))
          else:
            changes_that_failed_to_apply_to_tot.add(
                MarkChangeFailedToT(change))

          break
        else:
          # We applied the change successfully.
          changes_applied.add(change)
          changes_list.append(change)
          cros_build_lib.PrintBuildbotLink(str(change), change.url)
          if self.is_master:
            self.HandleApplied(change)

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

  # Note: All submit code, all gerrit code, and basically everything other
  # than patch resolution/applying needs to use .change_id from patch objects.
  # Basically all code from this point forward.

  def _SubmitChanges(self, changes):
    """Submits given changes to Gerrit.

    Args:
      changes: GerritPatch's to submit.

    Raises:
      TreeIsClosedException: if the tree is closed.
      FailedToSubmitAllChangesException: if we can't submit a change.
    """
    assert self.is_master, 'Non-master builder calling SubmitPool'
    changes_that_failed_to_submit = []
    # We use the default timeout here as while we want some robustness against
    # the tree status being red i.e. flakiness, we don't want to wait too long
    # as validation can become stale.
    if self.dryrun or ValidationPool._IsTreeOpen():
      for change in changes:
        was_change_submitted = False
        logging.info('Change %s will be submitted', change)
        try:
          self.SubmitChange(change)
          was_change_submitted = self._HelperFor(
              change).IsChangeCommitted(str(change.gerrit_number), self.dryrun)
        except cros_build_lib.RunCommandError:
          logging.error('gerrit review --submit failed for change.')
        finally:
          if not was_change_submitted:
            logging.error('Could not submit %s', str(change))
            self.HandleCouldNotSubmit(change)
            changes_that_failed_to_submit.append(change)

      if changes_that_failed_to_submit:
        raise FailedToSubmitAllChangesException(changes_that_failed_to_submit)

    else:
      raise TreeIsClosedException()

  def SubmitChange(self, change):
    """Submits patch using Gerrit Review."""
    cmd = self._HelperFor(change).GetGerritReviewCommand(
        ['--submit', '%s,%s' % (change.gerrit_number, change.patch_number)])

    _RunCommand(cmd, self.dryrun)

  def SubmitNonManifestChanges(self):
    """Commits changes to Gerrit from Pool that aren't part of the checkout.

    Raises:
      TreeIsClosedException: if the tree is closed.
      FailedToSubmitAllChangesException: if we can't submit a change.
    """
    self._SubmitChanges(self.non_manifest_changes)

  def SubmitPool(self):
    """Commits changes to Gerrit from Pool.  This is only called by a master.

    Raises:
      TreeIsClosedException: if the tree is closed.
      FailedToSubmitAllChangesException: if we can't submit a change.
    """
    self._SubmitChanges(self.changes)
    if self.changes_that_failed_to_apply_earlier:
      self.HandleApplicationFailure(self.changes_that_failed_to_apply_earlier)

  def HandleApplicationFailure(self, changes):
    """Handles changes that were not able to be applied cleanly.

    Args:
      changes: GerritPatch's to handle.
    """
    for change in changes:
      logging.info('Change %s did not apply cleanly.', change.change_id)
      if self.is_master:
        self.HandleCouldNotApply(change)

  def HandleValidationFailure(self):
    """Handles failed changes by removing them from next Validation Pools."""
    logging.info('Validation failed for all changes.')
    for change in self.changes:
      logging.info('Validation failed for change %s.', change)
      self.HandleCouldNotVerify(change)

  def HandleValidationTimeout(self):
    """Handles changes that timed out."""
    logging.info('Validation timed out for all changes.')
    for change in self.changes:
      logging.info('Validation timed out for change %s.', change)
      self._SendNotification(change,
          'The Commit Queue timed out while verifying your change in '
          '%(build_log)s . This means that a supporting builder did not '
          'finish building your change within the specified timeout. If you '
          'believe this happened in error, just re-mark your commit as ready. '
          'Your change will then get automatically retried.')
      self._HelperFor(change).RemoveCommitReady(change, dryrun=self.dryrun)

  def _SendNotification(self, change, msg):
    msg %= {'build_log':self.build_log}
    PaladinMessage(msg, change, self._HelperFor(change)).Send(self.dryrun)

  def HandleCouldNotSubmit(self, change):
    """Handler that is called when Paladin can't submit a change.

    This should be rare, but if an admin overrides the commit queue and commits
    a change that conflicts with this change, it'll apply, build/validate but
    receive an error when submitting.

    Args:
      change: GerritPatch instance to operate upon.
    """
    self._SendNotification(change,
        'The Commit Queue failed to submit your change in %(build_log)s . '
        'This can happen if you submitted your change or someone else '
        'submitted a conflicting change while your change was being tested.')
    self._HelperFor(change).RemoveCommitReady(change, dryrun=self.dryrun)

  def HandleCouldNotVerify(self, change):
    """Handler for when Paladin fails to validate a change.

    This handler notifies set Verified-1 to the review forcing the developer
    to re-upload a change that works.  There are many reasons why this might be
    called e.g. build or testing exception.

    Args:
      change: GerritPatch instance to operate upon.
    """
    self._SendNotification(change,
        'The Commit Queue failed to verify your change in %(build_log)s . '
        'If you believe this happened in error, just re-mark your commit as '
        'ready. Your change will then get automatically retried.')
    self._HelperFor(change).RemoveCommitReady(change, dryrun=self.dryrun)

  def HandleCouldNotApply(self, change):
    """Handler for when Paladin fails to apply a change.

    This handler notifies set CodeReview-2 to the review forcing the developer
    to re-upload a rebased change.

    Args:
      change: GerritPatch instance to operate upon.
    """
    msg = 'The Commit Queue failed to apply your change in %(build_log)s . '
    # This is written this way to protect against bugs in CQ itself.  We log
    # it both to the build output, and mark the change w/ it.
    extra_msg = getattr(change, 'apply_error_message', None)
    if extra_msg is None:
      logging.error(
          'Change %s was passed to HandleCouldNotApply without an appropriate '
          'apply_error_message set.  Internal bug.', change)
      extra_msg = (
          'Internal CQ issue: extra error info was not given,  Please contact '
          'the build team and ensure they are aware of this specific change '
          'failing.')

    msg += extra_msg
    self._SendNotification(change, msg)
    self._HelperFor(change).RemoveCommitReady(change, dryrun=self.dryrun)

  def HandleApplied(self, change):
    """Handler for when Paladin successfully applies a change.

    This handler notifies a developer that their change is being tried as
    part of a Paladin run defined by a build_log.

    Args:
      change: GerritPatch instance to operate upon.
    """
    self._SendNotification(change,
        'The Commit Queue has picked up your change. '
        'You can follow along at %(build_log)s .')


class PaladinMessage():
  """An object that is used to send messages to developers about their changes.
  """
  # URL where Paladin documentation is stored.
  _PALADIN_DOCUMENTATION_URL = ('http://www.chromium.org/developers/'
                                'tree-sheriffs/sheriff-details-chromium-os/'
                                'commit-queue-overview')

  def __init__(self, message, patch, helper):
    self.message = message
    self.patch = patch
    self.helper = helper

  def _ConstructPaladinMessage(self):
    """Adds any standard Paladin messaging to an existing message."""
    return self.message + (' Please see %s for more information.' %
                           self._PALADIN_DOCUMENTATION_URL)

  def Send(self, dryrun):
    """Sends the message to the developer."""
    cmd = self.helper.GetGerritReviewCommand(
        ['-m', '"%s"' % self._ConstructPaladinMessage(),
         '%s,%s' % (self.patch.gerrit_number, self.patch.patch_number)])
    _RunCommand(cmd, dryrun)
