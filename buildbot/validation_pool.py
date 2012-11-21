# Copyright (c) 2011-2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module that handles interactions with a Validation Pool.

The validation pool is the set of commits that are ready to be validated i.e.
ready for the commit queue to try.
"""

import contextlib
import logging
import sys
import time
import urllib
from xml.dom import minidom

from chromite.buildbot import cbuildbot_results as results_lib
from chromite.buildbot import constants
from chromite.buildbot import lkgm_manager
from chromite.buildbot import portage_utilities
from chromite.lib import cros_build_lib
from chromite.lib import gerrit
from chromite.lib import git
from chromite.lib import patch as cros_patch

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
    cros_build_lib.RunCommand(cmd, error_code_ok=True)


class GerritHelperNotAvailable(gerrit.GerritException):
  """Exception thrown when a specific helper is requested but unavailable."""

  def __init__(self, remote=constants.EXTERNAL_REMOTE):
    gerrit.GerritException.__init__()
    # Stringify the pool so that serialization doesn't try serializing
    # the actual HelperPool.
    self.remote = remote
    self.args = (remote,)

  def __str__(self):
    return (
        "Needed a remote=%s gerrit_helper, but one isn't allowed by this"
        "HelperPool instance.") % (self.remote,)


class HelperPool(object):
  """Pool of allowed GerritHelpers to be used by CQ/PatchSeries."""

  def __init__(self, cros_internal=None, cros=None):
    """Initialize this instance with the given handlers.

    Most likely you want the classmethod SimpleCreate which takes boolean
    options.

    If a given handler is None, then it's disabled; else the passed in
    object is used.
    """
    self.pool = {
        constants.EXTERNAL_REMOTE : cros,
        constants.INTERNAL_REMOTE : cros_internal
    }

  @classmethod
  def SimpleCreate(cls, cros_internal=True, cros=True):
    """Classmethod helper for creating a HelperPool from boolean options.

    Args:
      internal: If True, allow access to a GerritHelper for internal.
      external: If True, allow access to a GerritHelper for external.
    Returns:
      An appropriately configured HelperPool instance.
    """
    if cros:
      cros = gerrit.GerritHelper.FromRemote(constants.EXTERNAL_REMOTE)
    else:
      cros = None

    if cros_internal:
      cros_internal = gerrit.GerritHelper.FromRemote(constants.INTERNAL_REMOTE)
    else:
      cros_internal = None

    return cls(cros_internal=cros_internal, cros=cros)

  def ForChange(self, change):
    """Return the helper to use for a particular change.

    If no helper is configured, an Exception is raised.
    """
    return self.GetHelper(change.remote)

  def GetHelper(self, remote):
    """Return the helper to use for a given remote.

    If no helper is configured, an Exception is raised.
    """
    helper = self.pool.get(remote)
    if not helper:
      raise GerritHelperNotAvailable(remote)

    return helper

  def __iter__(self):
    for helper in self.pool.itervalues():
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
    except gerrit.GerritException, e:
      if isinstance(e, gerrit.QueryNotSpecific):
        e = ("%s\nSuggest you use gerrit numbers instead (prefixed with a * "
             "if it's an internal change)." % e)
      new_exc = cros_patch.PatchException(parent, e)
      raise new_exc.__class__, new_exc, sys.exc_info()[2]
    except cros_patch.PatchException, e:
      if e.patch.id == parent.id:
        raise
      new_exc = cros_patch.DependencyError(parent, e)
      raise new_exc.__class__, new_exc, sys.exc_info()[2]

  f.__name__ = functor.__name__
  return f


class PatchSeries(object):
  """Class representing a set of patches applied to a single git repository."""

  def __init__(self, path, helper_pool=None, force_content_merging=False,
               forced_manifest=None, deps_filter_fn=None):

    self.manifest = forced_manifest
    self._content_merging_projects = {}
    self.force_content_merging = force_content_merging

    if helper_pool is None:
      helper_pool = HelperPool.SimpleCreate(cros_internal=True, cros=True)
    self._helper_pool = helper_pool
    self._path = path
    if deps_filter_fn is None:
      deps_filter_fn = lambda x:x
    self.deps_filter_fn = deps_filter_fn

    self.applied = []
    self.failed = []
    self.failed_tot = {}

    # A mapping of ChangeId to exceptions if the patch failed against
    # ToT.  Primarily used to keep the resolution/applying from going
    # down known bad paths.
    self._committed_cache = cros_patch.PatchCache()
    self._lookup_cache = cros_patch.PatchCache()
    self._change_deps_cache = {}

  def GetTrackingBranchForChange(self, change, for_gerrit=False):
    """Identify the branch to work against for this change.

    Args:
      gerrit: If True, give the shortened form; no refs/heads, no refs/remotes.
    """
    ref = self.manifest.GetProjectsLocalRevision(change.project)
    return git.StripRefs(ref) if for_gerrit else ref

  def GetGitRepoForChange(self, change):
    return self.manifest.GetProjectPath(change.project, True)

  def _IsContentMerging(self, change):
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
    if self.force_content_merging:
      return True
    helper = self._helper_pool.ForChange(change)

    if not helper.version.startswith('2.1'):
      return self.manifest.ProjectIsContentMerging(change.project)

    # Fallback to doing gsql trickery to get it; note this requires admin
    # access.  This isn't required for CrOS anymore, but is left in place
    # should a thirdparty not yet be on >=2.2
    projects = self._content_merging_projects.get(helper)
    if projects is None:
      projects = helper.FindContentMergingProjects()
      self._content_merging_projects[helper] = projects

    return change.project in projects

  def ApplyChange(self, change, dryrun=False):
    # If we're in dryrun mode, then 3way is always allowed.
    # Otherwise, allow 3way only if the gerrit project allows it.
    trivial = False if dryrun else not self._IsContentMerging(change)
    return change.ApplyAgainstManifest(self.manifest, trivial=trivial)

  def _GetGerritPatch(self, change, query, parent_lookup=False):
    """Query the configured helpers looking for a given change.

    Args:
      change: A cros_patch.GitRepoPatch derivative that we're querying
        on behalf of.
      query: The ChangeId we're searching for.
      parent_lookup: If True, this means we're tracing out the git parents
        of the given change- as such limit the query purely to that
        project/branch.
    """
    remote = constants.EXTERNAL_REMOTE
    if query.startswith('*'):
      remote = constants.INTERNAL_REMOTE
    helper = self._helper_pool.GetHelper(remote)

    query = query_text = cros_patch.FormatPatchDep(query, force_external=True)
    if parent_lookup:
      query_text = "project:%s AND branch:%s AND %s" % (
          change.project,
          self.GetTrackingBranchForChange(change, True),
          query_text)
    change = helper.QuerySingleRecord(query_text, must_match=True)
    # If the query was a gerrit number based query, check the projects/change-id
    # to see if we already have it locally, but couldn't map it since we didn't
    # know the gerrit number at the time of the initial injection.
    existing = self._lookup_cache[
        cros_patch.FormatChangeId(
            change.change_id, force_internal=change.internal, strict=False)]
    if query.isdigit() and existing is not None:
      if ((existing.project == change.project
          and existing.tracking_branch == change.tracking_branch)
          or not parent_lookup):
        key = cros_patch.FormatGerritNumber(
            str(change.gerrit_number), force_internal=change.internal,
            strict=False)
        self._lookup_cache.InjectCustomKey(key, existing)
        return existing

    self.InjectLookupCache([change])
    if change.IsAlreadyMerged():
      self.InjectCommittedPatches([change])
    return change

  @_PatchWrapException
  def _LookupUncommittedChanges(self, parent, deps, parent_lookup=False,
                                limit_to=None):
    """Given a set of deps (changes), return unsatisfied dependencies.

    Args:
      parent: The change we're resolving for.
      deps: A sequence of dependencies for the parent that we need to identify
        as either merged, or needing resolving.
      parent_lookup: If True, this means we're trying to trace out the git
        parentage of a change, thus limit the lookup to the parents project
        and branch.
      limit_to: If non-None, then this must be a mapping (preferably a
        cros_patch.PatchCache for translation reasons) of which non-committed
        changes are allowed to be used for a transaction.
    Returns:
      A sequence of cros_patch.GitRepoPatch instances (or derivatives) that
      need to be resolved for this change to be mergable.
    """
    unsatisfied = []
    for dep in deps:
      if dep in self._committed_cache:
        continue

      dep_change = self._lookup_cache[dep]
      if parent_lookup and dep_change is not None:
        if not (parent.project == dep_change.project and
                self.GetTrackingBranchForChange(parent, True) ==
                self.GetTrackingBranchForChange(dep_change, True)):
          # TODO(build): In this scenario, the cache will get updated
          # with the new CL pulled from gerrit; this is questionable,
          # but there isn't a good answer here.  Rare enough it's being
          # ignored either way.
          dep_change = None

      if dep_change is None:
        dep_change = self._GetGerritPatch(parent, dep,
                                          parent_lookup=parent_lookup)

      if getattr(dep_change, 'IsAlreadyMerged', lambda: False)():
        continue
      elif limit_to is not None and dep_change not in limit_to:
        raise DependencyNotReadyForCommit(parent, dep)

      unsatisfied.append(dep_change)

    # Perform last minute custom filtering.
    return [x for x in unsatisfied if self.deps_filter_fn(x)]

  def CreateTransaction(self, change, limit_to=None):
    """Given a change, resolve it into a transaction.

    In this case, a transaction is defined as a group of commits that
    must land for the given change to be merged- specifically its
    parent deps, and its CQ-DEPENDS.

    Args:
      change: A cros_patch.GitRepoPatch instance to generate a transaction
        for.
      limit_to: If non-None, limit the allowed uncommitted patches to
        what's in that container/mapping.
    Returns:
      A sequency of the necessary cros_patch.GitRepoPatch objects for
      this transaction.
    """
    plan, stack = [], cros_patch.PatchCache()
    self._ResolveChange(change, plan, stack, limit_to=limit_to)
    return plan

  def _ResolveChange(self, change, plan, stack, limit_to=None):
    """Helper for resolving a node and its dependencies into the plan.

    No external code should call this; all internal code should invoke this
    rather than ResolveTransaction since this maintains the necessary stack
    tracking that is used to detect and handle cyclic dependencies.

    Raises:
      If the change couldn't be resolved, a DependencyError or
      cros_patch.PatchException can be raised.
    """
    if change in self._committed_cache:
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
    stack.Inject(change)
    try:
      self._PerformResolveChange(change, plan, stack, limit_to=limit_to)
    finally:
      stack.Remove(change)

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
      git_repo = self.GetGitRepoForChange(change)
      val = self._change_deps_cache[change] = (
          change.GerritDependencies(
              git_repo, self.GetTrackingBranchForChange(change)),
          change.PaladinDependencies(git_repo))
    return val

  def _PerformResolveChange(self, change, plan, stack, limit_to=None):
    """Resolve and ultimately add a change into the plan."""
    # Pull all deps up front, then process them.  Simplifies flow, and
    # localizes the error closer to the cause.
    gdeps, pdeps = self._GetDepsForChange(change)
    gdeps = self._LookupUncommittedChanges(change, gdeps, limit_to=limit_to,
                                           parent_lookup=True)
    pdeps = self._LookupUncommittedChanges(change, pdeps, limit_to=limit_to)

    def _ProcessDeps(deps):
      for dep in deps:
        if dep in plan:
          continue
        try:
          self._ResolveChange(dep, plan, stack, limit_to=limit_to)
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
    self._committed_cache.Inject(*changes)

  def InjectLookupCache(self, changes):
    """Inject into the internal lookup cache the given changes, using them
    (rather than asking gerrit for them) as needed for dependencies.
    """
    self._lookup_cache.Inject(*changes)

  def FetchChanges(self, changes):
    for change in changes:
      change.Fetch(self.GetGitRepoForChange(change))

  def _ApplyDecorator(functor):
    """Decorator for Apply that does appropriate self.manifest manipulation.

    Note this is implemented in this fashion so that we can be sure the
    instances manifest attribute is properly maintained, and so that we
    don't have to tell people "go look at docstring blah".
    """
    # pylint: disable=E0213,W0212,E1101,E1102
    def f(self, changes, **kwargs):
      manifest = kwargs.pop('manifest', None)
      # Wipe is used to track if we need to reset manifest to None, and
      # to identify if we already had a forced_manifest via __init__.
      wipe = self.manifest is None
      if manifest:
        if not wipe:
          raise ValueError("manifest can't be specified when one is forced "
                           "via __init__")
      elif wipe:
        manifest = git.ManifestCheckout.Cached(self._path)
      else:
        manifest = self.manifest

      try:
        self.manifest = manifest
        return functor(self, changes, **kwargs)
      finally:
        if wipe:
          self.manifest = None
    f.__name__ = functor.__name__
    f.__doc__ = functor.__doc__
    return f

  @_ApplyDecorator
  def Apply(self, changes, dryrun=False, frozen=True,
            honor_ordering=False, changes_filter=None):
    """Applies changes from pool into the build root specified by the manifest.

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
      changes_filter: If not None, must be a functor taking two arguments:
        series, changes; it must return the changes to work on.
        This is invoked after the initial changes have been fetched,
        thus this is a way for consumers to do last minute checking of the
        changes being inspected, and expand the changes if necessary.
        Primarily this is of use for cbuildbot patching when dealing w/
        uploaded/remote patches.
    Returns:
      A tuple of changes-applied, Exceptions for the changes that failed
      against ToT, and Exceptions that failed inflight;  These exceptions
      are cros_patch.PatchException instances.
    """

    # Prefetch the changes; we need accurate change_id/id's, which is
    # guaranteed via Fetch.
    self.FetchChanges(changes)
    if changes_filter:
      changes = changes_filter(self, changes)

    self.InjectLookupCache(changes)
    allowed_changes = cros_patch.PatchCache(changes) if frozen else None
    resolved, applied, failed = [], [], []
    for change in changes:
      try:
        resolved.append(
            (change, self.CreateTransaction(change, limit_to=allowed_changes)))
      except cros_patch.PatchException, e:
        logging.info("Failed creating transaction for %s: %s", change, e)
        failed.append(e)
      else:
        logging.info("Transaction for %s is %s.",
            change, ', '.join(map(str, resolved[-1][-1])))

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
    """ContextManager used to rollback changes to a build root if necessary.

    Specifically, if an unhandled non system exception occurs, this context
    manager will roll back all relevant modifications to the git repos
    involved.

    Args:
      commits: A sequence of cros_patch.GitRepoPatch instances that compromise
        this transaction- this is used to identify exactly what may be changed,
        thus what needs to be tracked and rolled back if the transaction fails.
    """
    # First, the book keeping code; gather required data so we know what
    # to rollback to should this transaction fail.  Specifically, we track
    # what was checked out for each involved repo, and if it was a branch,
    # the sha1 of the branch; that information is enough to rewind us back
    # to the original repo state.
    project_state = set(map(self.GetGitRepoForChange, commits))
    resets, checkouts = [], []
    for project_dir in project_state:
      current_sha1 = git.RunGit(
          project_dir, ['rev-list', '-n1', 'HEAD']).output.strip()
      assert current_sha1

      result = git.RunGit(
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
        git.RunGit(project_dir, ['checkout', ref])

      for project_dir, sha1 in resets:
        git.RunGit(project_dir, ['reset', '--hard', sha1])

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
      if change in self._committed_cache:
        continue

      try:
        self.ApplyChange(change, dryrun=dryrun)
      except cros_patch.PatchException, e:
        if not e.inflight:
          self.failed_tot[change.id] = e
        raise
      applied.append(change)
      if hasattr(change, 'url'):
        s = '%s %s' % (change.owner, cros_patch.FormatGerritNumber(
            change.gerrit_number, force_internal=change.internal))
        cros_build_lib.PrintBuildbotLink(s, change.url)

    logging.debug('Done investigating changes.  Applied %s',
                  ' '.join([c.id for c in applied]))

  @classmethod
  def WorkOnSingleRepo(cls, git_repo, tracking_branch, **kwargs):
    """Classmethod to generate a PatchSeries that targets a single git repo.

    It does this via forcing a fake manifest, which in turn points
    tracking branch/paths/content-merging at what is passed through here.

    Args:
      git_repo: Absolute path to the git repository to operate upon.
      tracking_branch: Which tracking branch patches should apply against.
      kwargs: See PatchSeries.__init__ for the various optional args;
        not forced_manifest cannot be used here, and force_content_merging
        defaults to True in this usage.
    Returns:
      A PatchSeries instance w/ a forced manifest."""

    if 'forced_manifest' in kwargs:
      raise ValueError("RawPatchSeries doesn't allow a forced_manifest "
                       "argument.")
    merging = kwargs.setdefault('force_content_merging', True)
    kwargs['forced_manifest'] = _ManifestShim(
        git_repo, tracking_branch, content_merging=merging)

    return cls(git_repo, **kwargs)


class _ManifestShim(object):
  """Class used in conjunction with PatchSeries to support standalone git repos.

  This works via duck typing; we match the 3 necessary methods that PatchSeries
  uses."""

  def __init__(self, path, tracking_branch, remote='origin',
               content_merging=True):

    self.path = path
    self.tracking_branch = 'refs/remotes/%s/%s' % (remote, tracking_branch)
    self.content_merging = content_merging

  def GetProjectsLocalRevision(self, _project):
    return self.tracking_branch

  def GetProjectPath(self, _project, _absolute=False):
    return self.path

  def ProjectIsContentMerging(self, _project):
    return self.content_merging


class ValidationFailedMessage(object):
  """Message indicating that changes failed to be validated."""

  def __init__(self, builder_name, build_log, tracebacks, internal):
    """Create a ValidationFailedMessage object.

    Args:
      builder_name: The URL-quoted name of the builder.
      build_log: The URL users should visit to see the build log.
      tracebacks: A list of results_lib.RecordedTraceback objects.
      internal: Whether this failure occurred on an internal builder.
    """
    self.builder_name = builder_name
    self.build_log = build_log
    self.tracebacks = tuple(tracebacks)
    self.internal = internal

  def __str__(self):
    details = []
    for x in self.tracebacks:
      details.append('The %s stage failed: %s' % (x.failed_stage, x.exception))
    if not details:
      details = ['cbuildbot failed']
    details.append('in %s' % (self.build_log,))
    return '%s: %s' % (urllib.unquote(self.builder_name), ' '.join(details))


class ValidationPool(object):
  """Class that handles interactions with a validation pool.

  This class can be used to acquire a set of commits that form a pool of
  commits ready to be validated and committed.

  Usage:  Use ValidationPool.AcquirePool -- a static
  method that grabs the commits that are ready for validation.
  """

  GLOBAL_DRYRUN = False
  MAX_TIMEOUT = 60 * 60 * 4
  SLEEP_TIMEOUT = 30
  STATUS_URL = 'https://chromiumos-status.appspot.com/current?format=json'

  # The grace period (in seconds) before we reject a patch due to dependency
  # errors.
  REJECTION_GRACE_PERIOD = 30 * 60

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
        instance is created with full access to external and internal gerrit
        instances; full access is used to allow cross gerrit dependencies
        to be supported.
    """

    if helper_pool is None:
      helper_pool = HelperPool.SimpleCreate()

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
    cros_internal = cros = False
    if overlays in [constants.PUBLIC_OVERLAYS, constants.BOTH_OVERLAYS, False]:
      cros = True

    if overlays in [constants.PRIVATE_OVERLAYS, constants.BOTH_OVERLAYS]:
      cros_internal = True

    return HelperPool.SimpleCreate(cros_internal=cros_internal, cros=cros)

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
  def AcquirePool(cls, overlays, build_root, build_number, builder_name,
                  dryrun=False, changes_query=None):
    """Acquires the current pool from Gerrit.

    Polls Gerrit and checks for which change's are ready to be committed.

    Args:
      overlays:  One of constants.VALID_OVERLAYS.
      build_root: The location of the build root used to filter projects, and
        to apply patches against.
      build_number: Corresponding build number for the build.
      builder_name:  Builder name on buildbot dashboard.
      dryrun: Don't submit anything to gerrit.
      changes_query: The gerrit query to use to identify changes; if None,
        uses the internal defaults.
    Returns:
      ValidationPool object.
    Raises:
      TreeIsClosedException: if the tree is closed.
    """

    if changes_query is None:
      changes_query = constants.DEFAULT_CQ_READY_QUERY

    # We choose a longer wait here as we haven't committed to anything yet. By
    # doing this here we can reduce the number of builder cycles.
    end_time = time.time() + cls.MAX_TIMEOUT
    while True:
      time_left = end_time - time.time()
      if not dryrun and not cros_build_lib.TreeOpen(
          cls.STATUS_URL, cls.SLEEP_TIMEOUT, max_timeout=time_left):
        raise TreeIsClosedException()

      # Only master configurations should call this method.
      pool = ValidationPool(overlays, build_root, build_number, builder_name,
                            True, dryrun)
      # Iterate through changes from all gerrit instances we care about.
      for helper in cls.GetGerritHelpersForOverlays(overlays):
        raw_changes = helper.Query(changes_query, sort='lastUpdated')
        raw_changes.reverse()

        changes, non_manifest_changes = ValidationPool._FilterNonCrosProjects(
            raw_changes, git.ManifestCheckout.Cached(build_root))
        pool.changes.extend(changes)
        pool.non_manifest_changes.extend(non_manifest_changes)

      if pool.changes or pool.non_manifest_changes or dryrun or time_left < 0:
        break

      logging.info('Waiting for new CLs (%d minutes left)...', time_left / 60)
      time.sleep(cls.SLEEP_TIMEOUT)

    return pool

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
        except gerrit.QueryHasNoResults:
          pass
      else:
        raise NoMatchingChangeFoundException(
            'Could not find change defined by %s' % pending_commit)

    return pool

  @staticmethod
  def _FilterNonCrosProjects(changes, manifest):
    """Filters changes to a tuple of relevant changes.

    There are many code reviews that are not part of Chromium OS and/or
    only relevant on a different branch. This method returns a tuple of (
    relevant reviews in a manifest, relevant reviews not in the manifest). Note
    that this function must be run while chromite is checked out in a
    repo-managed checkout.

    Args:
      changes:  List of GerritPatch objects.
      manifest: The manifest to check projects/branches against.

    Returns tuple of
      relevant reviews in a manifest, relevant reviews not in the manifest.
    """

    def IsCrosReview(change):
      return (change.project.startswith('chromiumos') or
              change.project.startswith('chromeos'))

    # First we filter to only Chromium OS repositories.
    changes = [c for c in changes if IsCrosReview(c)]

    projects = manifest.projects

    changes_in_manifest = []
    changes_not_in_manifest = []
    for change in changes:
      patch_branch = 'refs/heads/%s' % change.tracking_branch
      project_data = projects.get(change.project)
      if project_data is not None and patch_branch == project_data['revision']:
        changes_in_manifest.append(change)
        continue

      changes_not_in_manifest.append(change)
      logging.info('Filtered change %s', change)

    return changes_in_manifest, changes_not_in_manifest

  @classmethod
  def _FilterDependencyErrors(cls, errors):
    """Filter out ignorable DependencyError exceptions.

    If a dependency isn't marked as ready, or a dependency fails to apply,
    we only complain after REJECTION_GRACE_PERIOD has passed since the patch
    was uploaded.

    This helps in two situations:
      1) If the developer is in the middle of marking a stack of changes as
         ready, we won't reject their work until the grace period has passed.
      2) If the developer marks a big circular stack of changes as ready, and
         some change in the middle of the stack doesn't apply, the user will
         get a chance to rebase their change before we mark all the changes as
         'not ready'.

    This function filters out dependency errors that can be ignored due to
    the grace period.

    Args:
      errors: List of exceptions to filter.

    Returns:
      List of unfiltered exceptions.
    """
    reject_timestamp = time.time() - cls.REJECTION_GRACE_PERIOD
    results = []
    for error in errors:
      results.append(error)
      if reject_timestamp < error.patch.approval_timestamp:
        while error is not None:
          if isinstance(error, cros_patch.DependencyError):
            logging.info('Ignoring dependency errors for %s due to grace '
                         'period', error.patch)
            results.pop()
            break
          error = getattr(error, 'error', None)
    return results

  def ApplyPoolIntoRepo(self, manifest=None):
    """Applies changes from pool into the directory specified by the buildroot.

    This method applies changes in the order specified.  It also respects
    dependency order.
    Returns:

    True if we managed to apply any changes.
    """
    try:
      # pylint: disable=E1123
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

    failed_tot = self._FilterDependencyErrors(failed_tot)
    if failed_tot:
      logging.info(
          'The following changes could not cleanly be applied to ToT: %s',
          ' '.join([c.patch.id for c in failed_tot]))
      self._HandleApplyFailure(failed_tot)

    failed_inflight = self._FilterDependencyErrors(failed_inflight)
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
    if not self.dryrun and not cros_build_lib.TreeOpen(
        self.STATUS_URL, self.SLEEP_TIMEOUT):
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

  @staticmethod
  def _FindSuspects(changes, messages):
    """Figure out what changes probably caused our failures.

    We use a fairly simplistic algorithm to calculate breakage: If you changed
    a package, and that package broke, you probably broke the build. If there
    were multiple changes to a broken package, we fail them all.

    Some safeguards are implemented to ensure that bad changes are kicked out:
      1) Changes to overlays (e.g. ebuilds, eclasses, etc.) are always kicked
         out if the build fails.
      2) If a package fails that nobody changed, we kick out all of the
         changes.
      3) If any failures occur that we can't explain, we kick out all of the
         changes.

    It is certainly possible to trick this algorithm: If one developer submits
    a change to libchromeos that breaks the power_manager, and another developer
    submits a change to the power_manager at the same time, only the
    power_manager change will be kicked out. That said, in that situation, the
    libchromeos change will likely be kicked out on the next run, thanks to
    safeguard #2 above.

    This function is intentionally static, and should be kept simple. If it
    starts getting complicated, we should move it to a different file.

    Args:
      changes: List of changes to examine.
      messages: A list of build failure messages from supporting builders.
    Returns:
      suspects: Set of changes that likely caused the failure.
    """
    suspects = set()
    blame_everything = False

    # If there were no internal failures, only kick out external changes.
    if any(message.internal for message in messages):
      candidates = changes
    else:
      candidates = [change for change in changes if not change.internal]

    for message in messages:
      for recorded_traceback in message.tracebacks:
        exception = recorded_traceback.exception
        blame_assigned = False
        if isinstance(exception, results_lib.PackageBuildFailure):
          for package in exception.failed_packages:
            failed_projects = portage_utilities.FindWorkonProjects([package])
            for change in candidates:
              if change.project in failed_projects:
                blame_assigned = True
                suspects.add(change)
        if not blame_assigned:
          blame_everything = True

    if blame_everything or not suspects:
      suspects = set(candidates)
    else:
      # Never treat changes to overlays as innocent.
      suspects.update(change for change in candidates
                      if '/overlays/' in change.project)

    return suspects

  @staticmethod
  def _CreateValidationFailureMessage(change, suspects, messages):
    """Create a message explaining why a validation failure occurred.

    Args:
      change: The change we want to create a message for.
      suspects: The set of suspect changes that we think broke the build.
      messages: A list of build failure messages from supporting builders.
    """
    msg = ['The following build(s) failed:'] + map(str, messages)

    # Create a list of changes other than this one that might be guilty.
    other_suspects = suspects - set([change])
    other_suspects_str = ', '.join(sorted(
        cros_patch.FormatChangeId(x.change_id, force_internal=x.internal)
                                  for x in other_suspects))

    if change in suspects:
      if other_suspects_str:
        msg.append('Your change may have caused this failure. There are '
                   'also other changes that may be at fault: %s'
                   % other_suspects_str)
      else:
        msg.append('This failure was probably caused by your change.')

      msg.append('Please check whether the failure is your fault. If your '
                 'change is not at fault, you may mark it as ready again.')
    else:
      if len(suspects) == 1:
        msg.append('This failure was probably caused by %s'
                   % other_suspects_str)
      else:
        msg.append('One of the following changes is probably at fault: %s'
                   % other_suspects_str)
      msg.insert(
          0, 'NOTE: The Commit Queue will retry your change automatically.')

    return '\n\n'.join(msg)

  def HandleValidationFailure(self, messages):
    """Handles a list of validation failure messages from slave builders.

    This handler parses a list of failure messages from our list of builders
    and calculates which changes were likely responsible for the failure. The
    changes that were responsible for the failure have their Commit Ready bit
    stripped and the other changes are left marked as Commit Ready.

    Args:
      messages: A list of build failure messages from supporting builders.
          These must be ValidationFailedMessage objects.
    """

    # First, calculate which changes are likely at fault for the failure.
    suspects = self._FindSuspects(self.changes, messages)

    # Send out failure notifications for each change.
    for change in self.changes:
      msg = self._CreateValidationFailureMessage(change, suspects, messages)
      self._SendNotification(change, '%(details)s', details=msg)
      if change in suspects:
        self._helper_pool.ForChange(change).RemoveCommitReady(
            change, dryrun=self.dryrun)

  def GetValidationFailedMessage(self):
    """Returns message indicating these changes failed to be validated."""
    logging.info('Validation failed for all changes.')
    internal = self._overlays in [constants.PRIVATE_OVERLAYS,
                                  constants.BOTH_OVERLAYS]
    return ValidationFailedMessage(self._builder_name, self.build_log,
                                   results_lib.Results.GetTracebacks(),
                                   internal)

  def HandleCouldNotApply(self, change):
    """Handler for when Paladin fails to apply a change.

    This handler strips the Commit Ready bit forcing the developer
    to re-upload a rebased change as this theirs failed to apply cleanly.

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
                        replace('\\', '\\\\').replace('"', '\\"'))
    cmd = self.helper.GetGerritReviewCommand(
        ['-m', message,
         '%s,%s' % (self.patch.gerrit_number, self.patch.patch_number)])
    _RunCommand(cmd, dryrun)
