# Copyright (c) 2011-2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module that handles interactions with a Validation Pool.

The validation pool is the set of commits that are ready to be validated i.e.
ready for the commit queue to try.
"""

import ConfigParser
import contextlib
import cPickle
import functools
import httplib
import logging
import os
import sys
import time
import urllib
from xml.dom import minidom

from chromite.buildbot import cbuildbot_results as results_lib
from chromite.buildbot import constants
from chromite.buildbot import lkgm_manager
from chromite.buildbot import manifest_version
from chromite.buildbot import portage_utilities
from chromite.lib import cros_build_lib
from chromite.lib import gerrit
from chromite.lib import git
from chromite.lib import gob_util
from chromite.lib import gs
from chromite.lib import parallel
from chromite.lib import patch as cros_patch
from chromite.lib import timeout_util

# Third-party libraries bundled with chromite need to be listed after the
# first chromite import.
import digraph

# We import mox so that w/in ApplyPoolIntoRepo, if a mox exception is
# thrown, we don't cover it up.
try:
  import mox
except ImportError:
  mox = None


PRE_CQ = 'pre-cq'
CQ = 'cq'

# The gerrit-on-borg team tells us that delays up to 2 minutes can be
# normal.  Setting timeout to 3 minutes to be safe-ish.
SUBMITTED_WAIT_TIMEOUT = 3 * 60 # Time in seconds.

class TreeIsClosedException(Exception):
  """Raised when the tree is closed and we wanted to submit changes."""

  def __init__(self, closed_or_throttled=False):
    """Initialization.

    Args:
      closed_or_throttled: True if the exception is being thrown on a
                           possibly 'throttled' tree. False if only
                           thrown on a 'closed' tree. Default: False
    """
    if closed_or_throttled:
      status_text = 'closed or throttled'
      opposite_status_text = 'open'
    else:
      status_text = 'closed'
      opposite_status_text = 'throttled or open'

    super(TreeIsClosedException, self).__init__(
        'Tree is %s.  Please set tree status to %s to '
        'proceed.' % (status_text, opposite_status_text))


class FailedToSubmitAllChangesException(results_lib.StepFailure):
  """Raised if we fail to submit any changes."""

  def __init__(self, changes):
    super(FailedToSubmitAllChangesException, self).__init__(
        'FAILED TO SUBMIT ALL CHANGES:  Could not verify that changes %s were '
        'submitted' % ' '.join(str(c) for c in changes))


class InternalCQError(cros_patch.PatchException):
  """Exception thrown when CQ has an unexpected/unhandled error."""

  def __init__(self, patch, message):
    cros_patch.PatchException.__init__(self, patch, message=message)

  def ShortExplanation(self):
    return 'failed to apply due to a CQ issue: %s' % (self.message,)


class NoMatchingChangeFoundException(Exception):
  """Raised if we try to apply a non-existent change."""


class ChangeNotInManifestException(Exception):
  """Raised if we try to apply a not-in-manifest change."""


class PatchNotCommitReady(cros_patch.PatchException):
  """Raised if a patch is not marked as commit ready."""

  def ShortExplanation(self):
    return 'isn\'t marked as Commit-Ready anymore.'


class PatchNotPublished(cros_patch.PatchException):
  """Raised if a patch is not published."""

  def ShortExplanation(self):
    return 'has not been published.'


class PatchRejected(cros_patch.PatchException):
  """Raised if a patch was rejected by the CQ because the CQ failed."""

  def ShortExplanation(self):
    return 'was rejected by the CQ.'


class PatchFailedToSubmit(cros_patch.PatchException):
  """Raised if we fail to submit a change."""

  def ShortExplanation(self):
    error = 'could not be submitted by the CQ.'
    if self.message:
      error += ' The error message from Gerrit was: %s' % (self.message,)
    else:
      error += ' The Gerrit server might be having trouble.'
    return error


class PatchConflict(cros_patch.PatchException):
  """Raised if a patch needs to be rebased."""

  def ShortExplanation(self):
    return ('no longer applies cleanly to tip of tree. Please rebase '
            'and re-upload your patch.')


class PatchSubmittedWithoutDeps(cros_patch.DependencyError):
  """Exception thrown when a patch was submitted incorrectly."""

  def ShortExplanation(self):
    dep_error = cros_patch.DependencyError.ShortExplanation(self)
    return ('was submitted, even though it %s\n'
            '\n'
            'You may want to revert your patch, and investigate why its'
            'dependencies failed to submit.\n'
            '\n'
            'This error only occurs when we have a dependency cycle, and we '
            'submit one change before realizing that a later change cannot '
            'be submitted.' % (dep_error,))


class PatchSeriesTooLong(cros_patch.PatchException):
  """Exception thrown when a required dep isn't satisfied."""

  def __init__(self, patch, max_length):
    cros_patch.PatchException.__init__(self, patch)
    self.max_length = max_length

  def ShortExplanation(self):
    return ("The Pre-CQ cannot handle a patch series longer than %s patches. "
            "Please wait for some patches to be submitted before marking more "
            "patches as ready. "  % (self.max_length,))

  def __str__(self):
    return self.ShortExplanation()


def _RunCommand(cmd, dryrun):
  """Runs the specified shell cmd if dryrun=False.

  Errors are ignored, but logged.
  """
  if dryrun:
    logging.info('Would have run: %s', ' '.join(cmd))
    return

  try:
    cros_build_lib.RunCommand(cmd)
  except cros_build_lib.RunCommandError:
    cros_build_lib.Error('Command failed', exc_info=True)


def GetStagesToIgnoreFromConfigFile(config_path):
  """Get a list of stage name prefixes to ignore from |config_path|.

  This function reads the specified config file and returns the list
  of stage name prefixes to ignore in the CQ. See GetStagesToIgnoreForChange
  for more details.

  Args:
    config_path: The path to the config file to read.
  """
  ignored_stages = []
  parser = ConfigParser.SafeConfigParser()
  try:
    parser.read(config_path)
    if parser.has_option('GENERAL', 'ignored-stages'):
      ignored_stages = parser.get('GENERAL', 'ignored-stages').split()
  except ConfigParser.Error:
    cros_build_lib.Error('Error parsing %r', config_path, exc_info=True)

  return ignored_stages


def GetStagesToIgnoreForChange(build_root, change):
  """Get a list of stages that the CQ should ignore for a given |change|.

  The list of stage name prefixes to ignore for each project is specified in a
  config file inside the project, named COMMIT-QUEUE.ini. The file would look
  like this:

  [GENERAL]
    ignored-stages: HWTest VMTest

  The CQ will submit changes to the given project even if the listed stages
  failed. These strings are stage name prefixes, meaning that "HWTest" would
  match any HWTest stage (e.g. "HWTest [bvt]" or "HWTest [foo]")

  Args:
    build_root: The root of the checkout.
    change: Change to examine.

  Returns:
    A list of stages to ignore for the given |change|.
  """
  manifest = git.ManifestCheckout.Cached(build_root)
  checkout = change.GetCheckout(manifest)
  if checkout:
    dirname = checkout.GetPath(absolute=True)
    path = os.path.join(dirname, 'COMMIT-QUEUE.ini')
    return GetStagesToIgnoreFromConfigFile(path)

  return []

class GerritHelperNotAvailable(gerrit.GerritException):
  """Exception thrown when a specific helper is requested but unavailable."""

  def __init__(self, remote=constants.EXTERNAL_REMOTE):
    gerrit.GerritException.__init__(self)
    # Stringify the pool so that serialization doesn't try serializing
    # the actual HelperPool.
    self.remote = remote
    self.args = (remote,)

  def __str__(self):
    return (
        "Needed a remote=%s gerrit_helper, but one isn't allowed by this "
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
      cros_internal: If True, allow access to a GerritHelper for internal.
      cros: If True, allow access to a GerritHelper for external.

    Returns:
      An appropriately configured HelperPool instance.
    """
    if cros:
      cros = gerrit.GetGerritHelper(constants.EXTERNAL_REMOTE)
    else:
      cros = None

    if cros_internal:
      cros_internal = gerrit.GetGerritHelper(constants.INTERNAL_REMOTE)
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
  be chained in addition).
  """
  def f(self, parent, *args, **kwargs):
    try:
      return functor(self, parent, *args, **kwargs)
    except gerrit.GerritException as e:
      if isinstance(e, gerrit.QueryNotSpecific):
        e = ("%s\nSuggest you use gerrit numbers instead (prefixed with a * "
             "if it's an internal change)." % e)
      new_exc = cros_patch.PatchException(parent, e)
      raise new_exc.__class__, new_exc, sys.exc_info()[2]
    except cros_patch.PatchException as e:
      if e.patch.id == parent.id:
        raise
      new_exc = cros_patch.DependencyError(parent, e)
      raise new_exc.__class__, new_exc, sys.exc_info()[2]

  f.__name__ = functor.__name__
  return f


class PatchSeries(object):
  """Class representing a set of patches applied to a single git repository."""

  def __init__(self, path, helper_pool=None, force_content_merging=False,
               forced_manifest=None, deps_filter_fn=None, is_submitting=False):
    """Constructor.

    Args:
      path: Path to the buildroot.
      helper_pool: Pool of allowed GerritHelpers to be used for fetching
        patches. Defaults to allowing both internal and external fetches.
      force_content_merging: Allow merging of trivial conflicts, even if they
        are disabled by Gerrit.
      forced_manifest: A manifest object to use for mapping projects to
        repositories. Defaults to the buildroot.
      deps_filter_fn: A function which specifies what patches you would
        like to accept. It is passed a patch and is expected to return
        True or False.
      is_submitting: Whether we are currently submitting patchsets. This is
        used to print better error messages.
    """
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
    self._is_submitting = is_submitting

    self.applied = []
    self.failed = []
    self.failed_tot = {}

    # A mapping of ChangeId to exceptions if the patch failed against
    # ToT.  Primarily used to keep the resolution/applying from going
    # down known bad paths.
    self._committed_cache = cros_patch.PatchCache()
    self._lookup_cache = cros_patch.PatchCache()
    self._change_deps_cache = {}

  def _ManifestDecorator(functor):
    """Method decorator that sets self.manifest automatically.

    This function automatically initializes the manifest, and allows callers to
    override the manifest if needed.
    """
    # pylint: disable=E0213,W0212,E1101,E1102
    def f(self, *args, **kwargs):
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
        return functor(self, *args, **kwargs)
      finally:
        if wipe:
          self.manifest = None

    f.__name__ = functor.__name__
    f.__doc__ = functor.__doc__
    return f

  @_ManifestDecorator
  def GetGitRepoForChange(self, change, strict=False):
    """Get the project path associated with the specified change.

    Args:
      change: The change to operate on.
      strict: If True, throw ChangeNotInManifest rather than returning
        None. Default: False.

    Returns:
      The project path if found in the manifest. Otherwise returns
      None (if strict=False).
    """
    project_dir = None
    if self.manifest:
      checkout = change.GetCheckout(self.manifest, strict=strict)
      if checkout is not None:
        project_dir = checkout.GetPath(absolute=True)

    return project_dir

  @_ManifestDecorator
  def _IsContentMerging(self, change):
    """Discern if the given change has Content Merging enabled in gerrit.

    Note if the instance was created w/ force_content_merging=True,
    then this function will lie and always return True to avoid the
    admin-level access required of <=gerrit-2.1.

    Returns:
      True if the change's project has content merging enabled, False if not.

    Raises:
      AssertionError: If the gerrit helper requested is disallowed.
      GerritException: If there is a failure in querying gerrit.
    """
    if self.force_content_merging:
      return True
    return self.manifest.ProjectIsContentMerging(change.project)

  @_ManifestDecorator
  def ApplyChange(self, change, dryrun=False):
    # If we're in dryrun mode, then 3way is always allowed.
    # Otherwise, allow 3way only if the gerrit project allows it.
    trivial = False if dryrun else not self._IsContentMerging(change)
    return change.ApplyAgainstManifest(self.manifest, trivial=trivial)

  def _LookupHelper(self, patch):
    """Returns the helper for the given cros_patch.PatchQuery object."""
    return self._helper_pool.GetHelper(patch.remote)

  def _GetGerritPatch(self, query):
    """Query the configured helpers looking for a given change.

    Args:
      project: The gerrit project to query.
      query: A cros_patch.PatchQuery object.

    Returns:
      A GerritPatch object.
    """
    helper = self._LookupHelper(query)
    query_text = query.ToGerritQueryText()
    change = helper.QuerySingleRecord(
        query_text, must_match=not git.IsSHA1(query_text))

    if not change:
      return

    # If the query was a gerrit number based query, check the projects/change-id
    # to see if we already have it locally, but couldn't map it since we didn't
    # know the gerrit number at the time of the initial injection.
    existing = self._lookup_cache[change]
    if cros_patch.ParseGerritNumber(query_text) and existing is not None:
      keys = change.LookupAliases()
      self._lookup_cache.InjectCustomKeys(keys, existing)
      return existing

    self.InjectLookupCache([change])
    if change.IsAlreadyMerged():
      self.InjectCommittedPatches([change])
    return change

  def _LookupUncommittedChanges(self, deps, limit_to=None):
    """Given a set of deps (changes), return unsatisfied dependencies.

    Args:
      deps: A list of cros_patch.PatchQuery objects representing
        sequence of dependencies for the leaf that we need to identify
        as either merged, or needing resolving.
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

      try:
        self._LookupHelper(dep)
      except GerritHelperNotAvailable:
        # Internal dependencies are irrelevant to external builders.
        logging.info("Skipping internal dependency: %s", dep)
        continue

      dep_change = self._lookup_cache[dep]

      if dep_change is None:
        dep_change = self._GetGerritPatch(dep)
      if dep_change is None:
        continue
      if getattr(dep_change, 'IsAlreadyMerged', lambda: False)():
        continue
      elif limit_to is not None and dep_change not in limit_to:
        if self._is_submitting:
          raise PatchRejected(dep_change)
        else:
          raise PatchNotCommitReady(dep_change)

      unsatisfied.append(dep_change)

    # Perform last minute custom filtering.
    return [x for x in unsatisfied if self.deps_filter_fn(x)]

  def CreateTransaction(self, change, limit_to=None):
    """Given a change, resolve it into a transaction.

    In this case, a transaction is defined as a group of commits that
    must land for the given change to be merged- specifically its
    parent deps, and its CQ-DEPEND.

    Args:
      change: A cros_patch.GitRepoPatch instance to generate a transaction
        for.
      limit_to: If non-None, limit the allowed uncommitted patches to
        what's in that container/mapping.

    Returns:
      A sequence of the necessary cros_patch.GitRepoPatch objects for
      this transaction.
    """
    plan, seen = [], cros_patch.PatchCache()
    self._AddChangeToPlanWithDeps(change, plan, seen, limit_to=limit_to)
    return plan

  def CreateTransactions(self, changes, limit_to=None):
    """Create a list of transactions from a list of changes.

    Args:
      changes: A list of cros_patch.GitRepoPatch instances to generate
        transactions for.
      limit_to: See CreateTransaction docs.

    Returns:
      A list of (change, plan, e) tuples for the given list of changes. The
      plan represents the necessary GitRepoPatch objects for a given change. If
      an exception occurs while creating the transaction, e will contain the
      exception. (Otherwise, e will be None.)
    """
    for change in changes:
      try:
        plan = self.CreateTransaction(change, limit_to=limit_to)
      except cros_patch.PatchException as e:
        yield (change, (), e)
      else:
        yield (change, plan, None)

  def CreateDisjointTransactions(self, changes, max_txn_length=None):
    """Create a list of disjoint transactions from a list of changes.

    Args:
      changes: A list of cros_patch.GitRepoPatch instances to generate
        transactions for.
      max_txn_length: The maximum length of any given transaction. Optional.
        By default, do not limit the length of transactions.

    Returns:
      A list of disjoint transactions and a list of exceptions. Each transaction
      can be tried independently, without involving patches from other
      transactions. Each change in the pool will included in exactly one of the
      transactions, unless the patch does not apply for some reason.
    """
    # Gather the dependency graph for the specified changes.
    deps, edges, failed = {}, {}, []
    for change, plan, ex in self.CreateTransactions(changes, limit_to=changes):
      if ex is not None:
        logging.info('Failed creating transaction for %s: %s', change, ex)
        failed.append(ex)
      else:
        # Save off the ordered dependencies of this change.
        deps[change] = plan

        # Mark every change in the transaction as bidirectionally connected.
        for change_dep in plan:
          edges.setdefault(change_dep, set()).update(plan)

    # Calculate an unordered group of strongly connected components.
    unordered_plans = digraph.StronglyConnectedComponents(list(edges), edges)

    # Sort the groups according to our ordered dependency graph.
    ordered_plans = []
    for unordered_plan in unordered_plans:
      ordered_plan, seen = [], set()
      for change in unordered_plan:
        # Iterate over the required CLs, adding them to our plan in order.
        new_changes = list(dep_change for dep_change in deps[change]
                           if dep_change not in seen)
        new_plan_size = len(ordered_plan) + len(new_changes)
        if not max_txn_length or new_plan_size <= max_txn_length:
          seen.update(new_changes)
          ordered_plan.extend(new_changes)

      if ordered_plan:
        # We found a transaction that is <= max_txn_length. Process the
        # transaction. Ignore the remaining patches for now; they will be
        # processed later (once the current transaction has been pushed).
        ordered_plans.append(ordered_plan)
      else:
        # We couldn't find any transactions that were <= max_txn_length.
        # This should only happen if circular dependencies prevent us from
        # truncating a long list of patches. Reject the whole set of patches
        # and complain.
        for change in unordered_plan:
          failed.append(PatchSeriesTooLong(change, max_txn_length))

    return ordered_plans, failed

  @_PatchWrapException
  def _AddChangeToPlanWithDeps(self, change, plan, seen, limit_to=None):
    """Add a change and its dependencies into a |plan|.

    Args:
      change: The change to add to the plan.
      plan: The list of changes to apply, in order. This function will append
        |change| and any necessary dependencies to |plan|.
      seen: The changes whose Gerrit dependencies have already been processed.
      limit_to: If non-None, limit the allowed uncommitted patches to
        what's in that container/mapping.

    Raises:
      DependencyError: If we could not resolve a dependency.
      GerritException or GOBError: If there is a failure in querying gerrit.
    """
    if change in self._committed_cache or change in plan:
      # If the requested change is already in the plan, then we have already
      # processed its dependencies.
      return

    # Get a list of the changes that haven't been committed.
    # These are returned as cros_patch.PatchQuery objects.
    gerrit_deps, paladin_deps = self.GetDepsForChange(change)

    # Only process the dependencies for each change once. We prioritize Gerrit
    # dependencies over CQ dependencies, since Gerrit dependencies might be
    # required in order for the change to apply.
    if change not in seen:
      gerrit_deps = self._LookupUncommittedChanges(
          gerrit_deps, limit_to=limit_to)
      seen.Inject(change)
      for dep in gerrit_deps:
        self._AddChangeToPlanWithDeps(dep, plan, seen, limit_to=limit_to)

    # If there are cyclic dependencies, we might have already applied this
    # patch as part of dependency resolution. If not, apply this patch.
    if change not in plan:
      plan.append(change)

      # Process paladin deps last, so as to avoid circular dependencies between
      # gerrit dependencies and paladin dependencies.
      paladin_deps = self._LookupUncommittedChanges(
          paladin_deps, limit_to=limit_to)
      for dep in paladin_deps:
        # Add the requested change (plus deps) to our plan, if it we aren't
        # already in the process of doing that.
        if dep not in seen:
          self._AddChangeToPlanWithDeps(dep, plan, seen, limit_to=limit_to)

  @_PatchWrapException
  def GetDepChangesForChange(self, change):
    """Look up the gerrit/paladin dependency changes for |change|.

    Returns:
      A tuple of GerritPatch objects which are change's Gerrit
      dependencies, and Paladin dependencies.

    Raises:
      DependencyError: If we could not resolve a dependency.
      GerritException or GOBError: If there is a failure in querying gerrit.
    """
    gerrit_deps, paladin_deps = self.GetDepsForChange(change)

    def _DepsToChanges(deps):
      dep_changes = []
      unprocessed_deps = []
      for dep in deps:
        dep_change = self._committed_cache[dep]
        if dep_change:
          dep_changes.append(dep_change)
        else:
          unprocessed_deps.append(dep)

      for dep in unprocessed_deps:
        dep_changes.extend(self._LookupUncommittedChanges(deps))

      return dep_changes

    return _DepsToChanges(gerrit_deps), _DepsToChanges(paladin_deps)

  @_PatchWrapException
  def GetDepsForChange(self, change):
    """Look up the gerrit/paladin deps for |change|.

    Returns:
      A tuple of PatchQuery objects representing change's Gerrit
      dependencies, and Paladin dependencies.

    Raises:
      DependencyError: If we could not resolve a dependency.
      GerritException or GOBError: If there is a failure in querying gerrit.
    """
    val = self._change_deps_cache.get(change)
    if val is None:
      git_repo = self.GetGitRepoForChange(change)
      val = self._change_deps_cache[change] = (
          change.GerritDependencies(),
          change.PaladinDependencies(git_repo))

    return val

  def InjectCommittedPatches(self, changes):
    """Record that the given patches are already committed.

    This is primarily useful for external code to notify this object
    that changes were applied to the tree outside its purview- specifically
    useful for dependency resolution.
    """
    self._committed_cache.Inject(*changes)

  def InjectLookupCache(self, changes):
    """Inject into the internal lookup cache the given changes, using them
    (rather than asking gerrit for them) as needed for dependencies.
    """
    self._lookup_cache.Inject(*changes)

  def FetchChanges(self, changes):
    """Fetch the specified changes, if needed.

    If we're an external builder, internal changes are filtered out.

    Returns:
      An iterator over a list of the filtered changes.
    """
    for change in changes:
      try:
        self._helper_pool.ForChange(change)
      except GerritHelperNotAvailable:
        # Internal patches are irrelevant to external builders.
        logging.info("Skipping internal patch: %s", change)
        continue
      change.Fetch(self.GetGitRepoForChange(change, strict=True))
      yield change

  @_ManifestDecorator
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
    changes = list(self.FetchChanges(changes))
    if changes_filter:
      changes = changes_filter(self, changes)

    self.InjectLookupCache(changes)
    limit_to = cros_patch.PatchCache(changes) if frozen else None
    resolved, applied, failed = [], [], []
    for change, plan, ex in self.CreateTransactions(changes, limit_to=limit_to):
      if ex is not None:
        logging.info("Failed creating transaction for %s: %s", change, ex)
        failed.append(ex)
      else:
        resolved.append((change, plan))
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
      except cros_patch.PatchException as e:
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
    self._is_submitting = True

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
    project_state = set(
        map(functools.partial(self.GetGitRepoForChange, strict=True), commits))
    resets = []
    for project_dir in project_state:
      current_sha1 = git.RunGit(
          project_dir, ['rev-list', '-n1', 'HEAD']).output.strip()
      resets.append((project_dir, current_sha1))
      assert current_sha1

    committed_cache = self._committed_cache.copy()

    try:
      yield
      # Reaching here means it was applied cleanly, thus return.
      return
    except Exception:
      logging.info("Rewinding transaction: failed changes: %s .",
                   ', '.join(map(str, commits)), exc_info=True)

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
      except cros_patch.PatchException as e:
        if not e.inflight:
          self.failed_tot[change.id] = e
        raise
      applied.append(change)

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
      A PatchSeries instance w/ a forced manifest.
    """

    if 'forced_manifest' in kwargs:
      raise ValueError("RawPatchSeries doesn't allow a forced_manifest "
                       "argument.")
    merging = kwargs.setdefault('force_content_merging', True)
    kwargs['forced_manifest'] = _ManifestShim(
        git_repo, tracking_branch, content_merging=merging)

    return cls(git_repo, **kwargs)


class _ManifestShim(object):
  """A fake manifest that only contains a single repository.

  This fake manifest is used to allow us to filter out patches for
  the PatchSeries class. It isn't a complete implementation -- we just
  implement the functions that PatchSeries uses. It works via duck typing.

  All of the below methods accept the same arguments as the corresponding
  methods in git.ManifestCheckout.*, but they do not make any use of the
  arguments -- they just always return information about this project.
  """

  def __init__(self, path, tracking_branch, remote='origin',
               content_merging=True):

    tracking_branch = 'refs/remotes/%s/%s' % (
        remote, git.StripRefs(tracking_branch),
    )
    attrs = dict(local_path=path, path=path, tracking_branch=tracking_branch)
    self.checkout = git.ProjectCheckout(attrs)
    self.content_merging = content_merging

  def FindCheckouts(self, *_args, **_kwargs):
    """Returns the list of checkouts.

    In this case, we only have one repository so we just return that repository.
    We accept the same arguments as git.ManifestCheckout.FindCheckouts, but we
    do not make any use of them.

    Returns:
      A list of ProjectCheckout objects.
    """
    return [self.checkout]

  def ProjectIsContentMerging(self, *_args, **_kwargs):
    """Check whether this project has content-merging enabled."""
    return self.content_merging


class ValidationFailedMessage(object):
  """Message indicating that changes failed to be validated."""

  def __init__(self, message, tracebacks, internal):
    """Create a ValidationFailedMessage object.

    Args:
      message: The message to print.
      tracebacks: Exceptions received by individual builders, if any.
      internal: Whether this failure occurred on an internal builder.
    """
    # Convert each of the input arguments into simple Python datastructures
    # (i.e. not generators) that can be easily pickled.
    self.message = str(message)
    self.tracebacks = tuple(tracebacks)
    self.internal = bool(internal)

  def __str__(self):
    return self.message

  def MightBeFlakyFailure(self):
    """Check if there is a good chance this is a flaky failure."""
    # We only consider a failed build to be flaky if there is only one failure,
    # and that failure is a flaky failure.
    flaky = False
    if len(self.tracebacks) == 1:
      # TimeoutErrors are often flaky.
      exc = self.tracebacks[0].exception
      if (isinstance(exc, results_lib.StepFailure) and exc.possibly_flaky or
          isinstance(exc, timeout_util.TimeoutError)):
        flaky = True
    return flaky

  def _MatchesFailureType(self, cls):
    """Check if all of the tracebacks match the specified failure type."""
    for traceback in self.tracebacks:
      if not isinstance(traceback.exception, cls):
        return False
    return True

  def IsPackageBuildFailure(self):
    """Check if all of the failures are package build failures."""
    return self._MatchesFailureType(results_lib.PackageBuildFailure)

  def FindPackageBuildFailureSuspects(self, changes):
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

    Args:
      changes: List of changes to examine.

    Returns:
      Set of changes that likely caused the failure.
    """
    blame_everything = False
    suspects = set()
    for traceback in self.tracebacks:
      for package in traceback.exception.failed_packages:
        failed_projects = portage_utilities.FindWorkonProjects([package])
        blame_assigned = False
        for change in changes:
          if change.project in failed_projects:
            blame_assigned = True
            suspects.add(change)
        if not blame_assigned:
          blame_everything = True

    if blame_everything or not suspects:
      suspects = changes[:]
    else:
      # Never treat changes to overlays as innocent.
      suspects.update(change for change in changes
                      if '/overlays/' in change.project)
    return suspects


class CalculateSuspects(object):
  """Diagnose the cause for a given set of failures."""

  @classmethod
  def _FindPackageBuildFailureSuspects(cls, changes, messages):
    """Figure out what CLs are at fault for a set of build failures."""
    suspects = set()
    for message in messages:
      suspects.update(message.FindPackageBuildFailureSuspects(changes))
    return suspects

  @classmethod
  def _MightBeFlakyFailure(cls, messages):
    """Check if there is a good chance this is a flaky failure."""
    # We consider a failed commit queue run to be flaky if only one builder
    # failed, and that failure is flaky.
    return len(messages) == 1 and messages[0].MightBeFlakyFailure()

  @classmethod
  def _FindPreviouslyFailedChanges(cls, candidates):
    """Find what changes that have previously failed the CQ.

    The first time a change is included in a build that fails due to a
    flaky (or apparently unrelated) failure, we assume that it is innocent. If
    this happens more than once, we kick out the CL.
    """
    suspects = set()
    for change in candidates:
      if ValidationPool.GetCLStatusCount(
          CQ, change, ValidationPool.STATUS_FAILED):
        suspects.add(change)
    return suspects

  @classmethod
  def FindSuspects(cls, changes, messages):
    """Find out what changes probably caused our failure.

    In cases where there were no internal failures, we can assume that the
    external failures are at fault. Otherwise, this function just defers to
    _FindPackagedBuildFailureSuspects and FindPreviouslyFailedChanges as needed.
    If the failures don't match either case, just fail everything.
    """

    suspects = set()

    # If there were no internal failures, only kick out external changes.
    if any(message.internal for message in messages):
      candidates = changes
    else:
      candidates = [change for change in changes if not change.internal]

    if all(message.IsPackageBuildFailure() for message in messages):
      suspects = cls._FindPackageBuildFailureSuspects(candidates, messages)
    elif cls._MightBeFlakyFailure(messages):
      suspects = cls._FindPreviouslyFailedChanges(changes)
    else:
      suspects.update(candidates)

    return suspects


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
  STATUS_FAILED = manifest_version.BuilderStatus.STATUS_FAILED
  STATUS_INFLIGHT = manifest_version.BuilderStatus.STATUS_INFLIGHT
  STATUS_PASSED = manifest_version.BuilderStatus.STATUS_PASSED
  STATUS_LAUNCHING = 'launching'
  STATUS_WAITING = 'waiting'
  INCONSISTENT_SUBMIT_MSG = ('Gerrit thinks that the change was not submitted, '
                             'even though we hit the submit button.')

  # The grace period (in seconds) before we reject a patch due to dependency
  # errors.
  REJECTION_GRACE_PERIOD = 30 * 60

  # Cache for the status of CLs.
  _CL_STATUS_CACHE = {}

  def __init__(self, overlays, build_root, build_number, builder_name,
               is_master, dryrun, changes=None, non_os_changes=None,
               conflicting_changes=None, pre_cq=False, metadata=None):
    """Initializes an instance by setting default variables to instance vars.

    Generally use AcquirePool as an entry pool to a pool rather than this
    method.

    Args:
      overlays: One of constants.VALID_OVERLAYS.
      build_root: Build root directory.
      build_number: Build number for this validation attempt.
      builder_name: Builder name on buildbot dashboard.
      is_master: True if this is the master builder for the Commit Queue.
      dryrun: If set to True, do not submit anything to Gerrit.
    Optional Args:
      changes: List of changes for this validation pool.
      non_os_changes: List of changes that are part of this validation
        pool but aren't part of the cros checkout.
      conflicting_changes: Changes that failed to apply but we're keeping around
        because they conflict with other changes in flight.
      pre_cq: If set to True, this builder is verifying CLs before they go to
        the commit queue.
      metadata: Optional CBuildbotMetadata instance where CL actions will
                be recorded.
    """

    self.build_root = build_root

    # These instances can be instantiated via both older, or newer pickle
    # dumps.  Thus we need to assert the given args since we may be getting
    # a value we no longer like (nor work with).
    if overlays not in constants.VALID_OVERLAYS:
      raise ValueError("Unknown/unsupported overlay: %r" % (overlays,))

    self._helper_pool = self.GetGerritHelpersForOverlays(overlays)

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

    self.build_log = self.ConstructDashboardURL(overlays, pre_cq, builder_name,
                                                str(build_number))

    self.is_master = bool(is_master)
    self.pre_cq = pre_cq
    self._metadata = metadata
    self.dryrun = bool(dryrun) or self.GLOBAL_DRYRUN
    self.queue = 'A trybot' if pre_cq else 'The Commit Queue'
    self.bot = PRE_CQ if pre_cq else CQ

    # See optional args for types of changes.
    self.changes = changes or []
    self.non_manifest_changes = non_os_changes or []
    # Note, we hold onto these CLs since they conflict against our current CLs
    # being tested; if our current ones succeed, we notify the user to deal
    # w/ the conflict.  If the CLs we're testing fail, then there is no
    # reason we can't try these again in the next run.
    self.changes_that_failed_to_apply_earlier = conflicting_changes or []

    # Integer timestamp (i.e. int(time.time())) at which the changes in this
    # pool were acquired. Set by AcquirePool or AcquirePoolFromManifest.
    self.acquired_at_time = 0

    # Private vars only used for pickling.
    self._overlays = overlays
    self._build_number = build_number
    self._builder_name = builder_name

  @staticmethod
  def GetBuildDashboardForOverlays(overlays, trybot):
    """Discern the dashboard to use based on the given overlay."""
    if trybot:
      return constants.TRYBOT_DASHBOARD
    if overlays in [constants.PRIVATE_OVERLAYS, constants.BOTH_OVERLAYS]:
      return constants.BUILD_INT_DASHBOARD
    return constants.BUILD_DASHBOARD

  @classmethod
  def ConstructDashboardURL(cls, overlays, trybot, builder_name, build_number,
                            stage=None):
    """Return the dashboard (buildbot) URL for this run

    Args:
      overlays: One of constants.VALID_OVERLAYS.
      trybot: Boolean: is this a remote trybot?
      builder_name: Builder name on buildbot dashboard.
      build_number: Build number for this validation attempt.
      stage: Link directly to a stage log, else use the general landing page.

    Returns:
      The fully formed URL
    """
    build_dashboard = cls.GetBuildDashboardForOverlays(overlays, trybot)
    url_suffix = 'builders/%s/builds/%s' % (builder_name, str(build_number))
    if stage:
      url_suffix += '/steps/%s/logs/stdio' % (stage,)
    url_suffix = urllib.quote(url_suffix)
    return os.path.join(build_dashboard, url_suffix)

  @staticmethod
  def GetGerritHelpersForOverlays(overlays):
    """Discern the allowed GerritHelpers to use based on the given overlay."""
    cros_internal = cros = False
    if overlays in [constants.PUBLIC_OVERLAYS, constants.BOTH_OVERLAYS, False]:
      cros = True

    if overlays in [constants.PRIVATE_OVERLAYS, constants.BOTH_OVERLAYS]:
      cros_internal = True

    return HelperPool.SimpleCreate(cros_internal=cros_internal, cros=cros)

  def __reduce__(self):
    """Used for pickling to re-create validation pool."""
    # NOTE: self._metadata is specifically excluded from the validation pool
    # pickle. We do not want the un-pickled validation pool to have a reference
    # to its own un-pickled metadata instance. Instead, we want to to refer
    # to the builder run's metadata instance. This is accomplished by setting
    # metadata at un-pickle time, in ValidationPool.Load(...).
    return (
        self.__class__,
        (
            self._overlays,
            self.build_root, self._build_number, self._builder_name,
            self.is_master, self.dryrun, self.changes,
            self.non_manifest_changes,
            self.changes_that_failed_to_apply_earlier,
            self.pre_cq))

  @classmethod
  def FilterDraftChanges(cls, changes):
    """Filter out draft changes based on the status of the latest patch set.

    Our Gerrit query cannot exclude changes whose latest patch set has
    not yet been published as long as there is one published patchset
    in the change. Such changes will fail when we try to merge them,
    which may lead to undesirable consequence (e.g. dependencies not
    respected).

    Args:
      changes: List of changes to filter.

    Returns:
      List of published changes.
    """
    return [x for x in changes if not x.patch_dict['currentPatchSet']['draft']]

  @classmethod
  def FilterNonMatchingChanges(cls, changes):
    """Filter out changes that don't actually match our query.

    Generally, Gerrit should only return patches that match our
    query. However, Gerrit keeps a query cache and the cached data may
    be stale.

    There are also race conditions (bugs in Gerrit) where the final
    patch won't match our query. Here's an example problem that this
    code fixes: If the Pre-CQ launcher picks up a CL while the CQ is
    committing the CL, it may catch a race condition where a new
    patchset has been created and committed by the CQ, but the CL is
    still treated as if it matches the query (which it doesn't,
    anymore).

    Args:
      changes: List of changes to filter.

    Returns:
      List of changes that match our query.
    """
    filtered_changes = []
    for change in changes:
      # Because the gerrit cache sometimes gets stale, double-check that the
      # change hasn't already been merged.
      if change.status != 'NEW':
        continue
      # Check whether the change should be rejected (e.g. verified:
      # -1, code-review: -2).
      should_reject = False
      for field, value in constants.DEFAULT_CQ_SHOULD_REJECT_FIELDS.iteritems():
        if change.HasApproval(field, value):
          should_reject = True
          break

      if should_reject:
        continue
      # Check that the user (or chrome-bot) uploaded a new change under our
      # feet while Gerrit was in the middle of answering our query.
      for field, value in constants.DEFAULT_CQ_READY_FIELDS.iteritems():
        if not change.HasApproval(field, value):
          break
      else:
        filtered_changes.append(change)

    return filtered_changes

  @classmethod
  def AcquirePreCQPool(cls, *args, **kwargs):
    """See ValidationPool.__init__ for arguments."""
    kwargs.setdefault('pre_cq', True)
    kwargs.setdefault('is_master', True)
    return cls(*args, **kwargs)

  @classmethod
  def AcquirePool(cls, overlays, repo, build_number, builder_name,
                  dryrun=False, changes_query=None, check_tree_open=True,
                  change_filter=None, throttled_ok=False, metadata=None):
    """Acquires the current pool from Gerrit.

    Polls Gerrit and checks for which changes are ready to be committed.
    Should only be called from master builders.

    Args:
      overlays: One of constants.VALID_OVERLAYS.
      repo: The repo used to sync, to filter projects, and to apply patches
        against.
      build_number: Corresponding build number for the build.
      builder_name: Builder name on buildbot dashboard.
      dryrun: Don't submit anything to gerrit.
      changes_query: The gerrit query to use to identify changes; if None,
        uses the internal defaults.
      check_tree_open: If True, only return when the tree is open.
      change_filter: If set, use change_filter(pool, changes,
        non_manifest_changes) to filter out unwanted patches.
      throttled_ok: if |check_tree_open|, treat a throttled tree as open.
                    Default: True.
      metadata: Optional CBuildbotMetadata instance where CL actions will
                be recorded.

    Returns:
      ValidationPool object.

    Raises:
      TreeIsClosedException: if the tree is closed (or throttled, if not
                             |throttled_ok|).
    """
    if change_filter is None:
      change_filter = lambda _, x, y: (x, y)

    # We choose a longer wait here as we haven't committed to anything yet. By
    # doing this here we can reduce the number of builder cycles.
    end_time = time.time() + cls.MAX_TIMEOUT
    while True:
      time_left = end_time - time.time()

      # Wait until the tree becomes open (or throttled, if |throttled_ok|,
      # and record the tree status in tree_status).
      if check_tree_open:
        try:
          tree_status = timeout_util.WaitForTreeStatus(
              cls.STATUS_URL, cls.SLEEP_TIMEOUT, timeout=time_left,
              throttled_ok=throttled_ok)
        except timeout_util.TimeoutError:
          raise TreeIsClosedException(closed_or_throttled=not throttled_ok)
      else:
        tree_status = constants.TREE_OPEN

      waiting_for = 'new CLs'

      # Select the right default gerrit query based on the the tree
      # status, or use custom |changes_query| if it was provided.
      using_default_query = (changes_query is None)
      if not using_default_query:
        query = changes_query
      elif tree_status == constants.TREE_THROTTLED:
        query = constants.THROTTLED_CQ_READY_QUERY
        waiting_for = 'new CQ+2 CLs or the tree to open'
      else:
        query = constants.DEFAULT_CQ_READY_QUERY

      # Sync so that we are up-to-date on what is committed.
      repo.Sync()

      # Only master configurations should call this method.
      pool = ValidationPool(overlays, repo.directory, build_number,
                            builder_name, True, dryrun, metadata=metadata)

      draft_changes = []
      # Iterate through changes from all gerrit instances we care about.
      for helper in cls.GetGerritHelpersForOverlays(overlays):
        raw_changes = helper.Query(query, sort='lastUpdated')
        raw_changes.reverse()

        # Reload the changes because the data in the Gerrit cache may be stale.
        raw_changes = list(cls.ReloadChanges(raw_changes))

        # If we used a default query, verify the results match the query, to
        # prevent race conditions. Note, this filters using the conditions
        # of DEFAULT_CQ_READY_QUERY even if the tree is throttled. Since that
        # query is strictly more permissive than the throttled query, we are
        # not at risk of incorrectly losing any patches here. We only expose
        # ourselves to the minor race condititon that a CQ+2 patch could have
        # been marked as CQ+1 out from under us, but still end up being picked
        # up in a throttled CQ run.
        if using_default_query:
          published_changes = cls.FilterDraftChanges(raw_changes)
          draft_changes.extend(set(raw_changes) - set(published_changes))
          raw_changes = cls.FilterNonMatchingChanges(published_changes)

        changes, non_manifest_changes = ValidationPool._FilterNonCrosProjects(
            raw_changes, git.ManifestCheckout.Cached(repo.directory))
        pool.changes.extend(changes)
        pool.non_manifest_changes.extend(non_manifest_changes)

      for change in draft_changes:
        pool.HandleDraftChange(change)

      # Filter out unwanted changes.
      pool.changes, pool.non_manifest_changes = change_filter(
          pool, pool.changes, pool.non_manifest_changes)

      if (pool.changes or pool.non_manifest_changes or dryrun or time_left < 0
          or cls.ShouldExitEarly()):
        break

      logging.info('Waiting for %s (%d minutes left)...', waiting_for,
                   time_left / 60)
      time.sleep(cls.SLEEP_TIMEOUT)

    pool.acquired_at_time = int(time.time())
    return pool

  @classmethod
  def AcquirePoolFromManifest(cls, manifest, overlays, repo, build_number,
                              builder_name, is_master, dryrun, metadata=None):
    """Acquires the current pool from a given manifest.

    This function assumes that you have already synced to the given manifest.

    Args:
      manifest: path to the manifest where the pool resides.
      overlays: One of constants.VALID_OVERLAYS.
      repo: The repo used to filter projects and to apply patches against.
      build_number: Corresponding build number for the build.
      builder_name: Builder name on buildbot dashboard.
      is_master: Boolean that indicates whether this is a pool for a master.
        config or not.
      dryrun: Don't submit anything to gerrit.
      metadata: Optional CBuildbotMetadata instance where CL actions will
                be recorded.

    Returns:
      ValidationPool object.
    """
    pool = ValidationPool(overlays, repo.directory, build_number, builder_name,
                          is_master, dryrun, metadata=metadata)
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

    pool.acquired_at_time = int(time.time())
    return pool

  @classmethod
  def ShouldExitEarly(cls):
    """Return whether we should exit early.

    This function is intended to be overridden by tests or by subclasses.
    """
    return False

  @staticmethod
  def _FilterNonCrosProjects(changes, manifest):
    """Filters changes to a tuple of relevant changes.

    There are many code reviews that are not part of Chromium OS and/or
    only relevant on a different branch. This method returns a tuple of (
    relevant reviews in a manifest, relevant reviews not in the manifest). Note
    that this function must be run while chromite is checked out in a
    repo-managed checkout.

    Args:
      changes: List of GerritPatch objects.
      manifest: The manifest to check projects/branches against.

    Returns:
      Tuple of (relevant reviews in a manifest,
                relevant reviews not in the manifest).
    """

    def IsCrosReview(change):
      return (change.project.startswith('chromiumos') or
              change.project.startswith('chromeos'))

    # First we filter to only Chromium OS repositories.
    changes = [c for c in changes if IsCrosReview(c)]

    changes_in_manifest = []
    changes_not_in_manifest = []
    for change in changes:
      if change.GetCheckout(manifest, strict=False):
        changes_in_manifest.append(change)
      else:
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

  @classmethod
  def PrintLinksToChanges(cls, changes):
    """Print links to the specified |changes|.

    Args:
      changes: A list of cros_patch.GerritPatch instances to generate
        transactions for.
    """
    # Completely fill the status cache in parallel.
    cls.FillCLStatusCache(CQ, changes)

    def SortKeyForChanges(change):
      all_failures = cls.GetCLStatusCount(CQ, change, cls.STATUS_FAILED,
                                          latest_patchset_only=False)
      failures = cls.GetCLStatusCount(CQ, change, cls.STATUS_FAILED)
      return (-all_failures, -failures,
              os.path.basename(change.project), change.gerrit_number)

    # Now, sort and print the changes.
    for change in sorted(changes, key=SortKeyForChanges):
      project = os.path.basename(change.project)
      gerrit_number = cros_patch.AddPrefix(change, change.gerrit_number)
      s = '%s | %s | %s' % (project, change.owner, gerrit_number)

      # Print a count of how many times a given CL has failed the CQ.
      all_failures = cls.GetCLStatusCount(CQ, change, cls.STATUS_FAILED,
                                          latest_patchset_only=False)
      failures = cls.GetCLStatusCount(CQ, change, cls.STATUS_FAILED)
      if all_failures:
        s += ' | fails:%d' % (failures,)
        if all_failures > failures:
          s += '(%d)' % (all_failures,)

      # Add a note if the latest patchset has already passed the CQ.
      passed = cls.GetCLStatusCount(CQ, change, cls.STATUS_PASSED)
      if passed > 0:
        s += ' | passed:%d' % passed

      cros_build_lib.PrintBuildbotLink(s, change.url)

  def ApplyPoolIntoRepo(self, manifest=None):
    """Applies changes from pool into the directory specified by the buildroot.

    This method applies changes in the order specified.  It also respects
    dependency order.

    Returns:
      True if we managed to apply any changes.
    """
    patch_series = PatchSeries(self.build_root, helper_pool=self._helper_pool)
    try:
      # pylint: disable=E1123
      applied, failed_tot, failed_inflight = patch_series.Apply(
          self.changes, dryrun=self.dryrun, manifest=manifest)
    except (KeyboardInterrupt, RuntimeError, SystemExit):
      raise
    except Exception as e:
      if mox is not None and isinstance(e, mox.Error):
        raise

      msg = (
          'Unhandled exception occurred while applying changes: %s\n\n'
          'To be safe, we have kicked out all of the CLs, so that the '
          'commit queue does not go into an infinite loop retrying '
          'patches.' % (e,)
      )
      links = ', '.join('CL:%s' % x.gerrit_number_str for x in self.changes)
      cros_build_lib.Error('%s\nAffected Patches are: %s', msg, links)
      errors = [InternalCQError(patch, msg) for patch in self.changes]
      self._HandleApplyFailure(errors)
      raise

    self.PrintLinksToChanges(applied)

    if self.is_master:
      inputs = [[change] for change in applied]
      parallel.RunTasksInProcessPool(self._HandleApplySuccess, inputs)

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

  @staticmethod
  def Load(filename, metadata=None):
    """Loads the validation pool from the file.

    Args:
      filename: path of file to load from.
      metadata: Optional CBuildbotInstance to use as metadata object
                for loaded pool (as metadata instances do not survive
                pickle/unpickle)
    """
    with open(filename, 'rb') as p_file:
      pool = cPickle.load(p_file)
      pool._metadata = metadata
      return pool

  def Save(self, filename):
    """Serializes the validation pool."""
    with open(filename, 'wb') as p_file:
      cPickle.dump(self, p_file, protocol=cPickle.HIGHEST_PROTOCOL)

  # Note: All submit code, all gerrit code, and basically everything other
  # than patch resolution/applying needs to use .change_id from patch objects.
  # Basically all code from this point forward.
  def _SubmitChangeWithDeps(self, patch_series, change, errors, limit_to):
    """Submit |change| and its dependencies.

    If you call this function multiple times with the same PatchSeries, each
    CL will only be submitted once.

    Args:
      patch_series: A PatchSeries() object.
      change: The change (a GerritPatch object) to submit.
      errors: A dictionary. This dictionary should contain all patches that have
        encountered errors, and map them to the associated exception object.
      limit_to: The list of patches that were approved by this CQ run. We will
        only consider submitting patches that are in this list.

    Returns:
      A copy of the errors object. If new errors have occurred while submitting
      this change, and those errors have prevented a change from being
      submitted, they will be added to the errors object.
    """
    # Find out what patches we need to submit.
    errors = errors.copy()
    try:
      plan = patch_series.CreateTransaction(change, limit_to=limit_to)
    except cros_patch.PatchException as e:
      self._HandleCouldNotSubmit(change, e)
      errors[change] = e
      return errors

    error_stack, submitted = [], []
    for dep_change in plan:
      # Has this change failed to submit before?
      dep_error = errors.get(dep_change)
      if dep_error is None and error_stack:
        # One of the dependencies failed to submit. Report an error.
        dep_error = cros_patch.DependencyError(dep_change, error_stack[-1])

      # If there were no errors, submit the patch.
      if dep_error is None:
        try:
          if self._SubmitChange(dep_change) or self.dryrun:
            submitted.append(dep_change)
          else:
            msg = self.INCONSISTENT_SUBMIT_MSG
            dep_error = PatchFailedToSubmit(dep_change, msg)
        except (gob_util.GOBError, gerrit.GerritException) as e:
          if getattr(e, 'http_status', None) == httplib.CONFLICT:
            dep_error = PatchConflict(dep_change)
          else:
            dep_error = PatchFailedToSubmit(dep_change, str(e))
          logging.error('%s', dep_error)

      # Add any error we saw to the stack.
      if dep_error is not None:
        logging.info('%s', dep_error)
        errors[dep_change] = dep_error
        error_stack.append(dep_error)

    # Track submitted patches so that we don't submit them again.
    patch_series.InjectCommittedPatches(submitted)

    # Look for incorrectly submitted patches. We only have this problem
    # when we have a dependency cycle, and we submit one change before
    # realizing that a later change cannot be submitted. For now, just
    # print an error message and notify the developers.
    #
    # If you see this error a lot, consider implementing a best-effort
    # attempt at reverting changes.
    for submitted_change in submitted:
      gdeps, pdeps = patch_series.GetDepChangesForChange(submitted_change)
      for dep in gdeps + pdeps:
        dep_error = errors.get(dep)
        if dep_error is not None:
          error = PatchSubmittedWithoutDeps(submitted_change, dep_error)
          self._HandleIncorrectSubmission(error)
          logging.error('%s was erroneously submitted.', submitted_change)

    return errors

  def SubmitChanges(self, changes, check_tree_open=True, throttled_ok=True):
    """Submits the given changes to Gerrit.

    Args:
      changes: GerritPatch's to submit.
      check_tree_open: Whether to check that the tree is open before submitting
        changes. If this is False, TreeIsClosedException will never be raised.
      throttled_ok: if |check_tree_open|, treat a throttled tree as open

    Returns:
      A list of the changes that failed to submit.

    Raises:
      TreeIsClosedException: if the tree is closed.
    """
    assert self.is_master, 'Non-master builder calling SubmitPool'
    assert not self.pre_cq, 'Trybot calling SubmitPool'

    # Mark all changes as successful.
    inputs = [[self.bot, change, self.STATUS_PASSED, self.dryrun]
              for change in changes]
    parallel.RunTasksInProcessPool(self.UpdateCLStatus, inputs)

    if (check_tree_open and not self.dryrun and not
       timeout_util.IsTreeOpen(self.STATUS_URL, self.SLEEP_TIMEOUT,
                               timeout=self.MAX_TIMEOUT,
                               throttled_ok=throttled_ok)):
      raise TreeIsClosedException(close_or_throttled=not throttled_ok)

    # First, reload all of the changes from the Gerrit server so that we have a
    # fresh view of their approval status. This is needed so that our filtering
    # that occurs below will be mostly up-to-date.
    errors = {}
    changes = list(self.ReloadChanges(changes))

    # Filter out changes that are already merged (e.g. dev chumped the
    # CL during the CQ run). We do not consider these as errors, and
    # print out warnings instead.
    uncommitted_changes = [x for x in changes if not x.IsAlreadyMerged()]
    for change in set(changes) - set(uncommitted_changes):
      logging.warning('%s is already merged. It was most likely chumped during '
                      'the current CQ run.', change)

    # Filter out the draft changes here to prevent the race condition
    # where user uploads a new draft patch set during the CQ run.
    published_changes = self.FilterDraftChanges(uncommitted_changes)
    for change in set(uncommitted_changes) - set(published_changes):
      errors[change] = PatchNotPublished(change)

    # Filter out changes that aren't marked as CR=+2, CQ=+1, V=+1 anymore, in
    # case the patch status changed during the CQ run.
    filtered_changes = self.FilterNonMatchingChanges(published_changes)
    for change in set(published_changes) - set(filtered_changes):
      errors[change] = PatchNotCommitReady(change)

    patch_series = PatchSeries(self.build_root, helper_pool=self._helper_pool)
    patch_series.InjectLookupCache(filtered_changes)
    for change in filtered_changes:
      errors = self._SubmitChangeWithDeps(patch_series, change, errors,
                                          filtered_changes)

    for patch, error in errors.iteritems():
      logging.error('Could not submit %s', patch)
      self._HandleCouldNotSubmit(patch, error)

    return errors.keys()

  @classmethod
  def ReloadChanges(cls, changes):
    """Reload the specified |changes| from the server.

    Args:
      changes: A list of PatchQuery objects.

    Returns:
      A list of GerritPatch objects.
    """
    return gerrit.GetGerritPatchInfoWithPatchQueries(changes)

  def _SubmitChange(self, change):
    """Submits patch using Gerrit Review."""
    logging.info('Change %s will be submitted', change)
    was_change_submitted = False
    helper = self._helper_pool.ForChange(change)
    helper.SubmitChange(change, dryrun=self.dryrun)
    updated_change = helper.QuerySingleRecord(change.gerrit_number)

    # If change is 'SUBMITTED' give gerrit some time to resolve that
    # to 'MERGED' or fail outright.
    if updated_change.status == 'SUBMITTED':
      def _Query():
        return helper.QuerySingleRecord(change.gerrit_number)
      def _Retry(value):
        return value and value.status == 'SUBMITTED'

      try:
        updated_change = timeout_util.WaitForSuccess(
            _Retry, _Query, timeout=SUBMITTED_WAIT_TIMEOUT, period=1)
      except timeout_util.TimeoutError:
        # The change really is stuck on submitted, not merged, then.
        logging.warning('Timed out waiting for gerrit to finish submitting'
                        ' change %s, but status is still "%s".',
                        change.gerrit_number_str, updated_change.status)

    was_change_submitted = updated_change.status == 'MERGED'
    if not was_change_submitted:
      logging.warning(
          'Change %s was submitted to gerrit without errors, but gerrit is'
          ' reporting it with status "%s" (expected "MERGED").',
          change.gerrit_number_str, updated_change.status)
      if updated_change.status == 'SUBMITTED':
        # So far we have never seen a SUBMITTED CL that did not eventually
        # transition to MERGED.  If it is stuck on SUBMITTED treat as MERGED.
        was_change_submitted = True
        logging.info('Proceeding now with the assumption that change %s'
                     ' will eventually transition to "MERGED".',
                     change.gerrit_number_str)
      else:
        logging.error('Most likely gerrit was unable to merge change %s.',
                      change.gerrit_number_str)

    if self._metadata:
      if was_change_submitted:
        action = constants.CL_ACTION_SUBMITTED
      else:
        action = constants.CL_ACTION_SUBMIT_FAILED
      self._metadata.RecordCLAction(change, action)

    return was_change_submitted

  def RemoveCommitReady(self, change):
    """Remove the commit ready bit for the specified |change|."""
    self._helper_pool.ForChange(change).RemoveCommitReady(change,
        dryrun=self.dryrun)
    if self._metadata:
      self._metadata.RecordCLAction(change, constants.CL_ACTION_KICKED_OUT)

  def SubmitNonManifestChanges(self, check_tree_open=True):
    """Commits changes to Gerrit from Pool that aren't part of the checkout.

    Args:
      check_tree_open: Whether to check that the tree is open before submitting
        changes. If this is False, TreeIsClosedException will never be raised.

    Raises:
      TreeIsClosedException: if the tree is closed.
    """
    self.SubmitChanges(self.non_manifest_changes,
                       check_tree_open=check_tree_open)

  def SubmitPool(self, check_tree_open=True, throttled_ok=True):
    """Commits changes to Gerrit from Pool.  This is only called by a master.

    Args:
      check_tree_open: Whether to check that the tree is open before submitting
        changes. If this is False, TreeIsClosedException will never be raised.
      throttled_ok: if |check_tree_open|, treat a throttled tree as open

    Raises:
      TreeIsClosedException: if the tree is closed.
      FailedToSubmitAllChangesException: if we can't submit a change.
    """
    # Note that SubmitChanges can throw an exception if it can't
    # submit all changes; in that particular case, don't mark the inflight
    # failures patches as failed in gerrit- some may apply next time we do
    # a CQ run (since the submit state has changed, we have no way of
    # knowing).  They *likely* will still fail, but this approach tries
    # to minimize wasting the developers time.
    errors = self.SubmitChanges(self.changes, check_tree_open=check_tree_open,
                                throttled_ok=throttled_ok)
    if errors:
      raise FailedToSubmitAllChangesException(errors)
    if self.changes_that_failed_to_apply_earlier:
      self._HandleApplyFailure(self.changes_that_failed_to_apply_earlier)

  def SubmitPartialPool(self, tracebacks):
    """If the build failed, push any CLs that don't care about the failure.

    Each project can specify a list of stages it does not care about in its
    COMMIT-QUEUE.ini file. Changes to that project will be submitted even if
    those stages fail.

    Args:
      tracebacks: A list of RecordedTraceback objects. These objects represent
        the exceptions that failed the build.

    Returns:
      A list of the rejected changes.
    """
    # Create a list of the failing stage prefixes.
    failing_stages = set(traceback.failed_prefix for traceback in tracebacks)

    # For each CL, look at whether it cares about the failures. Based on this,
    # categorize the CL as accepted or rejected.
    accepted, rejected = [], []
    for change in self.changes:
      ignored_stages = GetStagesToIgnoreForChange(self.build_root, change)
      if failing_stages.issubset(ignored_stages):
        accepted.append(change)
      else:
        rejected.append(change)

    # Actually submit the accepted changes.
    self.SubmitChanges(accepted)

    # Return the list of rejected changes.
    return rejected

  def _HandleApplyFailure(self, failures):
    """Handles changes that were not able to be applied cleanly.

    Args:
      failures: GerritPatch changes to handle.
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
      failure: GerritPatch instance to operate upon.
    """
    msg = ('%(queue)s failed to apply your change in %(build_log)s .'
           ' %(failure)s')
    self.SendNotification(failure.patch, msg, failure=failure)
    self.RemoveCommitReady(failure.patch)

  def _HandleIncorrectSubmission(self, failure):
    """Handler for when Paladin incorrectly submits a change."""
    msg = ('%(queue)s incorrectly submitted your change in %(build_log)s .'
           '  %(failure)s')
    self.SendNotification(failure.patch, msg, failure=failure)
    self.RemoveCommitReady(failure.patch)

  def HandleDraftChange(self, change):
    """Handler for when the latest patch set of |change| is not published.

    This handler removes the commit ready bit from the specified changes and
    sends the developer a message explaining why.

    Args:
      change: GerritPatch instance to operate upon.
    """
    msg = ('%(queue)s could not apply your change because the latest patch '
           'set is not published. Please publish your draft patch set before '
           'marking your commit as ready.')
    self.SendNotification(change, msg)
    self.RemoveCommitReady(change)

  def HandleValidationTimeout(self, changes=None, sanity=True):
    """Handles changes that timed out.

    This handler removes the commit ready bit from the specified changes and
    sends the developer a message explaining why.

    Args:
      changes: A list of cros_patch.GerritPatch instances to mark as failed.
        By default, mark all of the changes as failed.
      sanity: A boolean indicating whether the build was considered sane by
              any relevant sanity check builders (True = sane). If not sane,
              none of the changes will have their CommitReady bit modified.

    """
    if changes is None:
      changes = self.changes

    logging.info('Validation timed out for all changes.')
    msg = ('%(queue)s timed out while verifying your change in '
           '%(build_log)s . This means that a supporting builder did not '
           'finish building your change within the specified timeout.')
    if sanity:
      msg += ('If you believe this happened in error, just re-mark your '
              'commit as ready. Your change will then get automatically '
              'retried.')
    else:
      msg += ('The sanity check builder in this run failed, so no changes '
              'will be blamed for the failure.')

    for change in changes:
      logging.info('Validation timed out for change %s.', change)
      self.SendNotification(change, msg)
      if sanity:
        self.RemoveCommitReady(change)

  def SendNotification(self, change, msg, **kwargs):
    d = dict(build_log=self.build_log, queue=self.queue, **kwargs)
    try:
      msg %= d
    except (TypeError, ValueError) as e:
      logging.error(
          "Generation of message %s for change %s failed: dict was %r, "
          "exception %s", msg, change, d, e)
      raise e.__class__(
          "Generation of message %s for change %s failed: dict was %r, "
          "exception %s" % (msg, change, d, e))
    PaladinMessage(msg, change, self._helper_pool.ForChange(change)).Send(
        self.dryrun)

  def HandlePreCQSuccess(self):
    """Handler that is called when the Pre-CQ successfully verifies a change."""
    msg = '%(queue)s successfully verified your change in %(build_log)s .'
    for change in self.changes:
      if self.GetCLStatus(self.bot, change) != self.STATUS_PASSED:
        self.SendNotification(change, msg)
        self.UpdateCLStatus(self.bot, change, self.STATUS_PASSED,
                            dry_run=self.dryrun)

  def _HandleCouldNotSubmit(self, change, error=''):
    """Handler that is called when Paladin can't submit a change.

    This should be rare, but if an admin overrides the commit queue and commits
    a change that conflicts with this change, it'll apply, build/validate but
    receive an error when submitting.

    Args:
      change: GerritPatch instance to operate upon.
      error: The reason why the change could not be submitted.
    """
    self.SendNotification(change,
        '%(queue)s failed to submit your change in %(build_log)s . '
        '%(error)s', error=error)
    self.RemoveCommitReady(change)

  @staticmethod
  def _CreateValidationFailureMessage(pre_cq, change, suspects, messages,
                                      sanity=True):
    """Create a message explaining why a validation failure occurred.

    Args:
      pre_cq: Whether this builder is a Pre-CQ builder.
      change: The change we want to create a message for.
      suspects: The set of suspect changes that we think broke the build.
      messages: A list of build failure messages from supporting builders.
      sanity: A boolean indicating whether the build was considered sane by
              any relevant sanity check builders (True = sane). If not sane,
              none of the changes will have their CommitReady bit modified.
    """
    # Build a list of error messages. We don't want to build a ridiculously
    # long comment, as Gerrit will reject it. See http://crbug.com/236831
    max_error_len = 20000 / max(1, len(messages))
    msg = ['The following build(s) failed:']
    for message in map(str, messages):
      if len(message) > max_error_len:
        message = message[:max_error_len] + '... (truncated)'
      msg.append(message)

    # Create a list of changes other than this one that might be guilty.
    # Limit the number of suspects to 20 so that the list of suspects isn't
    # ridiculously long.
    max_suspects = 20
    other_suspects = suspects - set([change])
    if len(other_suspects) < max_suspects:
      other_suspects_str = ', '.join(sorted(
          'CL:%s' % x.gerrit_number_str for x in other_suspects))
    else:
      other_suspects_str = ('%d other changes. See the blamelist for more '
                            'details.' % (len(other_suspects),))

    will_retry_automatically = False
    if not sanity:
      msg.append('The sanity check builder in this run failed, implying that '
                 'either ToT or the build infrastructure itself was broken '
                 'even without the tested patches. Thus, no changes will be '
                 'blamed for the failure.')
      will_retry_automatically = True
    elif change in suspects:
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

      will_retry_automatically = not pre_cq

    if will_retry_automatically:
      msg.insert(
          0, 'NOTE: The Commit Queue will retry your change automatically.')

    return '\n\n'.join(msg)

  def _ChangeFailedValidation(self, change, messages, suspects, sanity):
    """Handles a validation failure for an individual change.

    Args:
      change: The change to mark as failed.
      messages: A list of build failure messages from supporting builders.
          These must be ValidationFailedMessage objects.
      suspects: The list of changes that are suspected of breaking the build.
      sanity: A boolean indicating whether the build was considered sane by
              any relevant sanity check builders (True = sane). If not sane,
              none of the changes will have their CommitReady bit modified.
    """
    msg = self._CreateValidationFailureMessage(self.pre_cq, change, suspects,
                                               messages, sanity)
    self.SendNotification(change, '%(details)s', details=msg)
    if sanity:
      if change in suspects:
        self.RemoveCommitReady(change)

      # Mark the change as failed. If the Ready bit is still set, the change
      # will be retried automatically.
      self.UpdateCLStatus(self.bot, change, self.STATUS_FAILED,
                          dry_run=self.dryrun)

  def HandleValidationFailure(self, messages, changes=None, sanity=True):
    """Handles a list of validation failure messages from slave builders.

    This handler parses a list of failure messages from our list of builders
    and calculates which changes were likely responsible for the failure. The
    changes that were responsible for the failure have their Commit Ready bit
    stripped and the other changes are left marked as Commit Ready.

    Args:
      messages: A list of build failure messages from supporting builders.
          These must be ValidationFailedMessage objects.
      changes: A list of cros_patch.GerritPatch instances to mark as failed.
        By default, mark all of the changes as failed.
      sanity: A boolean indicating whether the build was considered sane by
              any relevant sanity check builders (True = sane). If not sane,
              none of the changes will have their CommitReady bit modified.
    """
    if changes is None:
      changes = self.changes

    candidates = []
    for change in changes:
      # Ignore changes that were already verified.
      if self.pre_cq and self.GetCLStatus(PRE_CQ, change) == self.STATUS_PASSED:
        continue
      candidates.append(change)

    # First, calculate which changes are likely at fault for the failure.
    suspects = CalculateSuspects.FindSuspects(candidates, messages)

    # Send out failure notifications for each change.
    inputs = [[change, messages, suspects, sanity] for change in candidates]
    parallel.RunTasksInProcessPool(self._ChangeFailedValidation, inputs)

  def GetValidationFailedMessage(self):
    """Returns message indicating these changes failed to be validated."""
    logging.info('Validation failed for all changes.')
    internal = self._overlays in [constants.PRIVATE_OVERLAYS,
                                  constants.BOTH_OVERLAYS]
    details = []
    tracebacks = tuple(results_lib.Results.GetTracebacks())
    for x in tracebacks:
      details.append('The %s stage failed: %s' % (x.failed_stage, x.exception))
    if not details:
      details = ['cbuildbot failed']
    details.append('in %s' % (self.build_log,))
    msg = '%s: %s' % (self._builder_name, ' '.join(details))
    return ValidationFailedMessage(msg, tracebacks, internal)

  def HandleCouldNotApply(self, change):
    """Handler for when Paladin fails to apply a change.

    This handler strips the Commit Ready bit forcing the developer
    to re-upload a rebased change as this theirs failed to apply cleanly.

    Args:
      change: GerritPatch instance to operate upon.
    """
    msg = '%(queue)s failed to apply your change in %(build_log)s . '
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
    self.SendNotification(change, msg)
    self.RemoveCommitReady(change)

  def _HandleApplySuccess(self, change):
    """Handler for when Paladin successfully applies a change.

    This handler notifies a developer that their change is being tried as
    part of a Paladin run defined by a build_log.

    Args:
      change: GerritPatch instance to operate upon.
    """
    if self.pre_cq:
      status = self.GetCLStatus(self.bot, change)
      if status == self.STATUS_PASSED:
        return
    msg = ('%(queue)s has picked up your change. '
           'You can follow along at %(build_log)s .')
    self.SendNotification(change, msg)
    if not self.pre_cq or status == self.STATUS_LAUNCHING:
      self.UpdateCLStatus(self.bot, change, self.STATUS_INFLIGHT,
                          dry_run=self.dryrun)

  @classmethod
  def GetCLStatusURL(cls, bot, change, latest_patchset_only=True):
    """Get the status URL for |change| on |bot|.

    Args:
      bot: Which bot to look at. Can be CQ or PRE_CQ.
      change: GerritPatch instance to operate upon.
      latest_patchset_only: If True, return the URL for tracking the latest
        patchset. If False, return the URL for tracking all patchsets. Defaults
        to True.

    Returns:
      The status URL, as a string.
    """
    internal = 'int' if change.internal else 'ext'
    components = [constants.MANIFEST_VERSIONS_GS_URL, bot,
                  internal, str(change.gerrit_number)]
    if latest_patchset_only:
      components.append(str(change.patch_number))
    return '/'.join(components)

  @classmethod
  def GetCLStatus(cls, bot, change):
    """Get the status for |change| on |bot|.

    Args:
      change: GerritPatch instance to operate upon.
      bot: Which bot to look at. Can be CQ or PRE_CQ.

    Returns:
      The status, as a string.
    """
    url = cls.GetCLStatusURL(bot, change)
    ctx = gs.GSContext()
    try:
      return ctx.Cat(url).output
    except gs.GSNoSuchKey:
      logging.debug('No status yet for %r', url)
      return None

  @classmethod
  def UpdateCLStatus(cls, bot, change, status, dry_run):
    """Update the |status| of |change| on |bot|."""
    for latest_patchset_only in (False, True):
      url = cls.GetCLStatusURL(bot, change, latest_patchset_only)
      ctx = gs.GSContext(dry_run=dry_run)
      ctx.Copy('-', url, input=status)
      ctx.Counter('%s/%s' % (url, status)).Increment()

  @classmethod
  def GetCLStatusCount(cls, bot, change, status, latest_patchset_only=True):
    """Return how many times |change| has been set to |status| on |bot|.

    Args:
      bot: Which bot to look at. Can be CQ or PRE_CQ.
      change: GerritPatch instance to operate upon.
      status: The status string to look for.
      latest_patchset_only: If True, only how many times the latest patchset has
        been set to |status|. If False, count how many times any patchset has
        been set to |status|. Defaults to False.

    Returns:
      The number of times |change| has been set to |status| on |bot|, as an
      integer.
    """
    cache_key = (bot, change, status, latest_patchset_only)
    if cache_key not in cls._CL_STATUS_CACHE:
      base_url = cls.GetCLStatusURL(bot, change, latest_patchset_only)
      url = '%s/%s' % (base_url, status)
      cls._CL_STATUS_CACHE[cache_key] = gs.GSContext().Counter(url).Get()
    return cls._CL_STATUS_CACHE[cache_key]

  @classmethod
  def FillCLStatusCache(cls, bot, changes, statuses=None):
    """Cache all of the stats about the given |changes| in parallel.

    Args:
      bot: Bot to pull down stats for.
      changes: Changes to cache.
      statuses: Statuses to cache. By default, cache the PASSED and FAILED
        counts.
    """
    if statuses is None:
      statuses = (cls.STATUS_PASSED, cls.STATUS_FAILED)
    inputs = []
    for change in changes:
      for status in statuses:
        for latest_patchset_only in (False, True):
          cache_key = (bot, change, status, latest_patchset_only)
          if cache_key not in cls._CL_STATUS_CACHE:
            inputs.append(cache_key)

    with parallel.Manager() as manager:
      # Grab the CL status of all of the CLs in the background, into a proxied
      # dictionary.
      cls._CL_STATUS_CACHE = manager.dict(cls._CL_STATUS_CACHE)
      parallel.RunTasksInProcessPool(cls.GetCLStatusCount, inputs)

      # Convert the cache back into a regular dictionary before we shut down
      # the manager.
      cls._CL_STATUS_CACHE = dict(cls._CL_STATUS_CACHE)

  def CreateDisjointTransactions(self, manifest, max_txn_length=None):
    """Create a list of disjoint transactions from the changes in the pool.

    Args:
      manifest: Manifest to use.
      max_txn_length: The maximum length of any given transaction. Optional.
        By default, do not limit the length of transactions.

    Returns:
      A list of disjoint transactions. Each transaction can be tried
      independently, without involving patches from other transactions.
      Each change in the pool will included in exactly one of transactions,
      unless the patch does not apply for some reason.
    """
    patches = PatchSeries(self.build_root, forced_manifest=manifest)
    plans, failed = patches.CreateDisjointTransactions(
        self.changes, max_txn_length=max_txn_length)
    failed = self._FilterDependencyErrors(failed)
    if failed:
      self._HandleApplyFailure(failed)
    return plans


class PaladinMessage():
  """An object that is used to send messages to developers about their changes.
  """
  # URL where Paladin documentation is stored.
  _PALADIN_DOCUMENTATION_URL = ('http://www.chromium.org/developers/'
                                'tree-sheriffs/sheriff-details-chromium-os/'
                                'commit-queue-overview')

  # Gerrit can't handle commands over 32768 bytes. See http://crbug.com/236831
  MAX_MESSAGE_LEN = 32000

  def __init__(self, message, patch, helper):
    if len(message) > self.MAX_MESSAGE_LEN:
      message = message[:self.MAX_MESSAGE_LEN] + '... (truncated)'
    self.message = message
    self.patch = patch
    self.helper = helper

  def _ConstructPaladinMessage(self):
    """Adds any standard Paladin messaging to an existing message."""
    return self.message + ('\n\nCommit queue documentation: %s' %
                           self._PALADIN_DOCUMENTATION_URL)

  def Send(self, dryrun):
    """Posts a comment to a gerrit review."""
    body = {
        'message': self._ConstructPaladinMessage(),
        'notify': 'OWNER',
    }
    path = 'changes/%s/revisions/%s/review' % (
        self.patch.gerrit_number, self.patch.revision)
    if dryrun:
      logging.info('Would have sent %r to %s', body, path)
      return
    gob_util.FetchUrl(self.helper.host, path, reqtype='POST', body=body)
