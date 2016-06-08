# Copyright (c) 2011-2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module that handles interactions with a Validation Pool.

The validation pool is the set of commits that are ready to be validated i.e.
ready for the commit queue to try.
"""

from __future__ import print_function

import contextlib
import cPickle
import functools
import httplib
import os
import random
import sys
import time
from xml.dom import minidom

from chromite.cbuildbot import config_lib
from chromite.cbuildbot import constants
from chromite.cbuildbot import failures_lib
from chromite.cbuildbot import lkgm_manager
from chromite.cbuildbot import tree_status
from chromite.cbuildbot import triage_lib
from chromite.lib import clactions
from chromite.lib import cros_logging as logging
from chromite.lib import cros_build_lib
from chromite.lib import gerrit
from chromite.lib import git
from chromite.lib import gob_util
from chromite.lib import parallel
from chromite.lib import patch as cros_patch
from chromite.lib import timeout_util


site_config = config_lib.GetConfig()


# Third-party libraries bundled with chromite need to be listed after the
# first chromite import.
import digraph

# We import mox so that w/in ApplyPoolIntoRepo, if a mox exception is
# thrown, we don't cover it up.
try:
  import mox
except ImportError:
  mox = None


PRE_CQ = constants.PRE_CQ
CQ = constants.CQ

CQ_CONFIG = constants.CQ_MASTER
PRE_CQ_LAUNCHER_CONFIG = constants.PRE_CQ_LAUNCHER_CONFIG

# Set of configs that can reject a CL from the pre-CQ / CQ pipeline.
# TODO(davidjames): Any Pre-CQ config can reject CLs now, so this is wrong.
# This is only used for fail counts. Maybe it makes sense to just get rid of
# the fail count?
CQ_PIPELINE_CONFIGS = {CQ_CONFIG, PRE_CQ_LAUNCHER_CONFIG}

# The gerrit-on-borg team tells us that delays up to 2 minutes can be
# normal.  Setting timeout to 3 minutes to be safe-ish.
SUBMITTED_WAIT_TIMEOUT = 3 * 60 # Time in seconds.

MAX_PLAN_RECURSION = 150


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


class FailedToSubmitAllChangesException(failures_lib.StepFailure):
  """Raised if we fail to submit any change."""

  def __init__(self, changes, num_submitted):
    super(FailedToSubmitAllChangesException, self).__init__(
        'FAILED TO SUBMIT ALL CHANGES:  Could not verify that changes %s were '
        'submitted.'
        '\nSubmitted %d changes successfully.' %
        (' '.join(str(c) for c in changes), num_submitted))


class InternalCQError(cros_patch.PatchException):
  """Exception thrown when CQ has an unexpected/unhandled error."""

  def __init__(self, patch, message):
    cros_patch.PatchException.__init__(self, patch, message=message)

  def ShortExplanation(self):
    return 'failed to apply due to a CQ issue: %s' % (self.message,)


class InconsistentReloadException(Exception):
  """Raised if patches applied by the CQ cannot be found anymore."""


class PatchModified(cros_patch.PatchException):
  """Raised if a patch is modified while the CQ is running."""

  def __init__(self, patch, patch_number):
    cros_patch.PatchException.__init__(self, patch)
    self.new_patch_number = patch_number
    self.args = (patch, patch_number)

  def ShortExplanation(self):
    return ('was modified while the CQ was in the middle of testing it. '
            'Patch set %s was uploaded.' % self.new_patch_number)


class PatchRejected(cros_patch.PatchException):
  """Raised if a patch was rejected by the CQ because the CQ failed."""

  def ShortExplanation(self):
    return 'was rejected by the CQ.'


class PatchNotEligible(cros_patch.PatchException):
  """Raised if a patch was not eligible for transaction."""

  def ShortExplanation(self):
    return ('was not eligible (wrong manifest branch, wrong labels, or '
            'otherwise filtered from eligible set).')


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
    return ('could not be submitted because Gerrit reported a conflict. Did '
            'you modify your patch during the CQ run? Or do you just need to '
            'rebase?')


class PatchExceededRecursionLimit(cros_patch.PatchException):
  """Raised if we encountered recursion limit while trying to apply patch."""

  def ShortExplanation(self):
    return ('was part of a dependency stack that exceeded our recursion '
            'depth. Try breaking this stack into smaller pieces.')


# Note: This exception differs slightly in meaning from
# PatchExceededRecursionLimit. That exception is caused by a RuntimeError when
# we hit recursion depth, where as this one is thrown by us before we reach the
# python recursion limit.
class PatchReachedRecursionLimit(cros_patch.PatchException):
  """Raised if we gave up on a too-recursive patch plan."""

  def ShortExplanation(self):
    return ('was part of a dependency stack that reached our recursion '
            'depth limit. Try breaking this stack into smaller pieces.')


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


class GerritHelperNotAvailable(gerrit.GerritException):
  """Exception thrown when a specific helper is requested but unavailable."""

  def __init__(self, remote=site_config.params.EXTERNAL_REMOTE):
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
        site_config.params.EXTERNAL_REMOTE : cros,
        site_config.params.INTERNAL_REMOTE : cros_internal
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
      cros = gerrit.GetGerritHelper(site_config.params.EXTERNAL_REMOTE)
    else:
      cros = None

    if cros_internal:
      cros_internal = gerrit.GetGerritHelper(site_config.params.INTERNAL_REMOTE)
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
  """Class representing a set of patches applied to a repo checkout."""

  def __init__(self, path, helper_pool=None, forced_manifest=None,
               deps_filter_fn=None, is_submitting=False):
    """Constructor.

    Args:
      path: Path to the buildroot.
      helper_pool: Pool of allowed GerritHelpers to be used for fetching
        patches. Defaults to allowing both internal and external fetches.
      forced_manifest: A manifest object to use for mapping projects to
        repositories. Defaults to the buildroot.
      deps_filter_fn: A function which specifies what patches you would
        like to accept. It is passed a patch and is expected to return
        True or False.
      is_submitting: Whether we are currently submitting patchsets. This is
        used to print better error messages.
    """
    self.manifest = forced_manifest

    if helper_pool is None:
      helper_pool = HelperPool.SimpleCreate(cros_internal=True, cros=True)
    self._helper_pool = helper_pool
    self._path = path
    if deps_filter_fn is None:
      deps_filter_fn = lambda x: True
    self.deps_filter_fn = deps_filter_fn
    self._is_submitting = is_submitting

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
  def ApplyChange(self, change):
    # Always enable content merging.
    return change.ApplyAgainstManifest(self.manifest, trivial=False)

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

    Raises:
      Some variety of cros_patch.PatchException if an unsatisfiable required
      dependency is encountered.
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
          raise dep_change.GetMergeException() or PatchNotEligible(dep_change)

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

    Raises:
      DependencyError: If we could not resolve a dependency.
      GerritException or GOBError: If there is a failure in querying gerrit.
    """
    plan = []
    gerrit_deps_seen = cros_patch.PatchCache()
    cq_deps_seen = cros_patch.PatchCache()
    self._AddChangeToPlanWithDeps(change, plan, gerrit_deps_seen,
                                  cq_deps_seen, limit_to=limit_to)
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
        logging.info('Attempting to create transaction for %s', change)
        plan = self.CreateTransaction(change, limit_to=limit_to)
      except cros_patch.PatchException as e:
        yield (change, (), e)
      except RuntimeError as e:
        if 'maximum recursion depth' in e.message:
          yield (change, (), PatchExceededRecursionLimit(change))
        else:
          raise
      else:
        yield (change, plan, None)

  def CreateDisjointTransactions(self, changes, max_txn_length=None,
                                 merge_projects=False):
    """Create a list of disjoint transactions from a list of changes.

    Args:
      changes: A list of cros_patch.GitRepoPatch instances to generate
        transactions for.
      max_txn_length: The maximum length of any given transaction.  By default,
        do not limit the length of transactions.
      merge_projects: If set, put all changes to a given project in the same
        transaction.

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

    if merge_projects:
      projects = {}
      for change in deps:
        projects.setdefault(change.project, []).append(change)
      for project in projects:
        for change in projects[project]:
          edges.setdefault(change, set()).update(projects[project])

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
  def _AddChangeToPlanWithDeps(self, change, plan, gerrit_deps_seen,
                               cq_deps_seen, limit_to=None,
                               include_cq_deps=True,
                               remaining_depth=MAX_PLAN_RECURSION):
    """Add a change and its dependencies into a |plan|.

    Args:
      change: The change to add to the plan.
      plan: The list of changes to apply, in order. This function will append
        |change| and any necessary dependencies to |plan|.
      gerrit_deps_seen: The changes whose Gerrit dependencies have already been
        processed.
      cq_deps_seen: The changes whose CQ-DEPEND and Gerrit dependencies have
        already been processed.
      limit_to: If non-None, limit the allowed uncommitted patches to
        what's in that container/mapping.
      include_cq_deps: If True, include CQ dependencies in the list
        of dependencies. Defaults to True.
      remaining_depth: Amount of permissible recursion depth from this call.

    Raises:
      DependencyError: If we could not resolve a dependency.
      GerritException or GOBError: If there is a failure in querying gerrit.
    """
    if change in self._committed_cache:
      return

    if remaining_depth == 0:
      raise PatchReachedRecursionLimit(change)

    # Get a list of the changes that haven't been committed.
    # These are returned as cros_patch.PatchQuery objects.
    gerrit_deps, cq_deps = self.GetDepsForChange(change)

    # Only process the Gerrit dependencies for each change once. We prioritize
    # Gerrit dependencies over CQ dependencies, since Gerrit dependencies might
    # be required in order for the change to apply.
    old_plan_len = len(plan)
    if change not in gerrit_deps_seen:
      gerrit_deps = self._LookupUncommittedChanges(
          gerrit_deps, limit_to=limit_to)
      gerrit_deps_seen.Inject(change)
      for dep in gerrit_deps:
        self._AddChangeToPlanWithDeps(dep, plan, gerrit_deps_seen, cq_deps_seen,
                                      limit_to=limit_to, include_cq_deps=False,
                                      remaining_depth=remaining_depth - 1)

    if include_cq_deps and change not in cq_deps_seen:
      cq_deps = self._LookupUncommittedChanges(
          cq_deps, limit_to=limit_to)
      cq_deps_seen.Inject(change)
      for dep in plan[old_plan_len:] + cq_deps:
        # Add the requested change (plus deps) to our plan, if it we aren't
        # already in the process of doing that.
        if dep not in cq_deps_seen:
          self._AddChangeToPlanWithDeps(dep, plan, gerrit_deps_seen,
                                        cq_deps_seen, limit_to=limit_to,
                                        remaining_depth=remaining_depth - 1)

    # If there are cyclic dependencies, we might have already applied this
    # patch as part of dependency resolution. If not, apply this patch.
    if change not in plan:
      plan.append(change)

  @_PatchWrapException
  def GetDepChangesForChange(self, change):
    """Look up the Gerrit/CQ dependency changes for |change|.

    Returns:
      (gerrit_deps, cq_deps): The change's Gerrit dependencies and CQ
      dependencies, as lists of GerritPatch objects.

    Raises:
      DependencyError: If we could not resolve a dependency.
      GerritException or GOBError: If there is a failure in querying gerrit.
    """
    gerrit_deps, cq_deps = self.GetDepsForChange(change)

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

    return _DepsToChanges(gerrit_deps), _DepsToChanges(cq_deps)

  @_PatchWrapException
  def GetDepsForChange(self, change):
    """Look up the Gerrit/CQ deps for |change|.

    Returns:
      A tuple of PatchQuery objects representing change's Gerrit
      dependencies, and CQ dependencies.

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
    """Inject into the internal lookup cache the given changes.

    Uses |changes| rather than asking gerrit for them for dependencies.
    """
    self._lookup_cache.Inject(*changes)

  def FetchChanges(self, changes):
    """Fetch the specified changes, if needed.

    If we're an external builder, internal changes are filtered out.

    Returns:
      A list of the filtered changes.
    """
    by_repo = {}
    changes_to_fetch = []
    for change in changes:
      try:
        self._helper_pool.ForChange(change)
      except GerritHelperNotAvailable:
        # Internal patches are irrelevant to external builders.
        logging.info("Skipping internal patch: %s", change)
        continue
      repo = self.GetGitRepoForChange(change, strict=True)
      by_repo.setdefault(repo, []).append(change)
      changes_to_fetch.append(change)

    # Fetch changes in parallel. The change.Fetch() method modifies the
    # 'change' object, so make sure we grab all of that information.
    with parallel.Manager() as manager:
      fetched_changes = manager.dict()

      fetch_repo = functools.partial(
          _FetchChangesForRepo, fetched_changes, by_repo)
      parallel.RunTasksInProcessPool(fetch_repo, [[repo] for repo in by_repo])

      return [fetched_changes[c.id] for c in changes_to_fetch]

  def ReapplyChanges(self, by_repo):
    """Make sure that all of the local changes still apply.

    Syncs all of the repositories in the manifest and reapplies the changes on
    top of the tracking branch for each repository.

    Args:
      by_repo: A mapping from repo paths to changes in that repo.

    Returns:
      a new by_repo dict containing only the patches that apply correctly, and
      errors, a dict of patches to exceptions encountered while applying them.
    """
    self.ResetCheckouts(constants.PATCH_BRANCH, fetch=True)
    local_changes = reduce(set.union, by_repo.values(), set())
    applied_changes, failed_tot, failed_inflight = self.Apply(local_changes)
    errors = {}
    for exception in failed_tot + failed_inflight:
      errors[exception.patch] = exception

    # Filter out only the changes that applied.
    by_repo = dict(by_repo)
    for repo in by_repo:
      by_repo[repo] &= set(applied_changes)

    return by_repo, errors

  @_ManifestDecorator
  def ResetCheckouts(self, branch, fetch=False):
    """Updates |branch| in all Git checkouts in the manifest to their remotes.

    Args:
      branch: The branch to update.
      fetch: Indicates whether to sync the remotes before resetting.
    """
    if not self.manifest:
      logging.info("No manifest, skipping reset.")
      return

    def _Reset(checkout):
      path = checkout.GetPath()

      # There is no need to reset the branch if it doesn't exist.
      if not git.DoesCommitExistInRepo(path, branch):
        return

      if fetch:
        git.RunGit(path, ['fetch', '--all'])

      def _LogBranch():
        branches = git.RunGit(path, ['branch', '-vv']).output.splitlines()
        branch_line = [b for b in branches if branch in b]
        logging.info(branch_line)

      _LogBranch()
      git.RunGit(path, ['checkout', '-f', branch])
      logging.info('Resetting to %s', checkout['tracking_branch'])
      git.RunGit(path, ['reset', checkout['tracking_branch'], '--hard'])
      _LogBranch()

    parallel.RunTasksInProcessPool(
        _Reset,
        [[c] for c in self.manifest.ListCheckouts()])

  @_ManifestDecorator
  def Apply(self, changes, frozen=True, honor_ordering=False,
            changes_filter=None):
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
    changes = self.FetchChanges(changes)
    if changes_filter:
      changes = changes_filter(self, changes)

    self.InjectLookupCache(changes)
    limit_to = cros_patch.PatchCache(changes) if frozen else None
    resolved, applied, failed = [], [], []
    planned = set()
    for change, plan, ex in self.CreateTransactions(changes, limit_to=limit_to):
      if ex is not None:
        logging.info("Failed creating transaction for %s: %s", change, ex)
        failed.append(ex)
      else:
        resolved.append((change, plan))
        logging.info("Transaction for %s is %s.",
                     change, ', '.join(map(str, resolved[-1][-1])))
        planned.update(plan)

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
        change, plan = data
        ids = [x.id for x in plan]
        return -len(ids), position[change]
      resolved.sort(key=mk_key)

    for inducing_change, transaction_changes in resolved:
      try:
        with self._Transaction(transaction_changes):
          logging.debug("Attempting transaction for %s: changes: %s",
                        inducing_change,
                        ', '.join(map(str, transaction_changes)))
          self._ApplyChanges(inducing_change, transaction_changes)
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
    except Exception:
      logging.info("Rewinding transaction: failed changes: %s .",
                   ', '.join(map(str, commits)), exc_info=True)

      for project_dir, sha1 in resets:
        git.RunGit(project_dir, ['reset', '--hard', sha1])

      self._committed_cache = committed_cache
      raise

  @_PatchWrapException
  def _ApplyChanges(self, _inducing_change, changes):
    """Apply a given ordered sequence of changes.

    Args:
      _inducing_change: The core GitRepoPatch instance that lead to this
        sequence of changes; basically what this transaction was computed from.
        Needs to be passed in so that the exception wrapping machinery can
        convert any failures, assigning blame appropriately.
      manifest: A ManifestCheckout instance representing what we're working on.
      changes: A ordered sequence of GitRepoPatch instances to apply.
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
        self.ApplyChange(change)
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
        note forced_manifest cannot be used here.

    Returns:
      A PatchSeries instance w/ a forced manifest.
    """

    if 'forced_manifest' in kwargs:
      raise ValueError("RawPatchSeries doesn't allow a forced_manifest "
                       "argument.")
    kwargs['forced_manifest'] = _ManifestShim(git_repo, tracking_branch)

    return cls(git_repo, **kwargs)


def _FetchChangesForRepo(fetched_changes, by_repo, repo):
  """Fetch the changes for a given `repo`.

  Args:
    fetched_changes: A dict from change ids to changes which is updated by
      this method.
    by_repo: A mapping from repositories to changes.
    repo: The repository we should fetch the changes for.
  """
  changes = by_repo[repo]
  refs = set(c.ref for c in changes if not c.HasBeenFetched(repo))
  cmd = ['fetch', '-f', changes[0].project_url] + list(refs)
  git.RunGit(repo, cmd, print_cmd=True)

  for change in changes:
    sha1 = change.HasBeenFetched(repo) or change.sha1
    change.UpdateMetadataFromRepo(repo, sha1=sha1)
    fetched_changes[change.id] = change


class _ManifestShim(object):
  """A fake manifest that only contains a single repository.

  This fake manifest is used to allow us to filter out patches for
  the PatchSeries class. It isn't a complete implementation -- we just
  implement the functions that PatchSeries uses. It works via duck typing.

  All of the below methods accept the same arguments as the corresponding
  methods in git.ManifestCheckout.*, but they do not make any use of the
  arguments -- they just always return information about this project.
  """

  def __init__(self, path, tracking_branch, remote='origin'):

    tracking_branch = 'refs/remotes/%s/%s' % (
        remote, git.StripRefs(tracking_branch),
    )
    attrs = dict(local_path=path, path=path, tracking_branch=tracking_branch)
    self.checkout = git.ProjectCheckout(attrs)

  def FindCheckouts(self, *_args, **_kwargs):
    """Returns the list of checkouts.

    In this case, we only have one repository so we just return that repository.
    We accept the same arguments as git.ManifestCheckout.FindCheckouts, but we
    do not make any use of them.

    Returns:
      A list of ProjectCheckout objects.
    """
    return [self.checkout]


class ValidationPool(object):
  """Class that handles interactions with a validation pool.

  This class can be used to acquire a set of commits that form a pool of
  commits ready to be validated and committed.

  Usage:  Use ValidationPool.AcquirePool -- a static
  method that grabs the commits that are ready for validation.
  """

  # Global variable to control whether or not we should allow CL's to get tried
  # and/or committed when the tree is throttled.
  # TODO(sosa): Remove this global once metrics show that this is the direction
  # we want to go (and remove all additional throttled_ok logic from this
  # module.
  THROTTLED_OK = True
  GLOBAL_DRYRUN = False
  DEFAULT_TIMEOUT = 60 * 60 * 4
  # How long to wait when the tree is throttled before checking for CR+1 CL's.
  CQ_THROTTLED_TIMEOUT = 60 * 10
  SLEEP_TIMEOUT = 30
  # Buffer time to leave when using the global build deadline as the sync stage
  # timeout. We need some time to possibly extend the global build deadline
  # after the sync timeout is hit.
  EXTENSION_TIMEOUT_BUFFER = 10 * 60
  INCONSISTENT_SUBMIT_MSG = ('Gerrit thinks that the change was not submitted, '
                             'even though we hit the submit button.')

  # The grace period (in seconds) before we reject a patch due to dependency
  # errors.
  REJECTION_GRACE_PERIOD = 30 * 60

  # How many CQ runs to go back when making a decision about the CQ health.
  # Note this impacts the max exponential fallback (2^10=1024 max exponential
  # divisor)
  CQ_SEARCH_HISTORY = 10


  def __init__(self, overlays, build_root, build_number, builder_name,
               is_master, dryrun, candidates=None, non_os_changes=None,
               conflicting_changes=None, pre_cq_trybot=False,
               tree_was_open=True, applied=None, builder_run=None):
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
      candidates: List of changes to consider validating.
      non_os_changes: List of changes that are part of this validation
        pool but aren't part of the cros checkout.
      conflicting_changes: Changes that failed to apply but we're keeping around
        because they conflict with other changes in flight.
      pre_cq_trybot: If set to True, this is a Pre-CQ trybot. (Note: The Pre-CQ
        launcher is NOT considered a Pre-CQ trybot.)
      tree_was_open: Whether the tree was open when the pool was created.
      applied: List of CLs that have been applied to the current repo.
      builder_run: BuilderRun instance used to fetch cidb handle and metadata
        instance. Please note due to the pickling logic, this MUST be the last
        kwarg listed.
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
        ('candidates', candidates),
        ('non_os_changes', non_os_changes),
        ('applied', applied)):
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

    self.is_master = bool(is_master)
    self.pre_cq_trybot = pre_cq_trybot
    self._run = builder_run
    self.dryrun = bool(dryrun) or self.GLOBAL_DRYRUN
    if pre_cq_trybot:
      self.queue = 'A trybot'
    elif builder_name == constants.PRE_CQ_LAUNCHER_NAME:
      self.queue = 'The Pre-Commit Queue'
    else:
      self.queue = 'The Commit Queue'

    # See optional args for types of changes.
    self.candidates = candidates or []
    self.non_manifest_changes = non_os_changes or []
    self.applied = applied or []

    # Note, we hold onto these CLs since they conflict against our current CLs
    # being tested; if our current ones succeed, we notify the user to deal
    # w/ the conflict.  If the CLs we're testing fail, then there is no
    # reason we can't try these again in the next run.
    self.changes_that_failed_to_apply_earlier = conflicting_changes or []

    # Private vars only used for pickling and self._build_log.
    self._overlays = overlays
    self._build_number = build_number
    self._builder_name = builder_name

    # Set to False if the tree was not open when we acquired changes.
    self.tree_was_open = tree_was_open

  @property
  def build_log(self):
    if self._run:
      return tree_status.ConstructDashboardURL(
          self._run.GetBuildbotUrl(), self._builder_name, self._build_number)

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
    # NOTE: self._run is specifically excluded from the validation pool
    # pickle. We do not want the un-pickled validation pool to have a reference
    # to its own un-pickled BuilderRun instance. Instead, we want to to refer
    # to the new builder run's metadata instance. This is accomplished by
    # setting the BuilderRun at un-pickle time, in ValidationPool.Load(...).
    return (
        self.__class__,
        (
            self._overlays,
            self.build_root, self._build_number, self._builder_name,
            self.is_master, self.dryrun, self.candidates,
            self.non_manifest_changes,
            self.changes_that_failed_to_apply_earlier,
            self.pre_cq_trybot,
            self.tree_was_open,
            self.applied))

  @classmethod
  @failures_lib.SetFailureType(failures_lib.BuilderFailure)
  def AcquirePreCQPool(cls, *args, **kwargs):
    """See ValidationPool.__init__ for arguments."""
    kwargs.setdefault('tree_was_open', True)
    kwargs.setdefault('pre_cq_trybot', True)
    kwargs.setdefault('is_master', True)
    kwargs.setdefault('applied', [])
    pool = cls(*args, **kwargs)
    return pool

  @staticmethod
  def _WaitForQuery(query):
    """Helper method to return msg to print out when waiting for a |query|."""
    # Dictionary that maps CQ Queries to msg's to display.
    if query == constants.CQ_READY_QUERY:
      return 'new CLs'
    elif query == constants.THROTTLED_CQ_READY_QUERY:
      return 'new CQ+2 CLs or the tree to open'
    else:
      return 'waiting for tree to open'

  def AcquireChanges(self, gerrit_query, ready_fn, change_filter):
    """Helper method for AcquirePool. Adds changes to pool based on args.

    Queries gerrit using the given flags, filters out any unwanted changes, and
    handles draft changes.

    Args:
      gerrit_query: gerrit query to use.
      ready_fn: CR function (see constants).
      change_filter: If set, filters with change_filter(pool, changes,
        non_manifest_changes) to remove unwanted patches.
    """
    # Iterate through changes from all gerrit instances we care about.
    for helper in self._helper_pool:
      changes = helper.Query(gerrit_query, sort='lastUpdated')
      changes.reverse()

      if ready_fn:
        # The query passed in may include a dictionary of flags to use for
        # revalidating the query results. We need to do this because Gerrit
        # caches are sometimes stale and need sanity checking.
        changes = [x for x in changes if ready_fn(x)]

      # Tell users to publish drafts before marking them commit ready.
      for change in changes:
        if change.HasApproval('COMR', ('1', '2')) and change.IsDraft():
          self.HandleDraftChange(change)

      changes, non_manifest_changes = ValidationPool._FilterNonCrosProjects(
          changes, git.ManifestCheckout.Cached(self.build_root))
      self.candidates.extend(changes)
      self.non_manifest_changes.extend(non_manifest_changes)

    # Filter out unwanted changes.
    unfiltered_str = cros_patch.GetChangesAsString(self.candidates)
    self.candidates, self.non_manifest_changes = change_filter(
        self, self.candidates, self.non_manifest_changes)
    if self.candidates:
      filtered_str = cros_patch.GetChangesAsString(self.candidates)
      logging.info('Raw changes: %s', unfiltered_str)
      logging.info('Filtered changes: %s', filtered_str)

    return self.candidates or self.non_manifest_changes

  @classmethod
  def AcquirePool(cls, overlays, repo, build_number, builder_name, query,
                  dryrun=False, check_tree_open=True,
                  change_filter=None, builder_run=None):
    """Acquires the current pool from Gerrit.

    Polls Gerrit and checks for which changes are ready to be committed.
    Should only be called from master builders.

    Args:
      overlays: One of constants.VALID_OVERLAYS.
      repo: The repo used to sync, to filter projects, and to apply patches
        against.
      build_number: Corresponding build number for the build.
      builder_name: Builder name on buildbot dashboard.
      query: constants.CQ_READY_QUERY, PRECQ_READY_QUERY, or a custom
        query description of the form (<query_str>, None).
      dryrun: Don't submit anything to gerrit.
      check_tree_open: If True, only return when the tree is open.
      change_filter: If set, use change_filter(pool, changes,
        non_manifest_changes) to filter out unwanted patches.
      builder_run: instance used to record CL actions to metadata and cidb.

    Returns:
      ValidationPool object.

    Raises:
      TreeIsClosedException: if the tree is closed (or throttled, if not
                             |THROTTLED_OK|).
    """
    if change_filter is None:
      change_filter = lambda _, x, y: (x, y)

    # We choose a longer wait here as we haven't committed to anything yet. By
    # doing this here we can reduce the number of builder cycles.
    timeout = cls.DEFAULT_TIMEOUT
    if builder_run is not None:
      build_id, db = builder_run.GetCIDBHandle()
      if db:
        time_to_deadline = db.GetTimeToDeadline(build_id)
        if time_to_deadline is not None:
          # We must leave enough time before the deadline to allow us to extend
          # the deadline in case we hit this timeout.
          timeout = time_to_deadline - cls.EXTENSION_TIMEOUT_BUFFER

    end_time = time.time() + timeout
    # How long to wait until if the tree is throttled and we want to be more
    # accepting of changes. We leave it as end_time whenever the tree is open.
    tree_throttled_time = end_time
    status = constants.TREE_OPEN

    while True:
      current_time = time.time()
      time_left = end_time - current_time
      # Wait until the tree becomes open.
      if check_tree_open:
        try:
          status = tree_status.WaitForTreeStatus(
              period=cls.SLEEP_TIMEOUT, timeout=time_left,
              throttled_ok=cls.THROTTLED_OK)
          # Manages the timer for accepting CL's >= CR+1 based on tree status.
          # If the tree is not open.
          if status == constants.TREE_OPEN:
            # Reset the timer in case it was changed.
            tree_throttled_time = end_time
          elif tree_throttled_time == end_time:
            # Tree not open and tree_throttled_time not set.
            tree_throttled_time = current_time + cls.CQ_THROTTLED_TIMEOUT
        except timeout_util.TimeoutError:
          raise TreeIsClosedException(
              closed_or_throttled=not cls.THROTTLED_OK)

      # Sync so that we are up-to-date on what is committed.
      repo.Sync()

      # Determine the query to use.
      gerrit_query, ready_fn = query
      tree_was_open = True
      if (status == constants.TREE_THROTTLED and
          query == constants.CQ_READY_QUERY):
        if current_time < tree_throttled_time:
          gerrit_query, ready_fn = constants.THROTTLED_CQ_READY_QUERY
        else:
          # Note we only apply the tree not open logic after a given
          # window.
          tree_was_open = False
          gerrit_query, ready_fn = constants.CQ_READY_QUERY

      pool = ValidationPool(overlays, repo.directory, build_number,
                            builder_name, True, dryrun, builder_run=builder_run,
                            tree_was_open=tree_was_open, applied=[])

      if pool.AcquireChanges(gerrit_query, ready_fn, change_filter):
        break

      if dryrun or time_left < 0 or cls.ShouldExitEarly():
        break

      # Iterated through all queries with no changes.
      logging.info('Waiting for %s (%d minutes left)...',
                   cls._WaitForQuery(query), time_left / 60)
      time.sleep(cls.SLEEP_TIMEOUT)

    return pool

  def _GetFailStreak(self):
    """Returns the fail streak for the validation pool.

    Queries CIDB for the last CQ_SEARCH_HISTORY builds from the current build_id
    and returns how many of them haven't passed in a row. This is used for
    tree throttled validation pool logic.
    """
    # TODO(sosa): Remove Google Storage Fail Streak Counter.
    build_id, db = self._run.GetCIDBHandle()
    if not db:
      return 0

    builds = db.GetBuildHistory(self._run.config.name,
                                ValidationPool.CQ_SEARCH_HISTORY,
                                ignore_build_id=build_id)
    number_of_failures = 0
    # Iterate through the ordered list of builds until you get one that is
    # passed.
    for build in builds:
      if build['status'] != constants.BUILDER_STATUS_PASSED:
        number_of_failures += 1
      else:
        break

    return number_of_failures

  def AddPendingCommitsIntoPool(self, manifest):
    """Add the pending commits from |manifest| into pool.

    Args:
      manifest: path to the manifest.
    """
    manifest_dom = minidom.parse(manifest)
    pending_commits = manifest_dom.getElementsByTagName(
        lkgm_manager.PALADIN_COMMIT_ELEMENT)
    for pc in pending_commits:
      attr_names = cros_patch.ALL_ATTRS
      attr_dict = {}
      for name in attr_names:
        attr_dict[name] = pc.getAttribute(name)
      patch = cros_patch.GerritFetchOnlyPatch.FromAttrDict(attr_dict)

      self.candidates.append(patch)

  @classmethod
  def AcquirePoolFromManifest(cls, manifest, overlays, repo, build_number,
                              builder_name, is_master, dryrun,
                              builder_run=None):
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
      builder_run: BuilderRun instance used to record CL actions to metadata and
        cidb.

    Returns:
      ValidationPool object.
    """
    pool = ValidationPool(overlays, repo.directory, build_number, builder_name,
                          is_master, dryrun, builder_run=builder_run,
                          applied=[])
    pool.AddPendingCommitsIntoPool(manifest)
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
      return (change.project.startswith('chromiumos/') or
              change.project.startswith('chromeos/') or
              change.project.startswith('aosp/'))

    # First we filter to only Chromium OS repositories.
    changes = [c for c in changes if IsCrosReview(c)]

    changes_in_manifest = []
    changes_not_in_manifest = []
    for change in changes:
      if change.GetCheckout(manifest, strict=False):
        changes_in_manifest.append(change)
      elif change.IsMergeable():
        logging.info('Found non-manifest change %s', change)
        changes_not_in_manifest.append(change)
      else:
        logging.info('Non-manifest change %s is not commit ready yet', change)

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
      is_ready = error.patch.HasReadyFlag()
      if not is_ready or reject_timestamp < error.patch.approval_timestamp:
        while error is not None:
          if isinstance(error, cros_patch.DependencyError):
            if is_ready:
              logging.info('Ignoring dependency errors for %s due to grace '
                           'period', error.patch)
            else:
              logging.info('Ignoring dependency errors for %s until it is '
                           'marked trybot ready or commit ready', error.patch)
            results.pop()
            break
          error = getattr(error, 'error', None)
    return results

  @classmethod
  def PrintLinksToChanges(cls, changes):
    """Print links to the specified |changes|.

    This method prints a link to list of |changes| by using the
    information stored in |changes|. It should not attempt to query
    Google Storage or Gerrit.

    Args:
      changes: A list of cros_patch.GerritPatch instances to generate
        transactions for.
    """
    def SortKeyForChanges(change):
      return (-change.total_fail_count, -change.fail_count,
              os.path.basename(change.project), change.gerrit_number)

    # Now, sort and print the changes.
    for change in sorted(changes, key=SortKeyForChanges):
      project = os.path.basename(change.project)
      gerrit_number = cros_patch.AddPrefix(change, change.gerrit_number)
      author = change.owner
      # Show the owner, unless it's a non-standard email address.
      if (change.owner_email and
          not (change.owner_email.endswith(constants.GOOGLE_EMAIL) or
               change.owner_email.endswith(constants.CHROMIUM_EMAIL))):
        # We cannot print '@' in the link because it is used to separate
        # the display text and the URL by the buildbot annotator.
        author = change.owner_email.replace('@', '-AT-')

      s = '%s | %s | %s' % (project, author, gerrit_number)

      # Print a count of how many times a given CL has failed the CQ.
      if change.total_fail_count:
        s += ' | fails:%d' % (change.fail_count,)
        if change.total_fail_count > change.fail_count:
          s += '(%d)' % (change.total_fail_count,)

      # Add a note if the latest patchset has already passed the CQ.
      if change.pass_count > 0:
        s += ' | passed:%d' % change.pass_count

      logging.PrintBuildbotLink(s, change.url)

  def FilterChangesForThrottledTree(self):
    """Apply Throttled Tree logic to select patch candidates.

    This should be called before any CLs are applied.

    If the tree is throttled, we only test a random subset of our candidate
    changes. Call this to select that subset, and throw away unrelated changes.

    If the three was open when this pool was created, it does nothing.
    """
    if self.tree_was_open:
      return

    # By filtering the candidates before any calls to Apply, we can make sure
    # that repeated calls to Apply always consider the same list of candidates.
    fail_streak = self._GetFailStreak()
    test_pool_size = max(1, len(self.candidates) / (2**fail_streak))
    random.shuffle(self.candidates)
    self.candidates = self.candidates[:test_pool_size]

  def ApplyPoolIntoRepo(self, manifest=None, filter_fn=lambda p: True):
    """Applies changes from pool into the directory specified by the buildroot.

    This method applies changes in the order specified. If the build
    is running as the master, it also respects the dependency
    order. Otherwise, the changes should already be listed in an order
    that will not break the dependency order.

    It is safe to call this method more than once, probably with different
    filter functions. A given patch will never be applied more than  once.

    Args:
      manifest: A manifest object to use for mapping projects to repositories.
      filter_fn: Takes a patch argument, returns bool for 'should apply'.

    Returns:
      True if we managed to apply any changes.
    """
    # applied is a list of applied GerritPatch instances.
    # failed_tot and failed_inflight are lists of PatchException instances.
    applied = []
    failed_tot = []
    failed_inflight = []
    patch_series = PatchSeries(self.build_root, helper_pool=self._helper_pool)

    if self.is_master:
      try:
        candidates = [c for c in self.candidates if
                      c not in self.applied and filter_fn(c)]

        # pylint: disable=E1123
        applied, failed_tot, failed_inflight = patch_series.Apply(
            candidates, manifest=manifest)
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
        links = cros_patch.GetChangesAsString(self.candidates)
        logging.error('%s\nAffected Patches are: %s', msg, links)
        errors = [InternalCQError(patch, msg) for patch in self.candidates]
        self._HandleApplyFailure(errors)
        raise

      _, db = self._run.GetCIDBHandle()
      if db:
        action_history = db.GetActionsForChanges(applied)
        for change in applied:
          change.total_fail_count = clactions.GetCLActionCount(
              change, CQ_PIPELINE_CONFIGS, constants.CL_ACTION_KICKED_OUT,
              action_history, latest_patchset_only=False)
          change.fail_count = clactions.GetCLActionCount(
              change, CQ_PIPELINE_CONFIGS, constants.CL_ACTION_KICKED_OUT,
              action_history)
          change.pass_count = clactions.GetCLActionCount(
              change, CQ_PIPELINE_CONFIGS, constants.CL_ACTION_SUBMIT_FAILED,
              action_history)

    else:
      # Slaves do not need to create transactions and should simply
      # apply the changes serially, based on the order that the
      # changes were listed on the manifest.
      for change in self.candidates:
        try:
          # pylint: disable=E1123
          patch_series.ApplyChange(change, manifest=manifest)
        except cros_patch.PatchException as e:
          # Fail if any patch cannot be applied.
          self._HandleApplyFailure([InternalCQError(change, e)])
          raise
        else:
          applied.append(change)

    self.RecordPatchesInMetadataAndDatabase(applied)
    self.PrintLinksToChanges(applied)

    if self.is_master and not self.pre_cq_trybot:
      inputs = [[change, self.build_log] for change in applied]
      parallel.RunTasksInProcessPool(self.HandleApplySuccess, inputs)

    # We only filter out dependency errors in the CQ and Pre-CQ masters.
    # On Pre-CQ trybots, we want to reject patches immediately, because
    # otherwise the pre-cq master will think we just dropped the patch
    # on the floor and never tested it.
    if not self.pre_cq_trybot:
      failed_tot = self._FilterDependencyErrors(failed_tot)
      failed_inflight = self._FilterDependencyErrors(failed_inflight)

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
      for x in failed_inflight:
        self._HandleFailedToApplyDueToInflightConflict(x.patch)

    self.changes_that_failed_to_apply_earlier.extend(failed_inflight)
    self.applied.extend(applied)

    return bool(self.applied)

  @staticmethod
  def Load(filename, builder_run=None):
    """Loads the validation pool from the file.

    Args:
      filename: path of file to load from.
      builder_run: BuilderRun instance to use in unpickled validation pool, used
        for fetching cidb handle for access to metadata.
    """
    with open(filename, 'rb') as p_file:
      pool = cPickle.load(p_file)
      # pylint: disable=protected-access
      pool._run = builder_run
      return pool

  def Save(self, filename):
    """Serializes the validation pool."""
    with open(filename, 'wb') as p_file:
      cPickle.dump(self, p_file, protocol=cPickle.HIGHEST_PROTOCOL)

  # Note: All submit code, all gerrit code, and basically everything other
  # than patch resolution/applying needs to use .change_id from patch objects.
  # Basically all code from this point forward.
  def _SubmitChangeWithDeps(self, patch_series, change, errors, limit_to,
                            reason=None):
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
      reason: string reason for submission to be recorded in cidb. (Should be
        None or constant with name STRATEGY_* from constants.py)

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
      errors[change] = e
      return errors

    submitted = []
    dep_error = None
    for dep_change in plan:
      # Has this change failed to submit before?
      dep_error = errors.get(dep_change)
      if dep_error is not None:
        break

    if dep_error is None:
      for dep_change in plan:
        try:
          success = self._SubmitChangeUsingGerrit(dep_change, reason=reason)
          if success or self.dryrun:
            submitted.append(dep_change)
        except (gob_util.GOBError, gerrit.GerritException) as e:
          if getattr(e, 'http_status', None) == httplib.CONFLICT:
            if e.message.rstrip().endswith('change is merged'):
              submitted.append(dep_change)
            else:
              dep_error = PatchConflict(dep_change)
          else:
            dep_error = PatchFailedToSubmit(dep_change, str(e))

        if dep_change not in submitted:
          if dep_error is None:
            msg = self.INCONSISTENT_SUBMIT_MSG
            dep_error = PatchFailedToSubmit(dep_change, msg)

          # Log any errors we saw.
          logging.error('%s', dep_error)
          errors[dep_change] = dep_error
          break

    if (dep_error is not None and change not in errors and
        change not in submitted):
      # One of the dependencies failed to submit. Report an error.
      errors[change] = cros_patch.DependencyError(change, dep_error)

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
          break

    return errors

  def SubmitChanges(self, verified_cls, check_tree_open=True,
                    throttled_ok=True):
    """Submits the given changes to Gerrit.

    Args:
      verified_cls: A dictionary mapping the fully verified changes to their
        string reasons for submission.
      check_tree_open: Whether to check that the tree is open before submitting
        changes. If this is False, TreeIsClosedException will never be raised.
      throttled_ok: if |check_tree_open|, treat a throttled tree as open

    Returns:
      (submitted, errors) where submitted is a set of changes that were
      submitted, and errors is a map {change: error} containing changes that
      failed to submit.

    Raises:
      TreeIsClosedException: if the tree is closed.
    """
    assert self.is_master, 'Non-master builder calling SubmitPool'
    assert not self.pre_cq_trybot, 'Trybot calling SubmitPool'

    # TODO(pprabhu) It is bad form for master-paladin to do work after its
    # deadline has passed. Extend the deadline after waiting for slave
    # completion and ensure that none of the follow up stages go beyond the
    # deadline.
    if (check_tree_open and not self.dryrun and not
        tree_status.IsTreeOpen(period=self.SLEEP_TIMEOUT,
                               timeout=self.DEFAULT_TIMEOUT,
                               throttled_ok=throttled_ok)):
      raise TreeIsClosedException(
          closed_or_throttled=not throttled_ok)

    changes = verified_cls.keys()
    # Filter out changes that were modified during the CQ run.
    filtered_changes, errors = self.FilterModifiedChanges(changes)

    patch_series = PatchSeries(self.build_root, helper_pool=self._helper_pool,
                               is_submitting=True)

    patch_series.InjectLookupCache(filtered_changes)

    # Partition the changes into local changes and remote changes.  Local
    # changes have a local repository associated with them, so we can do a
    # batched git push for them.  Remote changes must be submitted via Gerrit.
    by_repo_cls = {}
    for change in filtered_changes:
      by_repo_cls.setdefault(
          patch_series.GetGitRepoForChange(change, strict=False), set()
          ).add(change)
    remote_changes = {c:verified_cls[c] for c in by_repo_cls.pop(None, set())}

    by_repo_cls, reapply_errors = patch_series.ReapplyChanges(by_repo_cls)

    # Map the changes in by_repo_cls to their submission reasons.
    by_repo = dict()
    for repo, cls in by_repo_cls.iteritems():
      by_repo[repo] = {cl:verified_cls[cl] for cl in cls}

    submitted_locals, local_submission_errors = self.SubmitLocalChanges(
        by_repo)
    submitted_remotes, remote_errors = self.SubmitRemoteChanges(
        patch_series, remote_changes)

    errors.update(reapply_errors)
    errors.update(local_submission_errors)
    errors.update(remote_errors)
    for patch, error in errors.iteritems():
      logging.error('Could not submit %s', patch)
      self._HandleCouldNotSubmit(patch, error)

    return submitted_locals | submitted_remotes, errors

  def SubmitRemoteChanges(self, patch_series, verified_cls):
    """Submits non-manifest changes via Gerrit.

    This function first splits the patches into disjoint transactions so that we
    can submit in parallel. We merge together changes to the same project into
    the same transaction because it helps avoid Gerrit performance problems
    (Gerrit chokes when two people hit submit at the same time in the same
    project).

    Args:
      patch_series: The PatchSeries instance associated with the changes.
      verified_cls: A dictionary mapping changes to their submission reasons.

    Returns:
      (submitted, errors) where submitted is a set of changes that were
      submitted, and errors is a map {change: error} containing changes that
      failed to submit.
    """
    changes = verified_cls.keys()
    plans, failed = patch_series.CreateDisjointTransactions(
        changes, merge_projects=True)
    errors = {}
    for error in failed:
      errors[error.patch] = error

    # Submit each disjoint transaction in parallel.
    with parallel.Manager() as manager:
      p_errors = manager.dict(errors)
      def _SubmitPlan(*plan):
        for change in plan:
          p_errors.update(self._SubmitChangeWithDeps(
              patch_series, change, dict(p_errors),
              plan, reason=verified_cls[change]))
      parallel.RunTasksInProcessPool(_SubmitPlan, plans, processes=4)

      submitted_changes = set(changes) - set(p_errors.keys())
      return (submitted_changes, dict(p_errors))

  def SubmitLocalChanges(self, by_repo):
    """Submit a set of local changes, i.e. changes which are in the manifest.

    Precondition: we must have already checked that all the changes are
    submittable, such as having a +2 in Gerrit.

    Args:
      by_repo: A mapping from repo paths to a dictionary contains changes to
        that repo and their corresponding submission reasons.

    Returns:
      (submitted, errors) where submitted is a set of changes that were
      submitted, and errors is a map {change: error} containing changes that
      failed to submit.
    """
    merged_errors = {}
    submitted = set()
    for repo, verified_cls in by_repo.iteritems():
      changes, errors = self._SubmitRepo(repo, verified_cls)
      submitted |= set(changes)
      merged_errors.update(errors)
    return submitted, merged_errors

  def _SubmitRepo(self, repo, verified_cls):
    """Submit a sequence of changes from the same repository.

    The changes must be from a repository that is checked out locally, we can do
    a single git push, and then verify that Gerrit updated its metadata for each
    patch.

    Args:
      repo: the path to the repository containing the changes
      verified_cls: a dictionary mapping changes from a single repository to
        their submission reasons.

    Returns:
      (submitted, errors) where submitted is a set of changes that were
      submitted, and errors is a map {change: error} containing changes that
      failed to submit.
    """
    changes = verified_cls.keys()
    branches = set((change.tracking_branch,) for change in changes)
    push_branch = functools.partial(self.PushRepoBranch, repo, changes)
    push_results = parallel.RunTasksInProcessPool(push_branch, branches)

    sha1s = {}
    errors = {}
    for sha1s_for_branch, branch_errors in filter(bool, push_results):
      sha1s.update(sha1s_for_branch)
      errors.update(branch_errors)

    for change in changes:
      push_success = change not in errors
      self._CheckChangeWasSubmitted(change, push_success,
                                    reason=verified_cls[change],
                                    sha1=sha1s.get(change))

    return set(changes) - set(errors), errors

  def PushRepoBranch(self, repo, changes, branch):
    """Pushes a branch of a repo to the remote.

    Args:
      repo: the path to the repository containing the changes
      changes: a sequence of changes from a single branch of a repository.
      branch: the tracking branch name.

    Returns:
      (sha1, errors) where sha1s is a mapping from changes to their sha1s, and
      errors is a map {change: error} containing changes that failed to submit.
    """

    project_url = next(iter(changes)).project_url
    remote_ref = git.GetTrackingBranch(repo)
    push_to = git.RemoteRef(project_url, branch)
    for _ in range(3):
      # try to resync and push.
      try:
        git.SyncPushBranch(repo, remote_ref.remote, remote_ref.ref)
      except cros_build_lib.RunCommandError:
        # TODO(phobbs) parse the sync failure output and find which change was
        # at fault.
        logging.error('git rebase failed for %s:%s; it is likely that a change '
                      'was chumped in the middle of the CQ run.',
                      repo, branch, exc_info=True)
        break

      try:
        git.GitPush(repo, 'HEAD', push_to, skip=self.dryrun)
        return {}
      except cros_build_lib.RunCommandError:
        logging.warn('git push failed for %s:%s; was a change chumped in the '
                     'middle of the CQ run?',
                     repo, branch, exc_info=True)

    errors = dict(
        (change, PatchFailedToSubmit(change, 'Failed to push to %s'))
        for change in changes)

    sha1s = dict(
        (change, change.GetLocalSHA1(repo, branch))
        for change in changes)

    return sha1s, errors

  def RecordPatchesInMetadataAndDatabase(self, changes):
    """Mark all patches as having been picked up in metadata.json and cidb.

    If self._run is None, then this function does nothing.
    """
    if not self._run:
      return

    metadata = self._run.attrs.metadata
    _, db = self._run.GetCIDBHandle()
    timestamp = int(time.time())

    for change in changes:
      metadata.RecordCLAction(change, constants.CL_ACTION_PICKED_UP,
                              timestamp)
      # TODO(akeshet): If a separate query for each insert here becomes
      # a performance issue, consider batch inserting all the cl actions
      # with a single query.
      if db:
        self._InsertCLActionToDatabase(change, constants.CL_ACTION_PICKED_UP)

  @classmethod
  def FilterModifiedChanges(cls, changes):
    """Filter out changes that were modified while the CQ was in-flight.

    Args:
      changes: A list of changes (as PatchQuery objects).

    Returns:
      This returns a tuple (unmodified_changes, errors).

      unmodified_changes: A reloaded list of changes, only including mergeable,
                          unmodified and unsubmitted changes.
      errors: A dictionary. This dictionary will contain all patches that have
        encountered errors, and map them to the associated exception object.
    """
    # Reload all of the changes from the Gerrit server so that we have a
    # fresh view of their approval status. This is needed so that our filtering
    # that occurs below will be mostly up-to-date.
    unmodified_changes, errors = [], {}
    reloaded_changes = list(cls.ReloadChanges(changes))
    old_changes = cros_patch.PatchCache(changes)

    if list(changes) != list(reloaded_changes):
      logging.error('Changes: %s', map(str, changes))
      logging.error('Reloaded changes: %s', map(str, reloaded_changes))
      for change in set(changes) - set(reloaded_changes):
        logging.error('%s disappeared after reloading', change)
      for change in set(reloaded_changes) - set(changes):
        logging.error('%s appeared after reloading', change)
      raise InconsistentReloadException()

    for reloaded_change in reloaded_changes:
      old_change = old_changes[reloaded_change]
      if reloaded_change.IsAlreadyMerged():
        logging.warning('%s is already merged. It was most likely chumped '
                        'during the current CQ run.', reloaded_change)
      elif reloaded_change.patch_number != old_change.patch_number:
        # If users upload new versions of a CL while the CQ is in-flight, then
        # their CLs are no longer tested. These CLs should be rejected.
        errors[old_change] = PatchModified(reloaded_change,
                                           reloaded_change.patch_number)
      elif not reloaded_change.IsMergeable():
        # Get the reason why this change is not mergeable anymore.
        errors[old_change] = reloaded_change.GetMergeException()
        errors[old_change].patch = old_change
      else:
        unmodified_changes.append(old_change)

    return unmodified_changes, errors

  @classmethod
  def ReloadChanges(cls, changes):
    """Reload the specified |changes| from the server.

    Args:
      changes: A list of PatchQuery objects.

    Returns:
      A list of GerritPatch objects.
    """
    return gerrit.GetGerritPatchInfoWithPatchQueries(changes)

  def _SubmitChangeUsingGerrit(self, change, reason=None):
    """Submits patch using Gerrit Review.

    This uses the Gerrit "submit" API, then waits for the patch to move out of
    "NEW" state, ideally into "MERGED" status.  It records in CIDB whether the
    Gerrit's status != "NEW".

    Args:
      change: GerritPatch to submit.
      reason: string reason for submission to be recorded in cidb. (Should be
        None or constant with name STRATEGY_* from constants.py)

    Returns:
      Whether the push succeeded, indicated by Gerrit review status not being
      "NEW".
    """
    logging.info('Change %s will be submitted', change)
    self._helper_pool.ForChange(change).SubmitChange(
        change, dryrun=self.dryrun)
    return self._CheckChangeWasSubmitted(change, True, reason)

  def _CheckChangeWasSubmitted(self, change, push_success, reason, sha1=None):
    """Confirms that a change is in "submitted" state in Gerrit.

    First, we force Gerrit to double-check whether the change has been merged,
    then we poll Gerrit until either the change is merged or we timeout. Then,
    we update cidb with information about whether the change was pushed
    successfully.

    Args:
      change: The change to check
      push_success: Whether we were successful in pushing the change.
      reason: string reason for submission to be recorded in cidb. (Should be
        None or constant with name STRATEGY_* from constants.py)
      sha1: Optional hint to Gerrit about what sha1 the pushed commit has.

    Returns:
      Whether the push succeeded and the Gerrit review is not in "NEW" state.
      Ideally it would be in "MERGED" state, but it is safe to proceed with it
      only in "SUBMITTED" state.
    """
    # TODO(phobbs): Use a helper process to check that Gerrit marked the change
    # as merged asynchronously.
    helper = self._helper_pool.ForChange(change)

    # Force Gerrit to check whether the change is merged.
    gob_util.CheckChange(helper.host, change.gerrit_number, sha1=sha1)

    updated_change = helper.QuerySingleRecord(change.gerrit_number)
    if push_success and updated_change.status == 'SUBMITTED':
      def _Query():
        return helper.QuerySingleRecord(change.gerrit_number)
      def _Retry(value):
        return value and value.status == 'SUBMITTED'

      # If we succeeded in pushing but the change is 'NEW' give gerrit some time
      # to resolve that to 'MERGED' or fail outright.
      try:
        updated_change = timeout_util.WaitForSuccess(
            _Retry, _Query, timeout=SUBMITTED_WAIT_TIMEOUT, period=1)
      except timeout_util.TimeoutError:
        # The change really is stuck on submitted, not merged, then.
        logging.warning('Timed out waiting for gerrit to notice that we'
                        ' submitted change %s, but status is still "%s".',
                        change.gerrit_number_str, updated_change.status)
        helper.SetReview(change, msg='This change was pushed, but we timed out'
                         'waiting for Gerrit to notice that it was submitted.')

    if push_success and not updated_change.status == 'MERGED':
      logging.warning(
          'Change %s was pushed without errors, but gerrit is'
          ' reporting it with status "%s" (expected "MERGED").',
          change.gerrit_number_str, updated_change.status)
      if updated_change.status == 'SUBMITTED':
        # So far we have never seen a SUBMITTED CL that did not eventually
        # transition to MERGED.  If it is stuck on SUBMITTED treat as MERGED.
        logging.info('Proceeding now with the assumption that change %s'
                     ' will eventually transition to "MERGED".',
                     change.gerrit_number_str)
      else:
        logging.error('Gerrit likely was unable to merge change %s.',
                      change.gerrit_number_str)

    succeeded = push_success and (updated_change.status != 'NEW')
    if self._run:
      self._RecordSubmitInCIDB(change, succeeded, reason)
    return succeeded

  def _RecordSubmitInCIDB(self, change, succeeded, reason):
    """Records in CIDB whether the submit succeeded."""
    action = (constants.CL_ACTION_SUBMITTED if succeeded
              else constants.CL_ACTION_SUBMIT_FAILED)

    metadata = self._run.attrs.metadata
    timestamp = int(time.time())
    metadata.RecordCLAction(change, action, timestamp)
    _, db = self._run.GetCIDBHandle()
    # NOTE(akeshet): The same |reason| will be recorded, regardless of whether
    # the change was submitted successfully or unsuccessfully. This is
    # probably what we want, because it gives us a way to determine why we
    # tried to submit changes that failed to submit.
    if db:
      self._InsertCLActionToDatabase(change, action, reason)

  def RemoveReady(self, change, reason=None):
    """Remove the commit ready and trybot ready bits for |change|."""
    self._helper_pool.ForChange(change).RemoveReady(change, dryrun=self.dryrun)
    if self._run:
      metadata = self._run.attrs.metadata
      timestamp = int(time.time())
      metadata.RecordCLAction(change, constants.CL_ACTION_KICKED_OUT,
                              timestamp)

    self._InsertCLActionToDatabase(change, constants.CL_ACTION_KICKED_OUT,
                                   reason)
    if self.pre_cq_trybot:
      self.UpdateCLPreCQStatus(change, constants.CL_STATUS_FAILED)

  def MarkForgiven(self, change, reason=None):
    """Mark |change| as forgiven with |reason|.

    Args:
      change: A GerritPatch or GerritPatchTuple object.
      reason: string reason for submission to be recorded in cidb. (Should be
        None or constant with name STRATEGY_* from constants.py)
    """
    self._InsertCLActionToDatabase(change, constants.CL_ACTION_FORGIVEN, reason)

  def _InsertCLActionToDatabase(self, change, action, reason=None):
    """If cidb is set up and not None, insert given cl action to cidb.

    Args:
      change: A GerritPatch or GerritPatchTuple object.
      action: The action taken, should be one of constants.CL_ACTIONS
      reason: string reason for submission to be recorded in cidb. (Should be
        None or constant with name STRATEGY_* from constants.py)
    """
    build_id, db = self._run.GetCIDBHandle()
    if db:
      db.InsertCLActions(
          build_id,
          [clactions.CLAction.FromGerritPatchAndAction(change, action, reason)])

  def SubmitNonManifestChanges(self, check_tree_open=True, reason=None):
    """Commits changes to Gerrit from Pool that aren't part of the checkout.

    Args:
      check_tree_open: Whether to check that the tree is open before submitting
        changes. If this is False, TreeIsClosedException will never be raised.
      reason: string reason for submission to be recorded in cidb. (Should be
        None or constant with name STRATEGY_* from constants.py)

    Raises:
      TreeIsClosedException: if the tree is closed.
    """
    verified_cls = {c:reason for c in self.non_manifest_changes}
    self.SubmitChanges(verified_cls,
                       check_tree_open=check_tree_open)

  def SubmitPool(self, check_tree_open=True, throttled_ok=True, reason=None):
    """Commits changes to Gerrit from Pool.  This is only called by a master.

    Args:
      check_tree_open: Whether to check that the tree is open before submitting
        changes. If this is False, TreeIsClosedException will never be raised.
      throttled_ok: if |check_tree_open|, treat a throttled tree as open
      reason: string reason for submission to be recorded in cidb. (Should be
        None or constant with name STRATEGY_* from constants.py)

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
    verified_cls = {c:reason for c in self.applied}
    submitted, errors = self.SubmitChanges(verified_cls,
                                           check_tree_open=check_tree_open,
                                           throttled_ok=throttled_ok)
    if errors:
      raise FailedToSubmitAllChangesException(errors, len(submitted))

    if self.changes_that_failed_to_apply_earlier:
      self._HandleApplyFailure(self.changes_that_failed_to_apply_earlier)

  def SubmitPartialPool(self, changes, messages, changes_by_config,
                        subsys_by_config, failing, inflight, no_stat):
    """If the build failed, push any CLs that don't care about the failure.

    In this function we calculate what CLs are definitely innocent and submit
    those CLs.

    Each project can specify a list of stages it does not care about in its
    COMMIT-QUEUE.ini file. Changes to that project will be submitted even if
    those stages fail. Or if unignored fail stage is only HWTestStage, submit
    changes that are unrelated to the failed hardware subsystems.

    Args:
      changes: A list of GerritPatch instances to examine.
      messages: A list of BuildFailureMessage or NoneType objects from
        the failed slaves.
      changes_by_config: A dictionary of relevant changes indexed by the
        config names.
      subsys_by_config: A dictionary of pass/fail HWTest subsystems indexed
        by the config names.
      failing: Names of the builders that failed.
      inflight: Names of the builders that timed out.
      no_stat: Set of builder names of slave builders that had status None.

    Returns:
      A set of the non-submittable changes.
    """
    fully_verified = triage_lib.CalculateSuspects.GetFullyVerifiedChanges(
        changes, changes_by_config, subsys_by_config, failing, inflight,
        no_stat, messages, self.build_root)
    fully_verified_cls = fully_verified.keys()
    if fully_verified_cls:
      logging.info('The following changes will be submitted using '
                   'board-aware submission logic: %s',
                   cros_patch.GetChangesAsString(fully_verified_cls))
    self.SubmitChanges(fully_verified)

    # Return the list of non-submittable changes.
    return set(changes) - set(fully_verified_cls)

  def _HandleApplyFailure(self, failures):
    """Handles changes that were not able to be applied cleanly.

    Args:
      failures: List of cros_patch.PatchException instances to handle.
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
      failure: cros_patch.PatchException instance to operate upon.
    """
    msg = ('%(queue)s failed to apply your change in %(build_log)s .'
           ' %(failure)s')
    self.SendNotification(failure.patch, msg, failure=failure)
    self.RemoveReady(failure.patch)

  def _HandleIncorrectSubmission(self, failure):
    """Handler for when Paladin incorrectly submits a change."""
    msg = ('%(queue)s incorrectly submitted your change in %(build_log)s .'
           '  %(failure)s')
    self.SendNotification(failure.patch, msg, failure=failure)
    self.RemoveReady(failure.patch)

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
    self.RemoveReady(change)

  def _HandleFailedToApplyDueToInflightConflict(self, change):
    """Handler for when a patch conflicts with another patch in the CQ run.

    This handler simply comments on the affected change, explaining why it
    is being skipped in the current CQ run.

    Args:
      change: GerritPatch instance to operate upon.
    """
    msg = ('%(queue)s could not apply your change because it conflicts with '
           'other change(s) that it is testing. If those changes do not pass '
           'your change will be retried. Otherwise it will be rejected at '
           'the end of this CQ run.')
    self.SendNotification(change, msg)

  def HandleValidationTimeout(self, changes=None, sanity=True):
    """Handles changes that timed out.

    If sanity is set, then this handler removes the commit ready bit
    from infrastructure changes and sends the developer a message explaining
    why.

    Args:
      changes: A list of cros_patch.GerritPatch instances to mark as failed.
        By default, mark all of the changes as failed.
      sanity: A boolean indicating whether the build was considered sane. If
        not sane, none of the changes will have their CommitReady bit modified.
    """
    if changes is None:
      changes = self.applied

    logging.info('Validation timed out for all changes.')
    base_msg = ('%(queue)s timed out while verifying your change in '
                '%(build_log)s . This means that a supporting builder did not '
                'finish building your change within the specified timeout.')

    blamed = triage_lib.CalculateSuspects.FilterChangesForInfraFail(changes)

    for change in changes:
      logging.info('Validation timed out for change %s.', change)
      if sanity and change in blamed:
        msg = ('%s If you believe this happened in error, just re-mark your '
               'commit as ready. Your change will then get automatically '
               'retried.' % base_msg)
        self.SendNotification(change, msg)
        self.RemoveReady(change)
      else:
        msg = ('NOTE: The Commit Queue will retry your change automatically.'
               '\n\n'
               '%s The build failure may have been caused by infrastructure '
               'issues, so your change will not be blamed for the failure.'
               % base_msg)
        self.SendNotification(change, msg)
        self.MarkForgiven(change)

  def SendNotification(self, change, msg, **kwargs):
    if not kwargs.get('build_log'):
      kwargs['build_log'] = self.build_log
    kwargs.setdefault('queue', self.queue)
    d = dict(**kwargs)
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

  def HandlePreCQSuccess(self, changes):
    """Handler that is called when |changes| passed all pre-cq configs."""
    msg = ('%(queue)s has successfully verified your change.')
    def ProcessChange(change):
      self.SendNotification(change, msg)

    inputs = [[change] for change in changes]
    parallel.RunTasksInProcessPool(ProcessChange, inputs)

  def HandlePreCQPerConfigSuccess(self):
    """Handler that is called when a pre-cq tryjob verifies a change."""
    def ProcessChange(change):
      # Note: This function has no unit test coverage. Be careful when
      # modifying.
      if self._run:
        metadata = self._run.attrs.metadata
        timestamp = int(time.time())
        metadata.RecordCLAction(change, constants.CL_ACTION_VERIFIED,
                                timestamp)
        self._InsertCLActionToDatabase(change, constants.CL_ACTION_VERIFIED)

    # Process the changes in parallel.
    inputs = [[change] for change in self.applied]
    parallel.RunTasksInProcessPool(ProcessChange, inputs)

  def _HandleCouldNotSubmit(self, change, error=''):
    """Handler that is called when Paladin can't submit a change.

    This should be rare, but if an admin overrides the commit queue and commits
    a change that conflicts with this change, it'll apply, build/validate but
    receive an error when submitting.

    Args:
      change: GerritPatch instance to operate upon.
      error: The reason why the change could not be submitted.
    """
    self.SendNotification(
        change,
        '%(queue)s failed to submit your change in %(build_log)s . '
        '%(error)s', error=error)
    self.RemoveReady(change)

  @staticmethod
  def _CreateValidationFailureMessage(pre_cq_trybot, change, suspects, messages,
                                      sanity=True, infra_fail=False,
                                      lab_fail=False, no_stat=None,
                                      retry=False):
    """Create a message explaining why a validation failure occurred.

    Args:
      pre_cq_trybot: Whether the builder is a Pre-CQ trybot. (Note: The Pre-CQ
        launcher is NOT considered a Pre-CQ trybot.)
      change: The change we want to create a message for.
      suspects: The set of suspect changes that we think broke the build.
      messages: A list of build failure messages from supporting builders.
        These must be BuildFailureMessage objects or NoneType objects.
      sanity: A boolean indicating whether the build was considered sane. If
        not sane, none of the changes will have their CommitReady bit modified.
      infra_fail: The build failed purely due to infrastructure failures.
      lab_fail: The build failed purely due to test lab infrastructure failures.
      no_stat: A list of builders which failed prematurely without reporting
        status.
      retry: Whether we should retry automatically.

    Returns:
      A string that communicates what happened.
    """
    msg = []
    if no_stat:
      msg.append('The following build(s) did not start or failed prematurely:')
      msg.append(', '.join(no_stat))

    if messages:
      # Build a list of error messages. We don't want to build a ridiculously
      # long comment, as Gerrit will reject it. See http://crbug.com/236831
      max_error_len = 20000 / max(1, len(messages))
      msg.append('The following build(s) failed:')
      for message in map(str, messages):
        if len(message) > max_error_len:
          message = message[:max_error_len] + '... (truncated)'
        msg.append(message)

    # Create a list of changes other than this one that might be guilty.
    # Limit the number of suspects to 20 so that the list of suspects isn't
    # ridiculously long.
    max_suspects = 20
    other_suspects = set(suspects) - set([change])
    if len(other_suspects) < max_suspects:
      other_suspects_str = cros_patch.GetChangesAsString(other_suspects)
    else:
      other_suspects_str = ('%d other changes. See the blamelist for more '
                            'details.' % (len(other_suspects),))

    if not sanity:
      msg.append('The build was consider not sane because the sanity check '
                 'builder(s) failed. Your change will not be blamed for the '
                 'failure.')
      assert retry
    elif lab_fail:
      msg.append('The build encountered Chrome OS Lab infrastructure issues. '
                 ' Your change will not be blamed for the failure.')
      assert retry
    else:
      if infra_fail:
        msg.append('The build failure may have been caused by infrastructure '
                   'issues and/or bad %s changes.' % constants.INFRA_PROJECTS)

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
        elif len(suspects) > 0:
          msg.append('One of the following changes is probably at fault: %s'
                     % other_suspects_str)

        assert retry

    if retry:
      bot = 'The Pre-Commit Queue' if pre_cq_trybot else 'The Commit Queue'
      msg.insert(0, 'NOTE: %s will retry your change automatically.' % bot)

    return '\n\n'.join(msg)

  def _ChangeFailedValidation(self, change, messages, suspects, sanity,
                              infra_fail, lab_fail, no_stat):
    """Handles a validation failure for an individual change.

    Args:
      change: The change to mark as failed.
      messages: A list of build failure messages from supporting builders.
          These must be BuildFailureMessage objects.
      suspects: The list of changes that are suspected of breaking the build.
      sanity: A boolean indicating whether the build was considered sane. If
        not sane, none of the changes will have their CommitReady bit modified.
      infra_fail: The build failed purely due to infrastructure failures.
      lab_fail: The build failed purely due to test lab infrastructure failures.
      no_stat: A list of builders which failed prematurely without reporting
        status.
    """
    retry = not sanity or lab_fail or change not in suspects
    msg = self._CreateValidationFailureMessage(
        self.pre_cq_trybot, change, suspects, messages,
        sanity, infra_fail, lab_fail, no_stat, retry)
    self.SendNotification(change, '%(details)s', details=msg)
    if retry:
      self.MarkForgiven(change)
    else:
      self.RemoveReady(change)

  def HandleValidationFailure(self, messages, changes=None, sanity=True,
                              no_stat=None):
    """Handles a list of validation failure messages from slave builders.

    This handler parses a list of failure messages from our list of builders
    and calculates which changes were likely responsible for the failure. The
    changes that were responsible for the failure have their Commit Ready bit
    stripped and the other changes are left marked as Commit Ready.

    Args:
      messages: A list of build failure messages from supporting builders.
          These must be BuildFailureMessage objects or NoneType objects.
      changes: A list of cros_patch.GerritPatch instances to mark as failed.
        By default, mark all of the changes as failed.
      sanity: A boolean indicating whether the build was considered sane. If
        not sane, none of the changes will have their CommitReady bit modified.
      no_stat: A list of builders which failed prematurely without reporting
        status. If not None, this implies there were infrastructure issues.
    """
    if changes is None:
      changes = self.applied

    candidates = []

    if self.pre_cq_trybot:
      _, db = self._run.GetCIDBHandle()
      action_history = []
      if db:
        action_history = db.GetActionsForChanges(changes)

      for change in changes:
        # Don't reject changes that have already passed the pre-cq.
        pre_cq_status = clactions.GetCLPreCQStatus(
            change, action_history)
        if pre_cq_status == constants.CL_STATUS_PASSED:
          continue
        candidates.append(change)
    else:
      candidates.extend(changes)

    # Determine the cause of the failures and the changes that are likely at
    # fault for the failure.
    lab_fail = triage_lib.CalculateSuspects.OnlyLabFailures(messages, no_stat)
    infra_fail = triage_lib.CalculateSuspects.OnlyInfraFailures(
        messages, no_stat)
    suspects = triage_lib.CalculateSuspects.FindSuspects(
        candidates, messages, infra_fail=infra_fail, lab_fail=lab_fail,
        sanity=sanity)

    # Send out failure notifications for each change.
    inputs = [[change, messages, suspects, sanity, infra_fail,
               lab_fail, no_stat] for change in candidates]
    parallel.RunTasksInProcessPool(self._ChangeFailedValidation, inputs)

  def HandleApplySuccess(self, change, build_log=None):
    """Handler for when Paladin successfully applies (picks up) a change.

    This handler notifies a developer that their change is being tried as
    part of a Paladin run defined by a build_log.

    Args:
      change: GerritPatch instance to operate upon.
      action_history: List of CLAction instances.
      build_log: The URL to the build log.
    """
    msg = ('%(queue)s has picked up your change. '
           'You can follow along at %(build_log)s .')
    self.SendNotification(change, msg, build_log=build_log)

  def UpdateCLPreCQStatus(self, change, status):
    """Update the pre-CQ |status| of |change|."""
    action = clactions.TranslatePreCQStatusToAction(status)
    self._InsertCLActionToDatabase(change, action)

  def CreateDisjointTransactions(self, manifest, changes, max_txn_length=None):
    """Create a list of disjoint transactions from the changes in the pool.

    Args:
      manifest: Manifest to use.
      changes: List of changes to use.
      max_txn_length: The maximum length of any given transaction.  By default,
        do not limit the length of transactions.

    Returns:
      A list of disjoint transactions. Each transaction can be tried
      independently, without involving patches from other transactions.
      Each change in the pool will included in exactly one of transactions,
      unless the patch does not apply for some reason.
    """
    patches = PatchSeries(self.build_root, forced_manifest=manifest)
    plans, failed = patches.CreateDisjointTransactions(
        changes, max_txn_length=max_txn_length)
    failed = self._FilterDependencyErrors(failed)
    if failed:
      self._HandleApplyFailure(failed)
    return plans

  def RecordIrrelevantChanges(self, changes):
    """Records |changes| irrelevant to the slave build into cidb.

    Args:
      changes: A set of irrelevant changes to record.
    """
    if changes:
      logging.info('The following changes are irrelevant to this build: %s',
                   cros_patch.GetChangesAsString(changes))
    else:
      logging.info('All changes are considered relevant to this build.')

    for change in changes:
      self._InsertCLActionToDatabase(change,
                                     constants.CL_ACTION_IRRELEVANT_TO_SLAVE)


class PaladinMessage(object):
  """Object used to send messages to developers about their changes."""

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
