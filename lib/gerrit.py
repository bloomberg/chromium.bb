# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing helper class and methods for interacting with Gerrit."""

import operator

from chromite.buildbot import constants
from chromite.lib import cros_build_lib
from chromite.lib import git
from chromite.lib import gob_util
from chromite.lib import patch as cros_patch

gob_util.LOGGER = cros_build_lib.logger


class GerritException(Exception):
  """Base exception, thrown for gerrit failures"""


class QueryHasNoResults(GerritException):
  """Exception thrown when a query returns no results."""


class QueryNotSpecific(GerritException):
  """Thrown when a query needs to identify one CL, but matched multiple."""


class FailedToReachGerrit(GerritException):
  """Exception thrown if we failed to contact the Gerrit server."""


class GerritHelper(object):
  """Helper class to manage interaction with the gerrit-on-borg service."""

  # Maximum number of results to return per query.
  _GERRIT_MAX_QUERY_RETURN = 500

  # Fields that appear in gerrit change query results.
  MORE_CHANGES = '_more_changes'
  SORTKEY = '_sortkey'

  def __init__(self, host, remote, print_cmd=True):
    """Initialize.

    Args:
      host: Hostname (without protocol prefix) of the gerrit server.
      remote: The symbolic name of a known remote git host,
          taken from buildbot.contants.
      print_cmd: Determines whether all RunCommand invocations will be echoed.
          Set to False for quiet operation.
    """
    self.host = host
    self.remote = remote
    self.print_cmd = bool(print_cmd)
    self._version = None

  @classmethod
  def FromRemote(cls, remote, **kwargs):
    if remote == constants.INTERNAL_REMOTE:
      host = constants.INTERNAL_GERRIT_HOST
    elif remote == constants.EXTERNAL_REMOTE:
      host = constants.EXTERNAL_GERRIT_HOST
    else:
      raise ValueError('Remote %s not supported.' % remote)
    return cls(host, remote, **kwargs)

  @classmethod
  def FromGob(cls, gob, **kwargs):
    """Return a helper for a GoB instance."""
    host = constants.GOB_HOST % ('%s-review' % gob)
    return cls(host, gob, **kwargs)

  def SetReviewers(self, change, add=(), remove=()):
    """Modify the list of reviewers on a gerrit change.

    Args:
      change: ChangeId or change number for a gerrit review.
      add: Sequence of email addresses of reviewers to add.
      remove: Sequence of email addresses of reviewers to remove.
    """
    if add:
      gob_util.AddReviewers(self.host, change, add)
    if remove:
      gob_util.RemoveReviewers(self.host, change, remove)

  def GetChangeDetail(self, change_num):
    """Return detailed information about a gerrit change.

    Args:
      change_num: A gerrit change number.
    """
    return gob_util.GetChangeDetail(
        self.host, change_num, o_params=('CURRENT_REVISION', 'CURRENT_COMMIT'))

  def GrabPatchFromGerrit(self, project, change, commit, must_match=True):
    """Return a cros_patch.GerritPatch representing a gerrit change.

    Args:
      project: The name of the gerrit project for the change.
      change: A ChangeId or gerrit number for the change.
      commit: The git commit hash for a patch associated with the change.
      must_match: Raise an exception if the change is not found.
    """
    query = { 'project': project, 'commit': commit, 'must_match': must_match }
    return self.QuerySingleRecord(change, **query)

  def IsChangeCommitted(self, change, must_match=False):
    """Check whether a gerrit change has been merged.

    Args:
      change: A gerrit change number.
      must_match: Raise an exception if the change is not found.  If this is
          False, then a missing change will return None.
    """
    change = gob_util.GetChange(self.host, change)
    if not change:
      if must_match:
        raise QueryHasNoResults('Could not query for change %s' % change)
      return
    return change.get('status') == 'MERGED'

  def GetLatestSHA1ForBranch(self, project, branch):
    """Return the git hash at the tip of a branch."""
    url = '%s://%s/%s' % (gob_util.GERRIT_PROTOCOL, self.host, project)
    cmd = ['ls-remote', url, 'refs/heads/%s' % branch]
    try:
      result = git.RunGit('.', cmd, print_cmd=self.print_cmd)
      if result:
        return result.output.split()[0]
    except cros_build_lib.RunCommandError:
      cros_build_lib.Error('Command "%s" failed.', cros_build_lib.CmdToStr(cmd),
                           exc_info=True)

  def QuerySingleRecord(self, change=None, **kwargs):
    """Free-form query of a gerrit change that expects a single result.

    Args:
      change: A gerrit change number.
      **kwargs:
        dryrun: Don't query the gerrit server; just return None.
        must_match: Raise an exception if the query comes back empty.  If this
            is False, an unsatisfied query will return None.
        Refer to Query() docstring for remaining arguments.

    Returns:
      If kwargs['raw'] == True, return a python dict representing the
      change; otherwise, return a cros_patch.GerritPatch object.
    """
    query_kwds = kwargs
    dryrun = query_kwds.get('dryrun')
    must_match = query_kwds.pop('must_match', True)
    results = self.Query(change, **query_kwds)
    if dryrun:
      return None
    elif not results:
      if must_match:
        raise QueryHasNoResults('Query %s had no results' % (change,))
      return None
    elif len(results) != 1:
      raise QueryNotSpecific('Query %s returned too many results: %s'
                             % (change, results))
    return results[0]

  def Query(self, change=None, sort=None, current_patch=True, options=(),
            dryrun=False, raw=False, sortkey=None, **kwargs):
    """Free-form query for gerrit changes.

    Args:
      change: ChangeId, git commit hash, or gerrit number for a change.
      sort: A functor to extract a sort key from a cros_patch.GerritChange
          object, for sorting results..  If this is None, results will not be
          sorted.
      current_patch: If True, ask the gerrit server for extra information about
          the latest uploaded patch.
      options: Deprecated.
      dryrun: If True, don't query the gerrit server; return an empty list.
      raw: If True, return a list of python dict's representing the query
          results.  Otherwise, return a list of cros_patch.GerritPatch.
      sortkey: For continuation queries, this should be the '_sortkey' field
          extracted from the previous batch of results.
      kwargs: A dict of query parameters, as described here:
        https://gerrit-review.googlesource.com/Documentation/rest-api-changes.html#list-changes

    Returns:
      A list of python dicts or cros_patch.GerritChange.
    """
    query_kwds = kwargs
    if options:
      raise GerritException('"options" argument unsupported on gerrit-on-borg.')
    url_prefix = gob_util.GetGerritFetchUrl(self.host)
    # All possible params are documented at
    # pylint: disable=C0301
    # https://gerrit-review.googlesource.com/Documentation/rest-api-changes.html#list-changes
    o_params = ['DETAILED_ACCOUNTS', 'ALL_REVISIONS', 'DETAILED_LABELS']
    if current_patch:
      o_params.extend(['CURRENT_COMMIT', 'CURRENT_REVISION'])

    if change and cros_patch.ParseGerritNumber(change) and not query_kwds:
      if dryrun:
        cros_build_lib.Info('Would have run gob_util.GetChangeDetail(%s, %s)',
                            self.host, change)
        return []
      change = self.GetChangeDetail(change)
      if change is None:
        return []
      patch_dict = cros_patch.GerritPatch.ConvertQueryResults(change, self.host)
      if raw:
        return [patch_dict]
      return [cros_patch.GerritPatch(patch_dict, self.remote, url_prefix)]

    # TODO: We should allow querying using a cros_patch.PatchQuery
    # object directly.
    if change and cros_patch.ParseSHA1(change):
      # Use commit:sha1 for accurate query results (crbug.com/358381).
      kwargs['commit'] = change
      change = None
    elif change and cros_patch.ParseChangeID(change):
      # Use change:change-id for accurate query results (crbug.com/357876).
      kwargs['change'] = change
      change = None
    elif change and cros_patch.ParseFullChangeID(change):
      project, branch, change_id = cros_patch.ParseFullChangeID(change)
      kwargs['change'] = change_id
      kwargs['project'] = project
      kwargs['branch'] = branch
      change = None

    if change and query_kwds.get('change'):
      raise GerritException('Bad query params: provided a change-id-like query,'
                            ' and a "change" search parameter')

    if dryrun:
      cros_build_lib.Info(
          'Would have run gob_util.QueryChanges(%s, %s, first_param=%s, '
          'limit=%d)', self.host, repr(query_kwds), change,
          self._GERRIT_MAX_QUERY_RETURN)
      return []

    moar = gob_util.QueryChanges(
        self.host, query_kwds, first_param=change, sortkey=sortkey,
        limit=self._GERRIT_MAX_QUERY_RETURN, o_params=o_params)
    result = list(moar)
    while moar and self.MORE_CHANGES in moar[-1]:
      if self.SORTKEY not in moar[-1]:
        raise GerritException(
            'Gerrit query has more results, but is missing _sortkey field.')
      sortkey = moar[-1][self.SORTKEY]
      moar = gob_util.QueryChanges(
          self.host, query_kwds, first_param=change, sortkey=sortkey,
          limit=self._GERRIT_MAX_QUERY_RETURN, o_params=o_params)
      result.extend(moar)

    # NOTE: Query results are served from the gerrit cache, which may be stale.
    # To make sure the patch information is accurate, re-request each query
    # result directly, circumventing the cache.  For reference:
    #   https://code.google.com/p/chromium/issues/detail?id=302072
    result = [self.GetChangeDetail(x['_number']) for x in result]

    result = [cros_patch.GerritPatch.ConvertQueryResults(
        x, self.host) for x in result]
    if sort:
      result = sorted(result, key=operator.itemgetter(sort))
    if raw:
      return result
    return [cros_patch.GerritPatch(x, self.remote, url_prefix) for x in result]

  def QueryMultipleCurrentPatchset(self, changes):
    """Query the gerrit server for multiple changes.

    Args:
      changes: A sequence of gerrit change numbers.

    Returns:
      A list of cros_patch.GerritPatch.
    """
    if not changes:
      return
    url_prefix = gob_util.GetGerritFetchUrl(self.host)
    for change in changes:
      change_detail = self.GetChangeDetail(change)
      if not change_detail:
        raise GerritException('Change %s not found on server %s.'
                              % (change, self.host))
      patch_dict = cros_patch.GerritPatch.ConvertQueryResults(
          change_detail, self.host)
      yield change, cros_patch.GerritPatch(patch_dict, self.remote, url_prefix)

  @staticmethod
  def _to_changenum(change):
    """Unequivocally return a gerrit change number.

    The argument may either be an number, which will be returned unchanged;
    or an instance of GerritPatch, in which case its gerrit number will be
    returned.
    """
    if isinstance(change, cros_patch.GerritPatch):
      return change.gerrit_number

    return change

  def SetReview(self, change, msg=None, labels=None, dryrun=False):
    """Update the review labels on a gerrit change.

    Args:
      change: A gerrit change number.
      msg: A text comment to post to the review.
      labels: A dict of label/value to set on the review.
      dryrun: If True, don't actually update the review.
    """
    if not msg and not labels:
      return
    if dryrun:
      if msg:
        cros_build_lib.Info('Would have add message "%s" to change "%s".',
                            msg, change)
      if labels:
        for key, val in labels.iteritems():
          cros_build_lib.Info(
              'Would have set label "%s" to "%s" for change "%s".',
              key, val, change)
      return
    gob_util.SetReview(self.host, self._to_changenum(change),
                       msg=msg, labels=labels, notify='ALL')

  def RemoveCommitReady(self, change, dryrun=False):
    """Set the 'Commit-Queue' label on a gerrit change to '0'."""
    if dryrun:
      cros_build_lib.Info('Would have reset Commit-Queue label for %s', change)
      return
    gob_util.ResetReviewLabels(self.host, self._to_changenum(change),
                               label='Commit-Queue', notify='OWNER')

  def SubmitChange(self, change, dryrun=False):
    """Land (merge) a gerrit change."""
    if dryrun:
      cros_build_lib.Info('Would have submitted change %s', change)
      return
    gob_util.SubmitChange(self.host, self._to_changenum(change))

  def AbandonChange(self, change, dryrun=False):
    """Mark a gerrit change as 'Abandoned'."""
    if dryrun:
      cros_build_lib.Info('Would have abandoned change %s', change)
      return
    gob_util.AbandonChange(self.host, self._to_changenum(change))

  def RestoreChange(self, change, dryrun=False):
    """Re-activate a previously abandoned gerrit change."""
    if dryrun:
      cros_build_lib.Info('Would have restored change %s', change)
      return
    gob_util.RestoreChange(self.host, self._to_changenum(change))

  def DeleteDraft(self, change, dryrun=False):
    """Delete a draft patch set."""
    if dryrun:
      cros_build_lib.Info('Would have deleted draft patch set %s', change)
      return
    gob_util.DeleteDraft(self.host, self._to_changenum(change))



def GetGerritPatchInfo(patches):
  """Query Gerrit server for patch information.

  Args:
    patches: a list of patch IDs to query. Internal patches start with a '*'.

  Returns:
    A list of GerritPatch objects describing each patch.  Only the first
    instance of a requested patch is returned.

  Raises:
    PatchException if a patch can't be found.
  """
  # First, convert them to PatchQuery objects for query.
  patches = [cros_patch.ParsePatchDep(x) for x in patches]
  seen = set()
  results = []
  for patch in patches:
    helper = GetGerritHelper(patch.remote)
    raw_ids = [x.ToGerritQueryText() for x in patches]
    for _k, change in helper.QueryMultipleCurrentPatchset(raw_ids):
      # return a unique list, while maintaining the ordering of the first
      # seen instance of each patch.  Do this to ensure whatever ordering
      # the user is trying to enforce, we honor; lest it break on
      # cherry-picking.
      if change.id not in seen:
        results.append(change)
        seen.add(change.id)

  return results


def GetGerritHelper(remote=None, gob=None, **kwargs):
  """Return a GerritHelper instance for interacting with the given remote."""
  if gob:
    return GerritHelper.FromGob(gob, **kwargs)
  else:
    return GerritHelper.FromRemote(remote, **kwargs)


def GetGerritHelperForChange(change):
  """Return a usable GerritHelper instance for this change.

  If you need a GerritHelper for a specific change, get it via this
  function.
  """
  return GetGerritHelper(change.remote)


def GetCrosInternal(**kwargs):
  """Convenience method for accessing private ChromeOS gerrit."""
  return GetGerritHelper(constants.INTERNAL_REMOTE, **kwargs)


def GetCrosExternal(**kwargs):
  """Convenience method for accessing public ChromiumOS gerrit."""
  return GetGerritHelper(constants.EXTERNAL_REMOTE, **kwargs)


def GetChangeRef(change_number, patchset=None):
  """Given a change number, return the refs/changes/* space for it.

  Args:
    change_number: The gerrit change number you want a refspec for.
    patchset: If given it must either be an integer or '*'.  When given,
      the returned refspec is for that exact patchset.  If '*' is given, it's
      used for pulling down all patchsets for that change.

  Returns:
    A git refspec.
  """
  change_number = int(change_number)
  s = 'refs/changes/%02i/%i' % (change_number % 100, change_number)
  if patchset is not None:
    s += '/%s' % ('*' if patchset == '*' else int(patchset))
  return s
