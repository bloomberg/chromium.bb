# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing helper class and methods for interacting with Gerrit."""

import itertools
import json
import logging
import operator

from chromite.buildbot import constants
from chromite.buildbot import patch as cros_patch
from chromite.lib import cros_build_lib


class GerritException(Exception):
  "Base exception, thrown for gerrit failures"""

class FailedToReachGerrit(GerritException):
  """Exception thrown if we failed to contact the Gerrit server."""


class GerritHelper():
  """Helper class to manage interaction with Gerrit server."""
  def __init__(self, internal):
    """Initializes variables for interaction with a gerrit server."""
    if internal:
      self.ssh_port = constants.GERRIT_INT_PORT
      self.ssh_host = constants.GERRIT_INT_HOST
      self.ssh_url  = constants.GERRIT_INT_SSH_URL
    else:
      self.ssh_port = constants.GERRIT_PORT
      self.ssh_host = constants.GERRIT_HOST
      self.ssh_url  = constants.GERRIT_SSH_URL

    self.internal = internal

  @property
  def ssh_prefix(self):
    return ['ssh', '-p', self.ssh_port, self.ssh_host]

  def GetGerritQueryCommand(self, query_list):
    """Returns array corresponding to Gerrit Query command.

    Query is useful for getting information about changelists.  Returns results
    in json format for easy dictionary parsing.  Pass in |query| as the query
    args.
    """
    assert isinstance(query_list, list), 'Query command must be list.'
    return self.ssh_prefix + ['gerrit', 'query', '--format=json'] + query_list

  def GetGerritSqlCommand(self, command_list):
    """Returns array corresponding to Gerrit Review command.

    Review can be used to modify a changelist.  Specifically it can change
    scores, abandon, restore or submit it.  Pass in |command|.
    """
    assert isinstance(command_list, list), 'Sql command must be list.'
    return self.ssh_prefix + ['gerrit', 'gsql'] + command_list

  def GetGerritReviewCommand(self, command_list):
    """Returns array corresponding to Gerrit Review command.

    Review can be used to modify a changelist.  Specifically it can change
    scores, abandon, restore or submit it.  Pass in |command|.
    """
    assert isinstance(command_list, list), 'Review command must be list.'
    return self.ssh_prefix + ['gerrit', 'review'] + command_list


  def GrabChangesReadyForCommit(self):
    """Returns the list of changes to try.

    This methods returns a a list of GerritPatch's to try.
    """
    query_string = ('status:open AND CodeReview=+2 AND Verified=+1 '
                    'AND CommitReady=+1 '
                    'AND NOT CodeReview=-2 AND NOT Verified=-1')

    ready_for_commit = ['--current-patch-set', '"%s"' % query_string]

    query_cmd = self.GetGerritQueryCommand(ready_for_commit)
    raw_results = cros_build_lib.RunCommand(query_cmd,
                                            redirect_stdout=True)

    # Each line returned is a json dict.
    changes_to_commit = []
    for raw_result in raw_results.output.splitlines():
      result_dict = json.loads(raw_result)
      if not 'id' in result_dict:
        logging.debug('No change number found in %s', result_dict)
        continue

      changes_to_commit.append(cros_patch.GerritPatch(result_dict,
                                                      self.internal))

    # Change to commit are ordered in descending order by last update.
    # To be fair it makes sense to do apply changes eldest to newest.
    changes_to_commit.reverse()
    return changes_to_commit

  def GrabPatchFromGerrit(self, project, change, commit):
    """Returns the GerritChange described by the arguments.

    Args:
      project: Name of the Gerrit project for the change.
      change:  The change ID for the change.
      commit:  The specific commit hash for the patch from the review.
    """
    grab_change = (
        ['--current-patch-set',
         '"project:%(project)s AND change:%(change)s AND commit:%(commit)s"' %
             {'project': project, 'change': change, 'commit': commit}
        ])

    query_cmd = self.GetGerritQueryCommand(grab_change)
    raw_results = cros_build_lib.RunCommand(query_cmd,
                                            redirect_stdout=True)

    # First line returned is the json.
    raw_result = raw_results.output.splitlines()[0]
    result_dict = json.loads(raw_result)
    assert result_dict.get('id'), 'Code Review json missing change-id!'
    return cros_patch.GerritPatch(result_dict, self.internal)

  def IsChangeCommitted(self, changeid, dryrun=False):
    """Checks to see whether a change is already committed."""
    rev_cmd = self.GetGerritQueryCommand(['change:%s' % changeid])
    if not dryrun:
      raw_results = cros_build_lib.RunCommand(rev_cmd, redirect_stdout=True)
      result_dict = json.loads(raw_results.output.splitlines()[0])
      return result_dict.get('status') not in ['NEW', 'ABANDONED', None]
    else:
      logging.info('Would have run %s', ' '.join(rev_cmd))
      return True

  def GetLatestSHA1ForBranch(self, project, branch):
    """Finds the latest commit hash for a repository/branch.

    Returns:
      The latest commit hash for this patch's repo/branch.

    Raises:
      FailedToReachGerrit if we fail to contact gerrit.
    """
    ssh_url_project = '%s/%s' % (self.ssh_url, project)
    try:
      result = cros_build_lib.RunCommandWithRetries(3,
          ['git', 'ls-remote', ssh_url_project, 'refs/heads/%s' % (branch,)],
          redirect_stdout=True, print_cmd=True)
      if result:
        return result.output.split()[0]
    except cros_build_lib.RunCommandError as e:
      # Fall out to Gerrit error.
      logging.error('Failed to contact git server with %s', e)

    raise FailedToReachGerrit('Could not contact gerrit to get latest sha1')

  def _Query(self, or_parameters, sort=None, options=()):
    """Freeform querying of a gerrit server

    Args:
     or_parameters: sequence of gerrit query chunks that are OR'd together
     sort: if given, the key in the resultant json to sort on
     options: any additional commandline options to pass to gerrit query

    Returns:
     a sequence of dictionaries from the gerrit server
    Raises:
     RunCommandException if the invocation fails, or GerritException if
     there is something wrong w/ the query parameters given
    """

    cmd = self.ssh_prefix + ['gerrit', 'query', '--format=JSON']
    cmd.extend(options)
    cmd.extend(['--', ' OR '.join(or_parameters)])

    result = cros_build_lib.RunCommand(cmd, redirect_stdout=True)
    result = self.InterpretJSONResults(or_parameters, result.output)

    if sort:
      return sorted(result, key=operator.itemgetter(sort))
    return result

  def InterpretJSONResults(self, queries, result_string, query_type='stats'):
    result = map(json.loads, result_string.splitlines())

    status = result[-1]
    if 'type' not in status:
      raise GerritException('weird results from gerrit: asked %s, got %s' %
        (queries, result))

    if status['type'] != query_type:
      raise GerritException('bad gerrit query: parameters %s, error %s' %
        (queries, status.get('message', status)))

    return result[:-1]

  def QueryMultipleCurrentPatchset(self, queries):
    """Query chromeos gerrit servers for the current patch for given changes

    Args:
     queries: sequence of Change-IDs (Ic04g2ab, 6 characters to 40),
       or change numbers (12345 for example).
       A change number can refer to the same change as a Change ID,
       but Change IDs given should be unique, and the same goes for Change
       Numbers.

    Returns:
     an unordered sequence of GerritPatches for each requested query.

    Raises:
     GerritException: if a query fails to match, or isn't specific enough,
      or a query is malformed.
     RunCommandException: if for whatever reason, the ssh invocation to
      gerrit fails.
    """

    if not queries:
      return

    # process the queries in two seperate streams; this is done so that
    # we can identify exactly which patchset returned no results; it's
    # basically impossible to do it if you query with mixed numeric/ID

    numeric_queries = [x for x in queries if x.isdigit()]

    if numeric_queries:
      results = self._Query(['change:%s' % x for x in numeric_queries],
                            sort='number', options=('--current-patch-set',))

      # Sort via alpha comparison, rather than integer; Query sorts via the
      # raw textual field, thus we need to match that.
      numeric_queries = sorted(numeric_queries, key=str)

      for query, result in itertools.izip_longest(numeric_queries, results):
        if result is None or result['number'] != query:
          raise GerritException('Change number %s not found on server %s.'
                                 % (query, self.ssh_host))

        yield query, cros_patch.GerritPatch(result, self.internal)

    id_queries = sorted(x.lower() for x in queries if not x.isdigit())

    if not id_queries:
      return

    results = self._Query(['change:%s' % x for x in id_queries],
                          sort='id', options=('--current-patch-set',))

    last_patch_id = None
    for query, result in itertools.izip_longest(id_queries, results):
      # case insensitivity to ensure that if someone queries for IABC
      # and gerrit returns Iabc, we still properly match.
      result_id = result.get('id', '') if result is not None else ''
      result_id = result_id.lower()

      if result is None or (query and not result_id.startswith(query)):
        if last_patch_id and result_id.startswith(last_patch_id):
          raise GerritException(
              'While querying for change %s, we received '
              'back multiple results.  Please be more specific.  Server=%s'
              % (last_patch_id, self.ssh_host))

        raise GerritException('Change-ID %s not found on server %s.'
                              % (query, self.ssh_host))

      if query is None:
        raise GerritException(
            'While querying for change %s, we received '
            'back multiple results.  Please be more specific.  Server=%s'
            % (last_patch_id, self.ssh_host))

      yield query, cros_patch.GerritPatch(result, self.internal)
      last_patch_id = query


def GetGerritPatchInfo(patches):
  """Query Gerrit server for patch information.

  Args:
    patches: a list of patch ID's to query.  Internal patches start with a '*'.

  Returns:
    A list of GerritPatch objects describing each patch.  Only the first
    instance of a requested patch is returned.

  Raises:
    PatchException if a patch can't be found.
  """
  parsed_patches = {}
  internal_patches = [x for x in patches if x.startswith('*')]
  external_patches = [x for x in patches if not x.startswith('*')]

  if internal_patches:
    # feed it id's w/ * stripped off, but bind them back
    # so that we can return patches in the supplied ordering
    # while this may seem silly; we do this to preclude the potential
    # of a conflict between gerrit instances; since change-id is
    # effectively user controlled, better safe than sorry.
    helper = GerritHelper(True)
    raw_ids = [x[1:] for x in internal_patches]
    parsed_patches.update(('*' + k, v) for k, v in
        helper.QueryMultipleCurrentPatchset(raw_ids))

  if external_patches:
    helper = GerritHelper(False)
    parsed_patches.update(
        helper.QueryMultipleCurrentPatchset(external_patches))

  seen = set()
  results = []
  for query in patches:
    # return a unique list, while maintaining the ordering of the first
    # seen instance of each patch.  Do this to ensure whatever ordering
    # the user is trying to enforce, we honor; lest it break on cherry-picking
    gpatch = parsed_patches[query.lower()]
    if gpatch.id not in seen:
      results.append(gpatch)
      seen.add(gpatch.id)

  return results


def GetGerritHelperForChange(change):
  """Return a usable GerritHelper instance for this change.

  If you need a GerritHelper for a specific change, get it via this
  function.
  """
  return GerritHelper(change.internal)
