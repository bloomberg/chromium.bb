# Copyright (c) 2011-2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module that handles interactions with a Validation Pool.

The validation pool is the set of commits that are ready to be validated i.e.
ready for the commit queue to try.
"""

import contextlib
import json
import logging
import sys
import time
import urllib
from xml.dom import minidom

from chromite.buildbot import constants
from chromite.buildbot import gerrit_helper
from chromite.buildbot import lkgm_manager
from chromite.buildbot import patch as cros_patch
from chromite.lib import cros_build_lib

_BUILD_DASHBOARD = 'http://build.chromium.org/p/chromiumos'
_BUILD_INT_DASHBOARD = 'http://uberchromegw.corp.google.com/i/chromeos'

# We import mox so that w/in ApplyPoolIntoRepo, if a mox exception is
# thrown, we don't cover it up.
try:
  import mox
except ImportError:
  mox = None


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


class InternalCQError(cros_patch.PatchException):
  """Exception thrown when CQ has an unexpected/unhandled error."""

  def __init__(self, patch, message):
    cros_patch.PatchException.__init__(self, patch, message=message)

  def __str__(self):
    return "Patch %s failed to apply due to a CQ issue: %s" % (
        self.patch, self.message)


class NoMatchingChangeFoundException(Exception):
  """Raised if we try to apply a non-existent change."""


class DependencyNotReadyForCommit(cros_patch.PatchException):
  """Exception thrown when a required dep isn't satisfied."""

  def __init__(self, patch, unsatisfied_dep):
    cros_patch.PatchException.__init__(self, patch)
    self.unsatisfied_dep = unsatisfied_dep
    self.args += (unsatisfied_dep,)

  def __str__(self):
    return ("Change %s isn't ready for CQ/commit since its dependency "
            "%s isn't committed, or marked as Commit-Ready."
            % (self.patch, self.unsatisfied_dep))


def _RunCommand(cmd, dryrun):
  """Runs the specified shell cmd if dryrun=False."""
  if dryrun:
    logging.info('Would have run: %s', ' '.join(cmd))
  else:
    cros_build_lib.RunCommand(cmd, error_ok=True)


class HelperPool(object):
  """Pool of allowed GerritHelpers to be used by CQ/PatchSeries."""

  def __init__(self, internal=None, external=None):
    """Initialize this instance with the given handlers.

    Most likely you want the classmethod SimpleCreate which takes boolean
    options.

    If a given handler is None, then it's disabled; else the passed in
    object is used.

    """
    self._external = external
    self._internal = internal

  @classmethod
  def SimpleCreate(cls, internal=True, external=True):
    """Classmethod helper for creating a HelperPool from boolean options.

    Args:
      internal: If True, allow access to a GerritHelper for internal.
      external: If True, allow access to a GerritHelper for external.
    Returns:
      An appropriately configured HelperPool instance.
    """

    external = gerrit_helper.GerritHelper(internal=False) if external else None
    internal = gerrit_helper.GerritHelper(internal=True) if internal else None
    return cls(internal=internal, external=external)

  def ForChange(self, change):
    """Return the helper to use for a particular change.

    If no helper is configured, an Exception is raised.
    """
    return self.GetHelper(change.internal)

  def GetHelper(self, internal=False):
    """Return the helper to use for internal versus external.

    If no helper is configured, an Exception is raised.
    """
    if internal:
      if self._internal:
        return self._internal
    elif self._external:
      return self._external

    raise AssertionError(
        'Asked for an internal=%r helper, but none are allowed in this '
        'configuration.  This strongly points at the possibility of an '
        'internal bug.'
        % (internal,))

  def __iter__(self):
    for helper in (self._external, self._internal):
      if helper:
        yield helper


def _PatchWrapException(functor):
  """Decorator to intercept patch exceptions and wrap them.

  Specifically, for known/handled Exceptions, it intercepts and
  converts it into a DependencyError- via that, preserving the
  cause, while casting it into an easier to use form (one that can
  be chained in addition)."""
  def f(self, parent, *args, **kwds):
    try:
      return functor(self, parent, *args, **kwds)
    except gerrit_helper.GerritException, e:
      new_exc = cros_patch.PatchException(parent, e)
      raise new_exc.__class__, new_exc, sys.exc_info()[2]
    except cros_patch.PatchException, e:
      if e.patch.id == parent.id:
        raise
      new_exc = cros_patch.DependencyError(parent, e)
      raise new_exc.__class__, new_exc, sys.exc_info()[2]

  f.__name__ = functor.__name__
  return f


class RawPatchSeries(object):
  """Class representing a set of patches applied to a repository."""

  def __init__(self, path, helper_pool=None):

    if helper_pool is None:
      helper_pool = HelperPool.SimpleCreate(internal=True, external=True)
    self._helper_pool = helper_pool
    self._path = path

    self.applied = []
    self.failed = []
    self.failed_tot = {}

    # A mapping of ChangeId to exceptions if the patch failed against
    # ToT.  Primarily used to keep the resolution/applying from going
    # down known bad paths.
    self._committed_cache = {}
    self._lookup_cache = {}
    self._change_deps_cache = {}

  def _GetTrackingBranchForChange(self, change):
    return change.tracking_branch

  def _GetGitRepoForChange(self, _change):
    return self._path

  def ApplyChange(self, change, dryrun=False):
    # Always allow 3way for any patch application against a standalone
    # repository.
    # pylint: disable=W0613
    return change.Apply(self._path, change.tracking_branch, trivial=False)

  def _GetGerritPatch(self, change, query):
    """Query the configured helpers looking for a given change.

    Args:
      change: A cros_patch.GitRepoPatch derivative that we're querying
        on behalf of.
      query: The ChangeId or Change Number we're searching for.
    """
    helper = self._helper_pool.ForChange(change)
    change = helper.QuerySingleRecord(query, must_match=True)
    self.InjectLookupCache([change])
    return change

  @_PatchWrapException
  def _LookupAndFilterChanges(self, parent, merged, deps, frozen=False):
    """Given a set of deps (changes), return unsatisfied dependencies.

    Args:
      parent: The change we're resolving for.
      merged: A container of changes we should consider as merged already.
      deps: A sequence of dependencies for the parent that we need to identify
        as either merged, or needing resolving.
      frozen: If True, then raise an DependencyNotReady exception if any
        new dependencies are required by this change that weren't already
        supplied up front. This is used by the Commit Queue to notify users
        when a change they have marked as 'Commit Ready' requires a change that
        has not yet been marked as 'Commit Ready'.
    Returns:
      A sequence of cros_patch.GitRepoPatch instances (or derivatives) that
      need to be resolved for this change to be mergable.
    """
    unsatisfied = []
    for dep in deps:
      if dep in self._committed_cache:
        continue

      dep_change = self._lookup_cache.get(dep)
      if dep_change is not None:
        if dep_change not in merged and dep_change not in unsatisfied:
          unsatisfied.append(dep_change)
        continue

      dep_change = self._GetGerritPatch(parent, dep)
      if dep_change.IsAlreadyMerged():
        self.InjectCommittedPatches([dep_change])
      elif frozen:
        raise DependencyNotReadyForCommit(parent, dep)

      if dep_change is not None:
        assert dep == dep_change.id

      unsatisfied.append(dep_change)
    return unsatisfied

  def CreateTransaction(self, change, frozen=False):
    """Given a change, resolve it into a transaction.

    In this case, a transaction is defined as a group of commits that
    must land for the given change to be merged- specifically its
    parent deps, and its CQ-DEPENDS.

    Args:
      change: A cros_patch.GitRepoPatch instance to generate a transaction
        for.
      buildroot: Pathway to the root of a repo checkout to work on.
      frozen: If True, then resolution is limited purely to what is in
        the set of allowed changes; essentially, CQ mode.  If False,
        arbitrary resolution is allowed, pulling changes as necessary
        to create the transaction.
    Returns:
      A sequency of the necessary cros_patch.GitRepoPatch objects for
      this transaction.
    """
    plan, stack = [], []
    self._ResolveChange(change, plan, stack, frozen=frozen)
    return plan

  def _ResolveChange(self, change, plan, stack, frozen=False):
    """Helper for resolving a node and its dependencies into the plan.

    No external code should call this; all internal code should invoke this
    rather than ResolveTransaction since this maintains the necessary stack
    tracking that is used to detect and handle cyclic dependencies.

    Raises:
      If the change couldn't be resolved, a DependencyError or
      cros_patch.PatchException can be raised.
    """
    if change.id in self._committed_cache:
      return
    if change in stack:
      # If the requested change is already in the stack, then immediately
      # return- it's a cycle (requires CQ-DEPEND for it to occur); if
      # the earlier resolution attempt succeeds, than implicitly this
      # attempt will.
      # TODO(ferringb,sosa): this check actually doesn't handle gerrit
      # change numbers; support for that is broken currently anyways,
      # but this is one of the spots that needs fixing for that support.
      return
    stack.append(change)
    try:
      self._PerformResolveChange(change, plan,
                                 stack, frozen=frozen)
    finally:
      stack.pop(-1)

  @_PatchWrapException
  def _GetDepsForChange(self, change):
    """Look up the gerrit/paladin deps for a change

    Raises:
      DependencyError: Thrown if there is an issue w/ the commits
        metadata (either couldn't find the parent, or bad CQ-DEPEND).

    Returns:
      A tuple of the change's GerritDependencies(), and PaladinDependencies()
    """
    # TODO(sosa, ferringb): Modify helper logic to allows deps to be specified
    # across different gerrit instances.
    val = self._change_deps_cache.get(change)
    if val is None:
      git_repo = self._GetGitRepoForChange(change)
      val = self._change_deps_cache[change] = (
          change.GerritDependencies(
              git_repo, self._GetTrackingBranchForChange(change)),
          change.PaladinDependencies(git_repo))
    return val

  def _PerformResolveChange(self, change, plan, stack, frozen=False):
    """Resolve and ultimately add a change into the plan."""
    # Pull all deps up front, then process them.  Simplifies flow, and
    # localizes the error closer to the cause.
    gdeps, pdeps = self._GetDepsForChange(change)
    gdeps = self._LookupAndFilterChanges(change, plan, gdeps, frozen=frozen)
    pdeps = self._LookupAndFilterChanges(change, plan, pdeps, frozen=frozen)

    def _ProcessDeps(deps):
      for dep in deps:
        if dep in plan:
          continue
        try:
          self._ResolveChange(dep, plan, stack, frozen=frozen)
        except cros_patch.PatchException, e:
          raise cros_patch.DependencyError, \
                cros_patch.DependencyError(change, e), \
                sys.exc_info()[2]

    _ProcessDeps(gdeps)
    plan.append(change)
    _ProcessDeps(pdeps)

  def InjectCommittedPatches(self, changes):
    """Record that the given patches are already committed.

    This is primarily useful for external code to notify this object
    that changes were applied to the tree outside its purview- specifically
    useful for dependency resolution."""
    for change in changes:
      self._committed_cache[change.id] = change

  def InjectLookupCache(self, changes):
    """Inject into the internal lookup cache the given changes, using them
    (rather than asking gerrit for them) as needed for dependencies.
    """
    for change in changes:
      self._lookup_cache[change.id] = change

  def FetchChanges(self, changes):
    for change in changes:
      change.Fetch(self._GetGitRepoForChange(change))

  def Apply(self, changes, dryrun=False, frozen=True, honor_ordering=False,
            changes_checker=None):
    """Applies changes from pool into the directory specified by the buildroot.

    This method resolves each given change down into a set of transactions-
    the change and its dependencies- that must go in, then tries to apply
    the largest transaction first, working its way down.

    If a transaction cannot be applied, then it is rolled back
    in full- note that if a change is involved in multiple transactions,
    if an earlier attempt fails, that change can be retried in a new
    transaction if the failure wasn't caused by the patch being incompatible
    to ToT.

    Args:
      changes: A sequence of cros_patch.GitRepoPatch instances to resolve
        and apply.
      dryrun: If True, then content-merging is explicitly forced,
        and no modifications to gerrit will occur.
      frozen: If True, then resolving of the given changes is explicitly
        limited to just the passed in changes, or known committed changes.
        This is basically CQ/Paladin mode, used to limit the changes being
        pulled in/committed to just what we allow.
      honor_ordering: Apply normally will reorder the transactions it
        computes, trying the largest first, then degrading through smaller
        transactions if the larger of the two fails.  If honor_ordering
        is False, then the ordering given via changes is preserved-
        this is mainly of use for cbuildbot induced patching, and shouldn't
        be used for CQ patching.
      changes_checker: If not None, must be a functor taking two arguments:
        series, changes.  This is invoked after the changes have been fetched,
        thus this is a way for consumers to do last minute checking of the
        changes being inspected.  Primarily of use for cbuildbot patching.
    Returns:
      A tuple of changes-applied, Exceptions for the changes that failed
      against ToT, and Exceptions that failed inflight;  These exceptions
      are cros_patch.PatchException instances.
    """

    # Prefetch the changes; we need accurate change_id/id's, which is
    # guaranteed via Fetch.
    self.FetchChanges(changes)
    if changes_checker:
      changes_checker(self, changes)

    self.InjectLookupCache(changes)
    resolved, applied, failed = [], [], []
    for change in changes:
      try:
        resolved.append((change,
                         self.CreateTransaction(change, frozen=frozen)))
      except cros_patch.PatchException, e:
        logging.info("Failed creating transaction for %s: %s", change, e)
        failed.append(e)

    if not resolved:
      # No work to do; either no changes were given to us, or all failed
      # to be resolved.
      return [], failed, []

    if not honor_ordering:
      # Sort by length, falling back to the order the changes were given to us.
      # This is done to prefer longer transactions (more painful to rebase)
      # over shorter transactions.
      position = dict((change, idx) for idx, change in enumerate(changes))
      def mk_key(data):
        ids = [x.id for x in data[1]]
        return -len(ids), position[data[0]]
      resolved.sort(key=mk_key)

    for inducing_change, transaction_changes in resolved:
      try:
        with self._Transaction(transaction_changes):
          logging.debug("Attempting transaction for %s: changes: %s",
                        inducing_change,
                        ', '.join(map(str, transaction_changes)))
          self._ApplyChanges(inducing_change, transaction_changes,
                             dryrun=dryrun)
      except cros_patch.PatchException, e:
        logging.info("Failed applying transaction for %s: %s",
                     inducing_change, e)
        failed.append(e)
      else:
        applied.extend(transaction_changes)
        self.InjectCommittedPatches(transaction_changes)

    # Uniquify while maintaining order.
    def _uniq(l):
      s = set()
      for x in l:
        if x not in s:
          yield x
          s.add(x)

    applied = list(_uniq(applied))

    failed = [x for x in failed if x.patch not in applied]
    failed_tot = [x for x in failed if not x.inflight]
    failed_inflight = [x for x in failed if x.inflight]
    return applied, failed_tot, failed_inflight

  @contextlib.contextmanager
  def _Transaction(self, commits):
    """ContextManager used to rollback changes to a buildroot if necessary.

    Specifically, if an unhandled non system exception occurs, this context
    manager will roll back all relevant modifications to the git repos
    involved.

    Args:
      buildroot: The manifest checkout we're operating upon, specifically
        the root of it.
      commits: A sequence of cros_patch.GitRepoPatch instances that compromise
        this transaction- this is used to identify exactly what may be changed,
        thus what needs to be tracked and rolled back if the transaction fails.
    """
    # First, the book keeping code; gather required data so we know what
    # to rollback to should this transaction fail.  Specifically, we track
    # what was checked out for each involved repo, and if it was a branch,
    # the sha1 of the branch; that information is enough to rewind us back
    # to the original repo state.
    project_state = set(map(self._GetGitRepoForChange, commits))
    resets, checkouts = [], []
    for project_dir in project_state:
      current_sha1 = cros_build_lib.RunGitCommand(
          project_dir, ['rev-list', '-n1', 'HEAD']).output.strip()
      assert current_sha1

      result = cros_build_lib.RunGitCommand(
          project_dir, ['symbolic-ref', 'HEAD'], error_code_ok=True)
      if result.returncode == 128: # Detached HEAD.
        checkouts.append((project_dir, current_sha1))
      elif result.returncode == 0:
        checkouts.append((project_dir, result.output.strip()))
        resets.append((project_dir, current_sha1))
      else:
        raise Exception(
            'Unexpected state from git symbolic-ref HEAD: exit %i\n'
            'stdout: %s\nstderr: %s'
            % (result.returncode, result.output, result.error))

    committed_cache = self._committed_cache.copy()

    try:
      yield
      # Reaching here means it was applied cleanly, thus return.
      return
    except (MemoryError, RuntimeError):
      # Skip transactional rollback; if these occur, at least via
      # the scenarios where they're *supposed* to be raised, we really
      # should let things fail hard here.
      raise
    except:
      # pylint: disable=W0702
      logging.info("Rewinding transaction: failed changes: %s .",
                   ', '.join(map(str, commits)))
      for project_dir, ref in checkouts:
        cros_build_lib.RunGitCommand(project_dir, ['checkout', ref])

      for project_dir, sha1 in resets:
        cros_build_lib.RunGitCommand(project_dir, ['reset', '--hard', sha1])

      self._committed_cache = committed_cache
      raise

  @_PatchWrapException
  def _ApplyChanges(self, _inducing_change, changes, dryrun=False):
    """Apply a given ordered sequence of changes.

    Args:
      _inducing_change: The core GitRepoPatch instance that lead to this
        sequence of changes; basically what this transaction was computed from.
        Needs to be passed in so that the exception wrapping machinery can
        convert any failures, assigning blame appropriately.
      manifest: A ManifestCheckout instance representing what we're working on.
      changes: A ordered sequence of GitRepoPatch instances to apply.
      dryrun: Whether or not this is considered a production run.
    """
    # Bail immediately if we know one of the requisite patches won't apply.
    for change in changes:
      failure = self.failed_tot.get(change.id)
      if failure is not None:
        raise failure

    applied = []
    for change in changes:
      if change.id in self._committed_cache:
        continue

      try:
        self.ApplyChange(change, dryrun=dryrun)
      except cros_patch.PatchException, e:
        if not e.inflight:
          self.failed_tot[change.id] = e
        raise
      applied.append(change)
      if hasattr(change, 'url'):
        cros_build_lib.PrintBuildbotLink(str(change), change.url)

    logging.debug('Done investigating changes.  Applied %s',
                  ' '.join([c.id for c in applied]))


class PatchSeries(RawPatchSeries):
  """Class representing a set of patches applied to a repository."""

  def __init__(self, path, helper_pool=None, force_content_merging=False):
    RawPatchSeries.__init__(self, path, helper_pool=helper_pool)

    self.manifest = None
    self._content_merging_projects = {}
    self.force_content_merging = force_content_merging

  def _GetTrackingBranchForChange(self, change):
    ref = self.manifest.GetProjectsLocalRevision(change.project)
    if ref.startswith("refs/"):
      return ref
    # Revlocked manifests return sha1s; use the repo defined branch
    # so tracking is supported.
    return self.manifest.default_branch

  #pylint: disable=W0221
  def Apply(self, changes, **kwargs):
    """Apply the given changes against a manifest checkout.

    See RawPatchSeries.Apply for the argument details; this adds a single
    optional argument:
    Args:
      manifest: If given, the manifest object to use.  Else one is created
        for the configured buildroot.
    """
    manifest = kwargs.pop('manifest', None)
    if manifest is None:
      manifest = cros_build_lib.ManifestCheckout(self._path)
    self.manifest = manifest
    return RawPatchSeries.Apply(self, changes, **kwargs)

  def _IsContentMerging(self, change, dryrun=False):
    """Discern if the given change has Content Merging enabled in gerrit.

    Note if the instance was created w/ force_content_merging=True,
    then this function will lie and always return True to avoid the
    admin-level access required of <=gerrit-2.1.

    Raises:
      AssertionError: If the gerrit helper requested is disallowed.
      GerritException: If there is a failure in querying gerrit.
    Returns:
      True if the change's project has content merging enabled, False if not.
    """
    if self.force_content_merging or dryrun:
      return True
    helper = self._helper_pool.ForChange(change)

    if not helper.version.startswith('2.1'):
      return self.manifest.ProjectIsContentMerging(change.project)

    # Fallback to doing gsql trickery to get it; note this requires admin
    # access.
    projects = self._content_merging_projects.get(helper)
    if projects is None:
      projects = helper.FindContentMergingProjects()
      self._content_merging_projects[helper] = projects

    return change.project in projects

  def _GetGitRepoForChange(self, change):
    return self.manifest.GetProjectPath(change.project, True)

  def ApplyChange(self, change, dryrun=False):
    # If we're in dryrun mode, then 3way is always allowed.
    # Otherwise, allow 3way only if the gerrit project allows it.
    trivial = False if dryrun else not self._IsContentMerging(change)

    return change.ApplyAgainstManifest(self.manifest, trivial=trivial)


class ValidationPool(object):
  """Class that handles interactions with a validation pool.

  This class can be used to acquire a set of commits that form a pool of
  commits ready to be validated and committed.

  Usage:  Use ValidationPool.AcquirePool -- a static
  method that grabs the commits that are ready for validation.
  """

  GLOBAL_DRYRUN = False

  def __init__(self, overlays, build_root, build_number, builder_name,
               is_master, dryrun, changes=None, non_os_changes=None,
               conflicting_changes=None, helper_pool=None):
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
      helper_pool: A HelperPool instance.  If not specified, a HelperPool
        instance is created with it's access limit discerned via looking at
        overlays.
    """

    if helper_pool is None:
      helper_pool = self.GetGerritHelpersForOverlays(overlays)

    self.build_root = build_root
    self._helper_pool = helper_pool

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
        ('changes', changes), ('non_os_changes', non_os_changes)):
      if not changes_value:
        continue
      if not all(isinstance(x, cros_patch.GitRepoPatch) for x in changes_value):
        raise ValueError(
            'Invalid %s: all elements must be a GitRepoPatch derivative, got %r'
            % (changes_name, changes_value))

    if conflicting_changes and not all(
        isinstance(x, cros_patch.PatchException)
        for x in conflicting_changes):
      raise ValueError(
          'Invalid conflicting_changes: all elements must be a '
          'cros_patch.PatchException derivative, got %r'
          % (conflicting_changes,))

    build_dashboard = self.GetBuildDashboardForOverlays(overlays)

    self.build_log = '%s/builders/%s/builds/%s' % (
        build_dashboard, builder_name, str(build_number))

    self.is_master = bool(is_master)
    self.dryrun = bool(dryrun) or self.GLOBAL_DRYRUN

    # See optional args for types of changes.
    self.changes = changes or []
    self.non_manifest_changes = non_os_changes or []
    # Note, we hold onto these CLs since they conflict against our current CLs
    # being tested; if our current ones succeed, we notify the user to deal
    # w/ the conflict.  If the CLs we're testing fail, then there is no
    # reason we can't try these again in the next run.
    self.changes_that_failed_to_apply_earlier = conflicting_changes or []

    # Private vars only used for pickling.
    self._overlays = overlays
    self._build_number = build_number
    self._builder_name = builder_name
    self._patch_series = PatchSeries(self.build_root, helper_pool=helper_pool)

  @staticmethod
  def GetBuildDashboardForOverlays(overlays):
    """Discern the dashboard to use based on the given overlay."""
    if overlays in [constants.PRIVATE_OVERLAYS, constants.BOTH_OVERLAYS]:
      return _BUILD_INT_DASHBOARD
    return _BUILD_DASHBOARD

  @staticmethod
  def GetGerritHelpersForOverlays(overlays):
    """Discern the allowed GerritHelpers to use based on the given overlay."""
    # TODO(sosa): Remove False case once overlays logic has stabilized on TOT.
    internal = external = False
    if overlays in [constants.PUBLIC_OVERLAYS, constants.BOTH_OVERLAYS, False]:
      external = True

    if overlays in [constants.PRIVATE_OVERLAYS, constants.BOTH_OVERLAYS]:
      internal = True

    return HelperPool.SimpleCreate(internal=internal, external=external)

  def __reduce__(self):
    """Used for pickling to re-create validation pool."""
    return (
        self.__class__,
        (
            self._overlays,
            self.build_root, self._build_number, self._builder_name,
            self.is_master, self.dryrun, self.changes,
            self.non_manifest_changes,
            self.changes_that_failed_to_apply_earlier))

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
    if cros_build_lib.GetChromiteTrackingBranch() != 'master':
      cros_build_lib.Info('Not checking tree status as not tracking master.')
      return True

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
    logging.info('Waiting for the tree to open...')
    while time.time() - start_time < max_timeout:
      if _CanSubmit(status_url):
        return True
      time.sleep(sleep_timeout)

    return False

  @classmethod
  def AcquirePool(cls, overlays, build_root, build_number, builder_name,
                  dryrun=False):
    """Acquires the current pool from Gerrit.

    Polls Gerrit and checks for which change's are ready to be committed.

    Args:
      overlays:  One of constants.VALID_OVERLAYS.
      build_root: The location of the build root used to filter projects, and
        to apply patches against.
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
      pool = ValidationPool(overlays, build_root, build_number, builder_name,
                            True, dryrun)
      # Iterate through changes from all gerrit instances we care about.
      for helper in cls.GetGerritHelpersForOverlays(overlays):
        raw_changes = helper.GrabChangesReadyForCommit()
        changes, non_manifest_changes = ValidationPool._FilterNonCrosProjects(
            raw_changes, build_root)
        pool.changes.extend(changes)
        pool.non_manifest_changes.extend(non_manifest_changes)

      return pool
    else:
      raise TreeIsClosedException()

  @classmethod
  def AcquirePoolFromManifest(cls, manifest, overlays, build_root, build_number,
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
    pool = ValidationPool(overlays, build_root, build_number, builder_name,
                          is_master, dryrun)
    manifest_dom = minidom.parse(manifest)
    pending_commits = manifest_dom.getElementsByTagName(
        lkgm_manager.PALADIN_COMMIT_ELEMENT)
    for pending_commit in pending_commits:
      project = pending_commit.getAttribute(lkgm_manager.PALADIN_PROJECT_ATTR)
      change = pending_commit.getAttribute(lkgm_manager.PALADIN_CHANGE_ID_ATTR)
      commit = pending_commit.getAttribute(lkgm_manager.PALADIN_COMMIT_ATTR)

      for helper in cls.GetGerritHelpersForOverlays(overlays):
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

    handler = cros_build_lib.ManifestCheckout.Cached(buildroot)
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

  def ApplyPoolIntoRepo(self, manifest=None):
    """Applies changes from pool into the directory specified by the buildroot.

    This method applies changes in the order specified.  It also respects
    dependency order.
    Returns:

    True if we managed to apply any changes.
    """
    try:
      applied, failed_tot, failed_inflight = self._patch_series.Apply(
          self.changes, dryrun=self.dryrun, manifest=manifest)
    except (KeyboardInterrupt, RuntimeError, SystemExit):
      raise
    except Exception, e:
      if mox is not None and isinstance(e, mox.Error):
        raise

      # Stash a copy of the tb guts, since the next set of steps can
      # wipe it.
      exc = sys.exc_info()
      msg = (
          "Unhandled Exception occurred during CQ's Apply: %s\n"
          "Failing the entire series to prevent CQ from going into an "
          "infinite loop hanging on these CLs." % (e,))
      cros_build_lib.Error(
          "%s\nAffected Patches are: %s", msg,
          ', '.join(x.change_id for x in self.changes))
      try:
        self._HandleApplyFailure(
            [InternalCQError(patch, msg) for patch in self.changes])
      # pylint: disable=W0703
      except Exception, e:
        if mox is None or not isinstance(e, mox.Error):
          # If it's not a mox error, let it fly.
          raise
      raise exc[0], exc[1], exc[2]

    if self.is_master:
      for change in applied:
        self._HandleApplySuccess(change)

    if failed_tot:
      logging.info(
          'The following changes could not cleanly be applied to ToT: %s',
          ' '.join([c.patch.id for c in failed_tot]))
      self._HandleApplyFailure(failed_tot)

    if failed_inflight:
      logging.info(
          'The following changes could not cleanly be applied against the '
          'current stack of patches; if this stack fails, they will be tried '
          'in the next run.  Inflight failed changes: %s',
          ' '.join([c.patch.id for c in failed_inflight]))

    self.changes_that_failed_to_apply_earlier.extend(failed_inflight)
    self.changes = applied

    return bool(self.changes)

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
    if not self.dryrun and not self._IsTreeOpen():
      raise TreeIsClosedException()

    for change in changes:
      was_change_submitted = False
      logging.info('Change %s will be submitted', change)
      try:
        self._SubmitChange(change)
        was_change_submitted = self._helper_pool.ForChange(
            change).IsChangeCommitted(str(change.gerrit_number), self.dryrun)
      except cros_build_lib.RunCommandError:
        logging.error('gerrit review --submit failed for change.')
      finally:
        if not was_change_submitted:
          logging.error('Could not submit %s', str(change))
          self._HandleCouldNotSubmit(change)
          changes_that_failed_to_submit.append(change)

    if changes_that_failed_to_submit:
      raise FailedToSubmitAllChangesException(changes_that_failed_to_submit)

  def _SubmitChange(self, change):
    """Submits patch using Gerrit Review."""
    cmd = self._helper_pool.ForChange(change).GetGerritReviewCommand(
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
    # Note that _SubmitChanges can throw an exception if it can't
    # submit all changes; in that particular case, don't mark the inflight
    # failures patches as failed in gerrit- some may apply next time we do
    # a CQ run (since the# submit state has changed, we have no way of
    # knowing).  They *likely* will still fail, but this approach tries
    # to minimize wasting the developers time.
    self._SubmitChanges(self.changes)
    if self.changes_that_failed_to_apply_earlier:
      self._HandleApplyFailure(self.changes_that_failed_to_apply_earlier)

  def _HandleApplyFailure(self, failures):
    """Handles changes that were not able to be applied cleanly.

    Args:
      changes: GerritPatch's to handle.
    """
    for failure in failures:
      logging.info('Change %s did not apply cleanly.', failure.patch)
      if self.is_master:
        self._HandleCouldNotApply(failure)

  def _HandleCouldNotApply(self, failure):
    """Handler for when Paladin fails to apply a change.

    This handler notifies set CodeReview-2 to the review forcing the developer
    to re-upload a rebased change.

    Args:
      change: GerritPatch instance to operate upon.
    """
    msg = 'The Commit Queue failed to apply your change in %(build_log)s .'
    msg += '  %(failure)s'
    self._SendNotification(failure.patch, msg, failure=failure)
    self._helper_pool.ForChange(failure.patch).RemoveCommitReady(
        failure.patch, dryrun=self.dryrun)

  def HandleValidationFailure(self, failed_stage=None, exception=None):
    """Handles failed changes by removing them from next Validation Pools."""
    logging.info('Validation failed for all changes.')
    for change in self.changes:
      logging.info('Validation failed for change %s.', change)
      self._HandleCouldNotVerify(change, failed_stage=failed_stage,
                                 exception=exception)

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
      self._helper_pool.ForChange(change).RemoveCommitReady(
          change, dryrun=self.dryrun)

  def _SendNotification(self, change, msg, **kwargs):
    d = dict(build_log=self.build_log, **kwargs)
    try:
      msg %= d
    except (TypeError, ValueError), e:
      logging.error(
          "Generation of message %s for change %s failed: dict was %r, "
          "exception %s", msg, change, d, e)
      raise e.__class__(
          "Generation of message %s for change %s failed: dict was %r, "
          "exception %s" % (msg, change, d, e))
    PaladinMessage(msg, change, self._helper_pool.ForChange(change)).Send(
        self.dryrun)

  def _HandleCouldNotSubmit(self, change):
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
    self._helper_pool.ForChange(change).RemoveCommitReady(
        change, dryrun=self.dryrun)

  def _HandleCouldNotVerify(self, change, failed_stage=None, exception=None):
    """Handler for when Paladin fails to validate a change.

    This handler notifies set Verified-1 to the review forcing the developer
    to re-upload a change that works.  There are many reasons why this might be
    called e.g. build or testing exception.

    Args:
      change: GerritPatch instance to operate upon.
      failed_stage: If not None, the name of the first stage that failed.
      exception: The exception object thrown by the first failure.
    """
    if failed_stage and exception:
      detail = 'Oops!  The %s stage failed: %s' % (failed_stage, exception)
    else:
      detail = 'Oops!  The commit queue failed to verify your change.'

    self._SendNotification(
        change,
        '%(detail)s\n\nPlease check whether the failure is your fault: '
        '%(build_log)s . If your change is not at fault, you may mark it as '
        'ready again.', detail=detail)
    self._helper_pool.ForChange(change).RemoveCommitReady(
        change, dryrun=self.dryrun)

  def _HandleApplySuccess(self, change):
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
    return self.message + ('\n\nCommit queue documentation: %s' %
                           self._PALADIN_DOCUMENTATION_URL)

  def Send(self, dryrun):
    """Sends the message to the developer."""
    # Gerrit requires that commit messages are enclosed in quotes, and that
    # any backslashes or quotes within these quotes are escaped.
    # See com.google.gerrit.sshd.CommandFactoryProvider#split.
    message = '"%s"' % (self._ConstructPaladinMessage().
                        replace('\\','\\\\').replace('"', '\\"'))
    cmd = self.helper.GetGerritReviewCommand(
        ['-m', message,
         '%s,%s' % (self.patch.gerrit_number, self.patch.patch_number)])
    _RunCommand(cmd, dryrun)
