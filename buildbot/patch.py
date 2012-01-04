# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module that handles the processing of patches to the source tree."""

import constants
import glob
import json
import logging
import os
import re
import shutil
import tempfile
import itertools
import operator

from chromite.lib import cros_build_lib as cros_lib

# The prefix of the temporary directory created to store local patches.
_TRYBOT_TEMP_PREFIX = 'trybot_patch-'

def _RunCommand(cmd, dryrun):
  """Runs the specified shell cmd if dryrun=False."""
  if dryrun:
    logging.info('Would have run: %s', ' '.join(cmd))
  else:
    cros_lib.RunCommand(cmd, error_ok=True)


class GerritException(Exception):
  "Base exception, thrown for gerrit failures"""


class PatchException(GerritException):
  """Exception thrown by GetGerritPatchInfo."""


class ApplyPatchException(Exception):
  """Exception thrown if we fail to apply a patch."""
  # Types used to denote what we failed to apply against.
  TYPE_REBASE_TO_TOT = 1
  TYPE_REBASE_TO_PATCH_INFLIGHT = 2

  def __init__(self, patch, patch_type=TYPE_REBASE_TO_TOT):
    super(ApplyPatchException, self).__init__()
    self.patch = patch
    self.type = patch_type

  def __str__(self):
    return 'Failed to apply patch ' + str(self.patch)


class MissingChangeIDException(Exception):
  """Raised if a patch is missing a Change-ID."""
  pass


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


class Patch(object):
  """Abstract class representing a Git Patch."""

  def __init__(self, project, tracking_branch):
    """Initialization of abstract Patch class.

    Args:
      project: The name of the project that the patch applies to.
      tracking_branch:  The remote branch of the project the patch applies to.
    """
    self.project = project
    self.tracking_branch = tracking_branch

  def Apply(self, buildroot, trivial):
    """Applies the patch to specified buildroot. Implement in subclasses.

    Args:
      buildroot:  The buildroot.
      trivial:  Only allow trivial merges when applying change.

    Raises:
      PatchException
    """
    raise NotImplementedError('Applies the patch to specified buildroot.')


class GerritPatch(Patch):
  """Object that represents a Gerrit CL."""
  _PUBLIC_URL = os.path.join(constants.GERRIT_HTTP_URL, 'gerrit/p')
  _GIT_CHANGE_ID_RE = re.compile(r'^\s*Change-Id:\s*(\w+)\s*$', re.MULTILINE)
  _PALADIN_DEPENDENCY_RE = re.compile(r'^\s*CQ-DEPEND=(.*)$', re.MULTILINE)
  _PALADIN_BUG_RE = re.compile('(\w+)')

  def __init__(self, patch_dict, internal):
    """Construct a GerritPatch object from Gerrit query results.

    Gerrit query JSON fields are documented at:
    http://gerrit-documentation.googlecode.com/svn/Documentation/2.2.1/json.html

    Args:
      patch_dict: A dictionary containing the parsed JSON gerrit query results.
      internal: Whether the CL is an internal CL.
    """
    Patch.__init__(self, patch_dict['project'], patch_dict['branch'])
    self.internal = internal
    # id - The CL's ChangeId
    self.id = patch_dict['id']
    # ref - The remote ref that contains the patch.
    self.ref = patch_dict['currentPatchSet']['ref']
    # revision - The CL's SHA1 hash.
    self.revision = patch_dict['currentPatchSet']['revision']
    self.patch_number = patch_dict['currentPatchSet']['number']
    self.commit = patch_dict['currentPatchSet']['revision']
    self.owner, _, _ = patch_dict['owner']['email'].partition('@')
    self.gerrit_number = patch_dict['number']
    self.url = patch_dict['url']
    # status - Current state of this change.  Can be one of
    # ['NEW', 'SUBMITTED', 'MERGED', 'ABANDONED'].
    self.status = patch_dict['status']
    # Allows a caller to specify why we can't apply this change when we
    # HandleApplicaiton failures.
    self.apply_error_message = ('Please re-sync, rebase, and re-upload your '
                                'change.')


  def IsAlreadyMerged(self):
    """Returns whether the patch has already been merged in Gerrit."""
    return self.status == 'MERGED'

  def _GetProjectUrl(self):
    """Returns the url to the gerrit project."""
    if self.internal:
      url_prefix = constants.GERRIT_INT_SSH_URL
    else:
      url_prefix = self._PUBLIC_URL

    return os.path.join(url_prefix, self.project)

  def _RebaseOnto(self, branch, upstream, project_dir, trivial):
    """Attempts to rebase FETCH_HEAD onto branch -- while not on a branch.

    Raises:
      cros_lib.RunCommandError:  If the rebase operation returns an error code.
        In this case, we still rebase --abort before returning.
    """
    try:
      git_rb = ['git', 'rebase']
      if trivial: git_rb.extend(['--strategy', 'resolve', '-X', 'trivial'])
      git_rb.extend(['--onto', branch, upstream, 'FETCH_HEAD'])
      # Run the rebase command.
      cros_lib.RunCommand(git_rb, cwd=project_dir, print_cmd=False)

    except cros_lib.RunCommandError:
      cros_lib.RunCommand(['git', 'rebase', '--abort'], cwd=project_dir,
                          error_ok=True, print_cmd=False)
      raise

  def _RebasePatch(self, buildroot, project_dir, trivial):
    """Rebase patch fetched from gerrit onto constants.PATCH_BRANCH.

    When the function completes, the constants.PATCH_BRANCH branch will be
    pointing to the rebased change.

    Arguments:
      buildroot: The buildroot.
      project_dir: Directory of the project that is being patched.
      trivial: Use trivial logic that only allows trivial merges.  Note:
        Requires Git >= 1.7.6 -- bug <.  Bots have 1.7.6 installed.

    Raises:
      ApplyPatchException: If the patch failed to apply.
    """
    url = self._GetProjectUrl()
    upstream = _GetProjectManifestBranch(buildroot, self.project)
    cros_lib.RunCommand(['git', 'fetch', url, self.ref], cwd=project_dir,
                        print_cmd=False)
    try:
      self._RebaseOnto(constants.PATCH_BRANCH, upstream, project_dir, trivial)
      cros_lib.RunCommand(['git', 'checkout', '-B', constants.PATCH_BRANCH],
                          cwd=project_dir, print_cmd=False)
    except cros_lib.RunCommandError:
      try:
        # Failed to rebase against branch, try TOT.
        self._RebaseOnto(upstream, upstream, project_dir, trivial)
      except cros_lib.RunCommandError:
        raise ApplyPatchException(
            self, patch_type=ApplyPatchException.TYPE_REBASE_TO_TOT)
      else:
        # We failed to apply to patch_branch but succeeded against TOT.
        # We should pass a different type of exception in this case.
        raise ApplyPatchException(
            self, patch_type=ApplyPatchException.TYPE_REBASE_TO_PATCH_INFLIGHT)

    finally:
      cros_lib.RunCommand(['git', 'checkout', constants.PATCH_BRANCH],
                          cwd=project_dir, print_cmd=False)

  def Apply(self, buildroot, trivial=False):
    """Implementation of Patch.Apply().

    Raises:
      ApplyPatchException: If the patch failed to apply.
    """
    logging.info('Attempting to apply change %s', self)
    project_dir = cros_lib.GetProjectDir(buildroot, self.project)
    if not cros_lib.DoesLocalBranchExist(project_dir, constants.PATCH_BRANCH):
      upstream = cros_lib.GetManifestDefaultBranch(buildroot)
      cros_lib.RunCommand(['git', 'checkout', '-b', constants.PATCH_BRANCH,
                           '-t', 'm/' + upstream], cwd=project_dir,
                          print_cmd=False)
    self._RebasePatch(buildroot, project_dir, trivial)

  # --------------------- Gerrit Operations --------------------------------- #

  def RemoveCommitReady(self, helper, dryrun=False):
    """Remove any commit ready bits associated with CL."""
    query = ['-c',
             '"delete from patch_set_approvals where change_id=%s'
             ' AND category_id=\'COMR\';"' % self.gerrit_number
            ]
    cmd = helper.GetGerritSqlCommand(query)
    _RunCommand(cmd, dryrun)

  def HandleCouldNotSubmit(self, helper, build_log, dryrun=False):
    """Handler that is called when Paladin can't submit a change.

    This should be rare, but if an admin overrides the commit queue and commits
    a change that conflicts with this change, it'll apply, build/validate but
    receive an error when submitting.

    Args:
      helper: Instance of gerrit_helper for the gerrit instance.
      dryrun: If true, do not actually commit anything to Gerrit.

    """
    msg = ('The Commit Queue failed to submit your change in %s . '
           'This can happen if you submitted your change or someone else '
           'submitted a conflicting change while your change was being tested.'
           % build_log)
    PaladinMessage(msg, self, helper).Send(dryrun)
    self.RemoveCommitReady(helper, dryrun)

  def HandleCouldNotVerify(self, helper, build_log, dryrun=False):
    """Handler for when Paladin fails to validate a change.

    This handler notifies set Verified-1 to the review forcing the developer
    to re-upload a change that works.  There are many reasons why this might be
    called e.g. build or testing exception.

    Args:
      helper: Instance of gerrit_helper for the gerrit instance.
      build_log:  URL to the build log where verification results could be
        found.
      dryrun: If true, do not actually commit anything to Gerrit.

    """
    msg = ('The Commit Queue failed to verify your change in %s . '
           'If you believe this happened in error, just re-mark your commit as '
           'ready. Your change will then get automatically retried.' %
           build_log)
    PaladinMessage(msg, self, helper).Send(dryrun)
    self.RemoveCommitReady(helper, dryrun)

  def HandleCouldNotApply(self, helper, build_log, dryrun=False):
    """Handler for when Paladin fails to apply a change.

    This handler notifies set CodeReview-2 to the review forcing the developer
    to re-upload a rebased change.

    Args:
      helper: Instance of gerrit_helper for the gerrit instance.
      build_log: URL of where to find the logs for this build.
      dryrun: If true, do not actually commit anything to Gerrit.
    """
    msg = ('The Commit Queue failed to apply your change in %s . ' %
           build_log)
    msg += self.apply_error_message
    PaladinMessage(msg, self, helper).Send(dryrun)
    self.RemoveCommitReady(helper, dryrun)

  def HandleApplied(self, helper, build_log, dryrun=False):
    """Handler for when Paladin successfully applies a change.

    This handler notifies a developer that their change is being tried as
    part of a Paladin run defined by a build_log.

    Args:
      helper: Instance of gerrit_helper for the gerrit instance.
      build_log: URL of where to find the logs for this build.
      dryrun: If true, do not actually commit anything to Gerrit.
    """
    msg = ('The Commit Queue has picked up your change. '
           'You can follow along at %s .' % build_log)
    PaladinMessage(msg, self, helper).Send(dryrun)

  def CommitMessage(self, buildroot):
    """Returns the commit message for the patch as a string."""
    url = self._GetProjectUrl()
    project_dir = cros_lib.GetProjectDir(buildroot, self.project)
    cros_lib.RunCommand(['git', 'fetch', url, self.ref], cwd=project_dir,
                        print_cmd=False)
    return_obj = cros_lib.RunCommand(['git', 'show', '-s', 'FETCH_HEAD'],
                                     cwd=project_dir, redirect_stdout=True,
                                     print_cmd=False)
    return return_obj.output

  def GerritDependencies(self, buildroot):
    """Returns an ordered list of dependencies from Gerrit.

    The list of changes are in order from FETCH_HEAD back to m/master.

    Arguments:
      buildroot: The buildroot.
    Returns:
      An ordered list of Gerrit revisions that this patch depends on.
    Raises:
      MissingChangeIDException: If a dependent change is missing its ChangeID.
    """
    dependencies = []
    url = self._GetProjectUrl()
    logging.info('Checking for Gerrit dependencies for change %s', self)
    project_dir = cros_lib.GetProjectDir(buildroot, self.project)
    cros_lib.RunCommand(['git', 'fetch', url, self.ref], cwd=project_dir,
                        print_cmd=False)
    return_obj = cros_lib.RunCommand(
        ['git', 'log', '-z', '%s..FETCH_HEAD^' %
          _GetProjectManifestBranch(buildroot, self.project)],
        cwd=project_dir, redirect_stdout=True, print_cmd=False)

    for patch_output in return_obj.output.split('\0'):
      if not patch_output: continue
      change_id_match = self._GIT_CHANGE_ID_RE.search(patch_output)
      if change_id_match:
        dependencies.append(change_id_match.group(1))
      else:
        raise MissingChangeIDException('Missing Change-Id in %s' % patch_output)

    if dependencies:
      logging.info('Found %s Gerrit dependencies for change %s', dependencies,
                   self)

    return dependencies

  def PaladinDependencies(self, buildroot):
    """Returns an ordered list of dependencies based on the Commit Message.

    Parses the Commit message for this change looking for lines that follow
    the format:

    CQ-DEPEND:change_num+ e.g.

    A commit which depends on a couple others.

    BUG=blah
    TEST=blah
    CQ-DEPEND=10001,10002
    """
    dependencies = []
    logging.info('Checking for CQ-DEPEND dependencies for change %s', self)
    commit_message = self.CommitMessage(buildroot)
    matches = self._PALADIN_DEPENDENCY_RE.findall(commit_message)
    for match in matches:
      dependencies.extend(self._PALADIN_BUG_RE.findall(match))

    if dependencies:
      logging.info('Found %s Paladin dependencies for change %s', dependencies,
                   self)
    return dependencies

  def Submit(self, helper, dryrun=False):
    """Submits patch using Gerrit Review.

    Args:
      helper: Instance of gerrit_helper for the gerrit instance.
      dryrun: If true, do not actually commit anything to Gerrit.
    """
    cmd = helper.GetGerritReviewCommand(['--submit', '%s,%s' % (
        self.gerrit_number, self.patch_number)])
    _RunCommand(cmd, dryrun)

  def __str__(self):
    """Returns custom string to identify this patch."""
    return '%s:%s' % (self.owner, self.gerrit_number)

  # Define methods to use patches in sets.  We uniquely identify patches
  # by Gerrit change numbers.
  def __hash__(self):
    return hash(self.id)

  def __eq__(self, other):
    return self.id == other.id


def RemovePatchRoot(patch_root):
  """Removes the temporary directory storing patches."""
  assert os.path.basename(patch_root).startswith(_TRYBOT_TEMP_PREFIX)
  shutil.rmtree(patch_root)


class LocalPatch(Patch):
  """Object that represents a set of local commits that will be patched."""

  def __init__(self, project, tracking_branch, patch_dir, local_branch):
    """Construct a LocalPatch object.

    Args:
      project: Same as Patch constructor arg.
      tracking_branch: Same as Patch constructor arg.
      patch_dir: The directory where the .patch files are stored.
      local_branch:  The local branch of the project that the patch came from.
    """
    Patch.__init__(self, project, tracking_branch)
    self.patch_dir = patch_dir
    self.local_branch = local_branch

  def _GetFileList(self):
    """Return a list of .patch files in sorted order."""
    file_list = glob.glob(os.path.join(self.patch_dir, '*'))
    file_list.sort()
    return file_list

  def Apply(self, buildroot, trivial=False):
    """Implementation of Patch.Apply().  Does not accept trivial option.

    Raises:
      PatchException if the patch is for the wrong tracking branch.
    """
    assert not trivial, 'Local apply not compatible with trivial set'
    manifest_branch = _GetProjectManifestBranch(buildroot, self.project)
    if self.tracking_branch != manifest_branch:
      raise PatchException('branch %s for project %s is not tracking %s'
                           % (self.local_branch, self.project,
                              manifest_branch))

    project_dir = cros_lib.GetProjectDir(buildroot, self.project)
    try:
      cros_lib.RunCommand(['repo', 'start', constants.PATCH_BRANCH, '.'],
                          cwd=project_dir)
      cros_lib.RunCommand(['git', 'am', '--3way'] + self._GetFileList(),
                          cwd=project_dir)
    except cros_lib.RunCommandError:
      raise ApplyPatchException(self)

  def __str__(self):
    """Returns custom string to identify this patch."""
    return '%s:%s' % (self.project, self.local_branch)


def _QueryGerrit(server, port, or_parameters, sort=None, options=()):
  """Freeform querying of a gerrit server

  Args:
   server: the hostname to query
   port: the port to query
   or_parameters: sequence of gerrit query chunks that are OR'd together
   sort: if given, the key in the resultant json to sort on
   options: any additional commandline options to pass to gerrit query

  Returns:
   a sequence of dictionaries from the gerrit server
  Raises:
   RunCommandException if the invocation fails, or GerritException if
   there is something wrong w/ the query parameters given
  """

  cmd = ['ssh', '-p', port, server, 'gerrit', 'query', '--format=JSON']
  cmd.extend(options)
  cmd.extend(['--', ' OR '.join(or_parameters)])

  result = cros_lib.RunCommand(cmd, redirect_stdout=True)
  result = map(json.loads, result.output.splitlines())

  status = result[-1]
  if 'type' not in status:
    raise GerritException('weird results from gerrit: asked %s, got %s' %
      (or_parameters, result))

  if status['type'] != 'stats':
    raise GerritException('bad gerrit query: parameters %s, error %s' %
      (or_parameters, status.get('message', status)))

  result = result[:-1]

  if sort:
    return sorted(result, key=operator.itemgetter(sort))
  return result


def _QueryGerritMultipleCurrentPatchset(queries, internal=False):
  """Query chromeos gerrit servers for the current patch for given changes

  Args:
   queries: sequence of Change-IDs (Ic04g2ab, 6 characters to 40),
     or change numbers (12345 for example).
     A change number can refer to the same change as a Change ID,
     but Change IDs given should be unique, and the same goes for Change
     Numbers.
   internal: optional boolean; if the internal servers are to be queried,
     set to True.  Defaults to False.

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

  if internal:
    server, port = constants.GERRIT_INT_HOST, constants.GERRIT_INT_PORT
  else:
    server, port = constants.GERRIT_HOST, constants.GERRIT_PORT

  # process the queries in two seperate streams; this is done so that
  # we can identify exactly which patchset returned no results; it's
  # basically impossible to do it if you query with mixed numeric/ID

  numeric_queries = [x for x in queries if x.isdigit()]

  if numeric_queries:
    results = _QueryGerrit(server, port,
                           ['change:%s' % x for x in numeric_queries],
                           sort='number', options=('--current-patch-set',))

    for query, result in itertools.izip_longest(numeric_queries, results):
      if result is None or result['number'] != query:
        raise PatchException('Change number %s not found on server %s.'
                             % (query, server))

      yield query, GerritPatch(result, internal)

  id_queries = sorted(x.lower() for x in queries if not x.isdigit())

  if not id_queries:
    return

  results = _QueryGerrit(server, port, ['change:%s' % x for x in id_queries],
                        sort='id', options=('--current-patch-set',))

  last_patch_id = None
  for query, result in itertools.izip_longest(id_queries, results):

    # case insensitivity to ensure that if someone queries for IABC
    # and gerrit returns Iabc, we still properly match.
    result_id = result.get('id', '') if result is not None else ''
    result_id = result_id.lower()

    if result is None or (query and not result_id.startswith(query)):
      if last_patch_id and result_id.startswith(last_patch_id):
        raise PatchException('While querying for change %s, we received '
          'back multiple results.  Please be more specific.  Server=%s'
          % (last_patch_id, server))

      raise PatchException('Change-ID %s not found on server %s.'
                           % (query, server))

    if query is None:
      raise PatchException('While querying for change %s, we received '
          'back multiple results.  Please be more specific.  Server=%s'
          % (last_patch_id, server))

    yield query, GerritPatch(result, internal)
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
    raw_ids = [x[1:] for x in internal_patches]
    parsed_patches.update(('*' + k, v) for k, v in
        _QueryGerritMultipleCurrentPatchset(raw_ids, True))

  if external_patches:
    parsed_patches.update(
        _QueryGerritMultipleCurrentPatchset(external_patches))

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


def _GetRemoteTrackingBranch(project_dir, branch):
  """Get the remote tracking branch of a local branch.

  Raises:
    cros_lib.NoTrackingBranchException if branch does not track anything.
  """
  (remote, ref) = cros_lib.GetTrackingBranch(branch, project_dir)
  return cros_lib.GetShortBranchName(remote, ref)


def _GetProjectManifestBranch(buildroot, project):
  """Get the branch specified in the manifest for the project."""
  (remote, ref) = cros_lib.GetProjectManifestBranch(buildroot,
                                                    project)
  return cros_lib.GetShortBranchName(remote, ref)


def PrepareLocalPatches(patches, manifest_branch):
  """Finish validation of parameters, and save patches to a temp folder.

  Args:
    patches:  A list of user-specified patches, in project[:branch] form.
    manifest_branch: The manifest branch of the buildroot.

  Raises:
    PatchException if:
      1. The project branch isn't specified and the project isn't on a branch.
      2. The project branch doesn't track a remote branch.
  """
  patch_info = []
  patch_root = tempfile.mkdtemp(prefix=_TRYBOT_TEMP_PREFIX)

  for patch_id in range(0, len(patches)):
    project, branch = patches[patch_id].split(':')
    project_dir = cros_lib.GetProjectDir('.', project)

    patch_dir = os.path.join(patch_root, str(patch_id))
    cmd = ['git', 'format-patch', '%s..%s' % ('m/' + manifest_branch, branch),
           '-o', patch_dir]
    cros_lib.RunCommand(cmd, redirect_stdout=True, cwd=project_dir)
    if not os.listdir(patch_dir):
      raise PatchException('No changes found in %s:%s' % (project, branch))

    # Store remote tracking branch for verification during patch stage.
    try:
      tracking_branch = _GetRemoteTrackingBranch(project_dir, branch)
    except cros_lib.NoTrackingBranchException:
      raise PatchException('%s:%s needs to track a remote branch!'
                           % (project, branch))

    patch_info.append(LocalPatch(project, tracking_branch, patch_dir, branch))

  return patch_info
