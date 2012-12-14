# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing helper class and methods for interacting with Gerrit."""

import itertools
import json
import logging
import operator

from chromite.buildbot import constants
from chromite.lib import cros_build_lib
from chromite.lib import patch as cros_patch


class GerritException(Exception):
  "Base exception, thrown for gerrit failures"""


class QueryHasNoResults(GerritException):
  "Exception thrown when a query returns no results."""


class QueryNotSpecific(GerritException):
  """Exception thrown for when a query needs to identify one CL, but matched
  multiple."""


class FailedToReachGerrit(GerritException):
  """Exception thrown if we failed to contact the Gerrit server."""


class GerritHelper(object):
  """Helper class to manage interaction with Gerrit server."""

  _GERRIT_MAX_QUERY_RETURN = 500

  def __init__(self, host, remote, ssh_port=29418, ssh_user=None, suexec=None,
               print_cmd=True):
    """Initializes variables for interaction with a gerrit server.

    Args:
      host: Mandatory; this should be the address (ip or dns) of where
        the gerrit instance lives.
      port: Integer if given; the ssh port gerrit responds on.
      user: If given, the user to force for all ssh activities.
      suexec: If given, the email address to suexec to during ssh
        commands.  Used only by maintenance accounts.
      print_cmd: This is passed to all RunCommand invocations; set it
        to False if you want things quiet.
    """
    self.host = host
    self.remote = remote
    self.ssh_port = int(ssh_port)
    self.ssh_user = ssh_user
    self.suexec = suexec
    self.print_cmd = bool(print_cmd)
    self._version = None

  @classmethod
  def FromRemote(cls, remote, **kwds):
    if remote == constants.INTERNAL_REMOTE:
      port = constants.GERRIT_INT_PORT
      host = constants.GERRIT_INT_HOST
    elif remote == constants.EXTERNAL_REMOTE:
      port = constants.GERRIT_PORT
      host = constants.GERRIT_HOST
    else:
      raise ValueError('Remote %s not supported.' % remote)

    return cls(host, remote, ssh_port=port, **kwds)

  @classmethod
  def FromManifestProject(cls, manifest, project, **kwds):
    """Get the GerritHelper instance for a given project.

    Args:
      manifest: cros_build_lib.ManifestCheckout object.
      project: Project to look up.
    """
    remote = manifest.GetAttributeForProject(project, 'remote')
    return cls.FromRemote(remote, **kwds)

  @classmethod
  def GetCrosInternal(cls, **kwds):
    """Convenience method for accessing private ChromeOS gerrit."""
    return cls.FromRemote(constants.INTERNAL_REMOTE, **kwds)

  @classmethod
  def GetCrosExternal(cls, **kwds):
    """Convenience method for accessing public ChromiumOS gerrit."""
    return cls.FromRemote(constants.EXTERNAL_REMOTE, **kwds)

  @property
  def ssh_url(self):
    s = '%s@%s' % (self.ssh_user, self.host) if self.ssh_user else self.host
    return "ssh://%s:%i" % (s, self.ssh_port)

  @property
  def base_ssh_prefix(self):
    l = ['ssh', '-p', str(self.ssh_port), self.host]
    if self.ssh_user:
      l.extend(['-l', self.ssh_user])
    return l

  # Certain code needs access to this to override suexec...
  def GetSshPrefix(self, suexec=None):
    l = self.base_ssh_prefix
    suexec = suexec if suexec is not None else self.suexec
    if suexec is not None:
      l += ['suexec', '--as', suexec, '--']
    return l

  # ... but most code prefers just the property route.
  @property
  def ssh_prefix(self):
    return self.GetSshPrefix()

  def SetReviewers(self, change, add=(), remove=(), project=None):
    """Adjust the reviewers list for a given change.

    Arguments:
      change: Either the ChangeId, or preferably, the gerrit change number.
        If you use a ChangeId be aware that this command will fail if multiple
        changes match.  Can be either a string or an integer.
      add: Either this or removes must be given.  If given, it must be a
        either a single email address/group name, or a sequence of email
        addresses or group names to add as reviewers.  Note it's not
        considered an error if you attempt to add a reviewer that
        already is marked as a reviewer for the change.
      remove: Same rules as 'add', just is the list of reviewers to remove.
      project: If given, the project to find the given change w/in.  Unnecessary
        if passing a gerrit number for change; if passing a ChangeId, strongly
        advised that a project be specified.
    Raises:
      RunCommandError if the attempt to modify the reviewers list fails.  If
      the command fails, no changes to the reviewer list occurs.
    """
    if not add and not remove:
      raise ValueError('Either add or remove must be non empty')

    command = self.ssh_prefix + ['gerrit', 'set-reviewers']
    command.extend(cros_build_lib.iflatten_instance(
        [('--add', x) for x in cros_build_lib.iflatten_instance(add)] +
        [('--remove', x) for x in cros_build_lib.iflatten_instance(remove)]))
    if project is not None:
      command += ['--project', project]
    # Always set the change last; else --project may not take hold by that
    # point, with gerrit complaining of duplicates when there aren't.
    # Yes kiddies, gerrit can be retarded; this being one of those cases.
    command.append(str(change))
    cros_build_lib.RunCommandCaptureOutput(command, print_cmd=self.print_cmd)

  def GetGerritReviewCommand(self, command_list):
    """Returns array corresponding to Gerrit Review command.

    Review can be used to modify a changelist.  Specifically it can change
    scores, abandon, restore or submit it.  Pass in |command|.
    """
    assert isinstance(command_list, list), 'Review command must be list.'
    return self.ssh_prefix + ['gerrit', 'review'] + command_list

  def GrabPatchFromGerrit(self, project, change, commit, must_match=True):
    """Returns the GerritChange described by the arguments.

    Args:
      project: Name of the Gerrit project for the change.
      change:  The change ID for the change.
      commit:  The specific commit hash for the patch from the review.
      must_match: Defaults to True; if True, the given changeid *must*
        be found on the target gerrit server.  If False, a change not found
        is considered uncommited.
    """
    query = ('project:%(project)s AND change:%(change)s AND commit:%(commit)s'
             % {'project': project, 'change': change, 'commit': commit})
    return self.QuerySingleRecord(query, must_match=must_match)

  def IsChangeCommitted(self, query, dryrun=False, must_match=True):
    """Checks to see whether a change is already committed.

    Args:
      query: Either a Change-Id or a Change number to query for.
      dryrun: Whether to perform the operations or not.  If set, returns True.
      must_match: Defaults to True; if True, the given changeid *must*
        be found on the target gerrit server.  If False, a change not found
        is considered uncommited.
    Raises:
      GerritException: If must_match=True, and no match was found.
      QueryNotSpecific: If multiple CLs match the given query.  This can occur
        when a Change-ID was uploaded to multiple branches of a project
        unchanged.
    """
    result = self.QuerySingleRecord('change:%s' % (query,),
                                    must_match=must_match, dryrun=dryrun)
    if dryrun:
      return True

    if result is None:
      # This can only occur if must_match=False
      return False

    return result.status == 'MERGED'

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
          redirect_stdout=True, print_cmd=self.print_cmd)
      if result:
        return result.output.split()[0]
    except cros_build_lib.RunCommandError as e:
      # Fall out to Gerrit error.
      logging.error('Failed to contact git server with %s', e)

    raise FailedToReachGerrit('Could not contact gerrit to get latest sha1')

  def QuerySingleRecord(self, query, **kwds):
    """Freeform querying of a gerrit server, expecting exactly one row returned

    Args:
      query: See Query for details.  This is just a wrapping function.
      kwds: See Query for details.  This is just a wrapping function.  This
        method accepts one additional keyword that Query doesn't: must_match,
        which defaults to True.  If this is True and the query didn't match
        anything, it'll raise a GerritException.  If False, it returns None
    Returns:
      If raw=True, a single dictionary or a cros_patch.GerritPatch instance
        if a single record was found.  If must_match=False and no record was
        found in gerrit, None.
    Raises:
      GerritException derivatives.
    """
    dryrun = kwds.get('dryrun')
    must_match = kwds.pop('must_match', True)
    results = self.Query(query, **kwds)

    if dryrun:
      return None
    elif not results:
      if must_match:
        raise QueryHasNoResults('Query %s had no results' % (query,))
      return None
    elif len(results) != 1:
      raise QueryNotSpecific('Query %s returned too many results: %s'
                             % (query, results))
    return results[0]

  def Query(self, query, sort=None, current_patch=True, options=(),
            dryrun=False, raw=False, _resume_sortkey=None):
    """Freeform querying of a gerrit server

    Args:
     query: gerrit query to run: see the official docs for valid parameters:
       http://gerrit.googlecode.com/svn/documentation/2.1.7/cmd-query.html
     sort: if given, the key in the resultant json to sort on
     current_patch: If True, append --current-patch-set to options.  If this
       is set to False, return the raw dictionary.  If False, raw is forced
       to True.
     options: any additional commandline options to pass to gerrit query

    Returns:
     a sequence of dictionaries from the gerrit server
    Raises:
     RunCommandException if the invocation fails, or GerritException if
     there is something wrong w/ the query parameters given
    """

    cmd = self.ssh_prefix + ['gerrit', 'query', '--format=JSON']
    cmd.extend(options)
    if current_patch:
      cmd.append('--current-patch-set')
    else:
      raw = True

    # Note we intentionally cap the query to 500; gerrit does so
    # already, but we force it so that if gerrit were to change
    # its return limit, this wouldn't break.
    overrides = ['limit:%i' % self._GERRIT_MAX_QUERY_RETURN]
    if _resume_sortkey:
      overrides += ['resume_sortkey:%s' % _resume_sortkey]

    cmd.extend(['--', query] + overrides)

    if dryrun:
      logging.info('Would have run %s', ' '.join(cmd))
      return []
    result = cros_build_lib.RunCommand(cmd, redirect_stdout=True,
                                       print_cmd=self.print_cmd)
    result = self.InterpretJSONResults(query, result.output)

    if len(result) == self._GERRIT_MAX_QUERY_RETURN:
      # Gerrit cuts us off at 500; thus go recursive via the sortKey to
      # get the rest of the results.
      result += self.Query(query, _resume_sortkey=result[-1]['sortKey'],
                           current_patch=current_patch,
                           options=options, dryrun=dryrun, raw=True)

    if sort:
      result = sorted(result, key=operator.itemgetter(sort))
    if not raw:
      return [cros_patch.GerritPatch(x, self.remote, self.ssh_url)
              for x in result]

    return result

  def InterpretJSONResults(self, query, result_string, query_type='stats',
                           mode='query'):
    result = map(json.loads, result_string.splitlines())

    status = result[-1]
    if 'type' not in status:
      raise GerritException('Weird results from gerrit: asked %s %s, got %s' %
        (mode, query, result))

    if status['type'] != query_type:
      raise GerritException('Bad gerrit %s: query %s, error %s' %
        (mode, query, status.get('message', status)))

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
      query = ' OR '.join('change:%s' % x for x in numeric_queries)
      results = self.Query(query, sort='number')

      # Sort via alpha comparison, rather than integer; Query sorts via the
      # raw textual field, thus we need to match that.
      numeric_queries = sorted(numeric_queries, key=str)

      for query, result in itertools.izip_longest(numeric_queries, results):
        if result is None or result.gerrit_number != query:
          raise GerritException('Change number %s not found on server %s.'
                                 % (query, self.host))

        yield query, result

    id_queries = sorted(cros_patch.FormatPatchDep(x, sha1=False)
                        for x in queries if not x.isdigit())

    if not id_queries:
      return

    results = self.Query(' OR '.join('change:%s' % x for x in id_queries),
                         sort='id')

    last_patch_id = None
    for query, result in itertools.izip_longest(id_queries, results):
      # case insensitivity to ensure that if someone queries for IABC
      # and gerrit returns Iabc, we still properly match.
      result_id = ''
      if result:
        result_id = cros_patch.FormatChangeId(result.change_id)

      if result is None or (query and not result_id.startswith(query)):
        if last_patch_id and result_id.startswith(last_patch_id):
          raise GerritException(
              'While querying for change %s, we received '
              'back multiple results.  Please be more specific.  Server=%s'
              % (last_patch_id, self.host))

        raise GerritException('Change-ID %s not found on server %s.'
                              % (query, self.host))

      if query is None:
        raise GerritException(
            'While querying for change %s, we received '
            'back multiple results.  Please be more specific.  Server=%s'
            % (last_patch_id, self.host))

      yield query, result
      last_patch_id = query

  @property
  def version(self):
    obj = self._version
    if obj is None:
      # We suppress the gerrit version call's logging; it's basically
      # never useful log wise.
      obj = cros_build_lib.RunCommandCaptureOutput(
          self.ssh_prefix + ['gerrit', 'version'],
          print_cmd=False).output.strip()
      obj = obj.replace('gerrit version ', '')
      self._version = obj
    return obj

  def _SqlQuery(self, query, dryrun=False, is_command=False):
    """Run a gsql query against gerrit.

    Doing so requires an admin account, and a fair amount of care-
    bad code can trash the underlying DB pretty easily.

    Args:
     query: SQL query to run.
     dryrun: Should we run the SQL, or just pretend we did?
     is_command: Does the SQL modify records (DML), or is it just
       a query?  If it's DML, then this must be set to True.

    Return:
     List of dictionaries returned from gerrit for the SQL ran.
    """
    if dryrun:
      logging.info("Would have ran sql query %r", (query,))
      return []

    command = self.ssh_prefix + ['gerrit', 'gsql', '--format=JSON']
    result = cros_build_lib.RunCommand(command, redirect_stdout=True,
                                       input=query, print_cmd=self.print_cmd)

    query_type = 'update-stats' if is_command else 'query-stats'

    result = self.InterpretJSONResults(query, result.output,
                                       query_type=query_type,
                                       mode='gsql')
    return result

  def RemoveCommitReady(self, change, dryrun=False):
    """Remove any commit ready bits associated with CL.

    Args:
     change: GerritChange instance to strip the CR bit from.
     dryrun: Whether to perform the operation or not.
    """
    query = ("DELETE FROM patch_set_approvals WHERE change_id=%s"
             " AND patch_set_id=%s "
             " AND category_id='COMR';"
             % (change.gerrit_number, change.patch_number))
    self._SqlQuery(query, dryrun=dryrun, is_command=True)

  def FindContentMergingProjects(self):
    """Query the gerrit server to find which projects have content merging on

    Content merging is also known as 3way merging; if enabled, changes that
    aren't in the same git historical lineage and touch the same file can
    fall back to patch (fuzzy matches) semantics.  If disabled, then changes
    that try to touch the same file *must* be on the same lineage.

    Returns:
      A set of all projects that have content merging enabled
    """

    # Example json output from gerrit:
    # {"type":"row","columns":{"name":"webm/libvpx"}}
    # {"type":"row","columns":{"name":"webm/adaptive-prototype-manifest"}}
    # {"type":"row","columns":{"name":"webm/bitstream-guide"}}
    # {"type":"row","columns":{"name":"chromiumos/chromite"}}
    # {"type":"query-stats","rowCount":4,"runTimeMilliseconds":2}

    results = self._SqlQuery(
        "SELECT name FROM projects where use_content_merge='Y';")
    return frozenset(x['columns']['name'] for x in results)


def GetGerritPatchInfo(patches):
  """Query Gerrit server for patch information.

  Args:
    patches: a list of patch IDs to query.  Internal patches start with a '*'.

  Returns:
    A list of GerritPatch objects describing each patch.  Only the first
    instance of a requested patch is returned.

  Raises:
    PatchException if a patch can't be found.
  """
  parsed_patches = {}

  # First, standardize 'em.
  patches = [cros_patch.FormatPatchDep(x, sha1=False, allow_CL=True)
             for x in patches]

  # Next, split on internal vs external.
  internal_patches = [x for x in patches if x.startswith('*')]
  external_patches = [x for x in patches if not x.startswith('*')]

  if internal_patches:
    # feed it id's w/ * stripped off, but bind them back
    # so that we can return patches in the supplied ordering.
    # while this may seem silly, we do this to preclude the potential
    # of a conflict between gerrit instances.  Since change-id is
    # effectively user controlled, better safe than sorry.
    helper = GerritHelper.FromRemote(constants.INTERNAL_REMOTE)
    raw_ids = [x[1:] for x in internal_patches]
    parsed_patches.update(('*' + k, v) for k, v in
        helper.QueryMultipleCurrentPatchset(raw_ids))

  if external_patches:
    helper = GerritHelper.FromRemote(constants.EXTERNAL_REMOTE)
    parsed_patches.update(
        helper.QueryMultipleCurrentPatchset(external_patches))

  seen = set()
  results = []
  for query in patches:
    # return a unique list, while maintaining the ordering of the first
    # seen instance of each patch.  Do this to ensure whatever ordering
    # the user is trying to enforce, we honor; lest it break on cherry-picking
    gpatch = parsed_patches[query]
    if gpatch.change_id not in seen:
      results.append(gpatch)
      seen.add(gpatch.change_id)

  return results


def GetGerritHelperForChange(change):
  """Return a usable GerritHelper instance for this change.

  If you need a GerritHelper for a specific change, get it via this
  function.
  """
  return GerritHelper.FromRemote(change.remote)


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
