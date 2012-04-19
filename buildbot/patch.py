# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module that handles the processing of patches to the source tree."""

import constants
import logging
import os
import re

from chromite.lib import cros_build_lib as cros_lib


class PatchException(Exception):
  """Exception thrown by GetGerritPatchInfo."""


class ApplyPatchException(Exception):
  """Exception thrown if we fail to apply a patch."""

  def __init__(self, patch, inflight=False):
    super(ApplyPatchException, self).__init__()
    self.patch = patch
    self.inflight = inflight

  def __str__(self):
    return 'Failed to apply patch %s to %s' % (
        self.patch, 'current patch series' if self.inflight else 'ToT')


class MalformedChange(Exception):
  pass

class MissingChangeIDException(MalformedChange):
  """Raised if a patch is missing a Change-ID."""


class BrokenCQDepends(MalformedChange):
  """Raised if a patch has a CQ-DEPEND line that is ill formated."""


class BrokenChangeID(MalformedChange):
  """Raised if a patch has an invalid Change-ID."""


class GitRepoPatch(object):
  """Representing a patch from a branch of a local or remote git repository."""

  def __init__(self, project_url, project, ref, tracking_branch, sha1=None):
    """Initialization of abstract Patch class.

    Args:
      project_url: The url of the git repo (can be local or remote) to pull the
                   patch from.
      project: The name of the project that the patch applies to.
      ref: The refspec to pull from the git repo.
      tracking_branch:  The remote branch of the project the patch applies to.
      sha1: The sha1 of the commit, if known.  This *must* be accurate.  Can
        be None if not yet known- which case Fetch will update it.
    """
    self.project_url = project_url
    self.project = project
    self.ref = ref
    self.tracking_branch = os.path.basename(tracking_branch)
    self.sha1 = sha1

  def ProjectDir(self, buildroot):
    """Returns the local directory where this patch will be applied."""
    return cros_lib.GetProjectDir(buildroot, self.project)

  def Fetch(self, target_repo):
    """Fetch this patch into the target repository.

    FETCH_HEAD is implicitly reset by this operation.  Additionally,
    if the sha1 of the patch was not yet known, it is pulled and stored
    on this object.

    Finally, if the sha1 is known and it's already available in the target
    repository, this will skip the actual fetch operation (it's unneeded).
    """

    if self.sha1 is not None:
      ret = cros_lib.RunCommandCaptureOutput(
          ['git', 'show', self.sha1], cwd=target_repo, print_cmd=False,
          error_code_ok=True).returncode
      if ret == 0:
        return self.sha1

    cros_lib.RunCommand(['git', 'fetch', self.project_url, self.ref],
                        cwd=target_repo, print_cmd=False)

    # Even if we know the sha1, still do a sanity check to ensure we
    # actually just fetched it.
    ret = cros_lib.RunCommand(['git', 'rev-list', '-n1', 'FETCH_HEAD'],
                              cwd=target_repo, redirect_stdout=True,
                              print_cmd=False)
    ret = ret.output.strip()
    if self.sha1 is not None:
      if ret != self.sha1:
        raise PatchException('Patch %s specifies sha1 %s, yet in fetching from '
                             '%s we could not find that sha1.  Internal error '
                             'most likely.' % (self, self.sha1, self.ref))
    else:
      self.sha1 = ret
    return self.sha1

  def _RebaseOnto(self, branch, upstream, project_dir, rev, trivial):
    """Attempts to rebase the given rev into branch.

    Raises:
      cros_lib.RunCommandError:  If the rebase operation returns an error code.
        In this case, we still rebase --abort (or equivalent) before
        returning.
    """
    try:
      cmd = ['git', 'rebase']
      if trivial:
        cmd.extend(['--strategy', 'resolve', '-X', 'trivial'])
      cmd.extend(['--onto', branch, upstream, rev])

      # Run the rebase command.
      cros_lib.RunCommand(cmd, cwd=project_dir, print_cmd=False)

    except cros_lib.RunCommandError:
      cros_lib.RunCommand(['git', 'rebase', '--abort'],
                          cwd=project_dir, print_cmd=False, error_code_ok=True)
      raise

  def _RebasePatch(self, buildroot, project_dir, rev, trivial):
    """Rebase patch fetched from gerrit onto constants.PATCH_BRANCH.

    When the function completes, the constants.PATCH_BRANCH branch will be
    pointing to the rebased change.

    Arguments:
      buildroot: The buildroot.
      project_dir: Directory of the project that is being patched.
      rev: The rev we're rebasing into the tree.
      trivial: Use trivial logic that only allows trivial merges.  Note:
        Requires Git >= 1.7.6 -- bug <.  Bots have 1.7.6 installed.

    Raises:
      ApplyPatchException: If the patch failed to apply.
    """

    upstream = _GetProjectManifestBranch(buildroot, self.project)

    try:
      self._RebaseOnto(constants.PATCH_BRANCH, upstream, project_dir, rev,
                       trivial)
    except cros_lib.RunCommandError:
      # Ignore this error; next we fiddle with the patch a bit to
      # discern what exception to throw.
      pass
    else:
      cros_lib.RunCommand(['git', 'checkout', '-B', constants.PATCH_BRANCH,
                          'HEAD'], cwd=project_dir, print_cmd=False)
      return

    try:
      self._RebaseOnto(upstream, upstream, project_dir, rev, trivial)
      raise ApplyPatchException(self, inflight=True)
    except cros_lib.RunCommandError:
      # Cleanup the rebase attempt.
      cros_lib.RunCommandCaptureOutput(
          ['git', 'rebase', '--abort'], cwd=project_dir, print_cmd=False,
          error_code_ok=True)
      raise ApplyPatchException(self, inflight=False)

    finally:
      # Ensure we're on the correct branch on the way out.
      cros_lib.RunCommand(['git', 'checkout', constants.PATCH_BRANCH],
                          cwd=project_dir, print_cmd=False)

  def Apply(self, buildroot, trivial=False):
    """Applies the patch to specified buildroot.

      buildroot:  The buildroot.
      trivial:  Only allow trivial merges when applying change.

    Raises:
      ApplyPatchException: If the patch failed to apply.
    """
    logging.info('Attempting to apply change %s', self)
    manifest_branch = _GetProjectManifestBranch(buildroot, self.project)
    manifest_branch_base = os.path.basename(manifest_branch)
    if self.tracking_branch != manifest_branch_base:
      raise PatchException('branch %s for project %s is not tracking %s'
                           % (self.ref, self.project,
                              manifest_branch_base))

    project_dir = self.ProjectDir(buildroot)

    rev = self.Fetch(project_dir)

    if not cros_lib.DoesLocalBranchExist(project_dir, constants.PATCH_BRANCH):
      upstream = cros_lib.GetManifestDefaultBranch(buildroot)
      cros_lib.RunCommand(['git', 'checkout', '-b', constants.PATCH_BRANCH,
                           '-t', 'm/' + upstream], cwd=project_dir,
                          print_cmd=False)
    self._RebasePatch(buildroot, project_dir, rev, trivial)

  def __str__(self):
    """Returns custom string to identify this patch."""
    s = '%s:%s' % (self.project, self.ref)
    if self.sha1 is not None:
      s = '%s:%s' % (s, self.sha1[:8])
    return s


class LocalGitRepoPatch(GitRepoPatch):
  """Represents patch coming from an on-disk git repo."""

  def __init__(self, project_url, project, ref, tracking_branch, sha1):
    GitRepoPatch.__init__(self, project_url, project, ref, tracking_branch,
                          sha1=sha1)

  def Sha1Hash(self):
    """Get the Sha1 of the branch."""
    return self.sha1

  def _GetCarbonCopy(self):
    """Returns a copy of this commit object, with a different sha1.

    This is used to work around a Gerrit bug, where a commit object cannot be
    uploaded for review if an existing branch (in refs/tryjobs/*) points to
    that same sha1.  So instead we create a copy of the commit object and upload
    that to refs/tryjobs/*.

    Returns:
      The sha1 of the new commit object.
    """
    project_dir = self.ProjectDir(constants.SOURCE_ROOT)
    hash_fields = [('tree_hash', '%T'), ('parent_hash', '%P')]
    transfer_fields = [('GIT_AUTHOR_NAME', '%an'),
                       ('GIT_AUTHOR_EMAIL', '%ae'),
                       ('GIT_AUTHOR_DATE', '%ad'),
                       ('GIT_COMMITTER_NAME', '%cn'),
                       ('GIT_COMMITTER_EMAIL', '%ce')]
    fields = hash_fields + transfer_fields

    format_string = '%n'.join([code for _, code in fields] + ['%B'])
    result = cros_lib.RunCommand(['git', 'log', '--format=%s' % format_string,
                                  '-z', '-n1', self.sha1],
                                  cwd=project_dir, redirect_stdout=True)
    lines = result.output.splitlines()
    field_value = dict(zip([name for name, _ in fields],
                           [line.strip() for line in lines]))
    commit_body = '\n'.join(lines[len(fields):])

    if len(field_value['parent_hash'].split()) != 1:
      raise PatchException('Branch %s:%s contains merge result %s!'
                           % (self.project, self.ref, self.sha1))

    extra_env = dict([(field, field_value[field]) for field, _ in
                      transfer_fields])
    result = cros_lib.RunCommand(['git', 'commit-tree',
                                  field_value['tree_hash'], '-p',
                                  field_value['parent_hash']], cwd=project_dir,
                                  redirect_stdout=True, extra_env=extra_env,
                                  input=commit_body)

    new_sha1 = result.output.strip()
    if new_sha1 == self.sha1:
      raise PatchException('Internal error!  Carbon copy of %s is the same as '
                           'original!' % self.sha1)

    return new_sha1

  def Upload(self, remote_ref, dryrun=False):
    """Upload the patch to a remote git branch.

    Arguments:
      remote_ref: The ref on the remote host to push to.
      dryrun: Do the git push with --dry-run
    """
    push_url = constants.GERRIT_SSH_URL
    if cros_lib.IsProjectInternal(constants.SOURCE_ROOT, self.project):
      push_url = constants.GERRIT_INT_SSH_URL

    carbon_copy = self._GetCarbonCopy()
    cmd = ['git', 'push', os.path.join(push_url, self.project),
           '%s:%s' % (carbon_copy, remote_ref)]
    if dryrun:
      cmd.append('--dry-run')

    cros_lib.RunCommand(cmd, cwd=self.ProjectDir(constants.SOURCE_ROOT))


class GerritPatch(GitRepoPatch):
  """Object that represents a Gerrit CL."""
  _PUBLIC_URL = os.path.join(constants.GERRIT_HTTP_URL, 'gerrit/p')
  # Note the selective case insensitivity; gerrit allows only this.
  _VALID_CHANGE_ID_RE = re.compile(r'^I[0-9a-fA-F]{40}$')
  _GIT_CHANGE_ID_RE = re.compile(r'^Change-Id:[\t ]*(\w+)\s*$',
                                 re.I|re.MULTILINE)
  _PALADIN_DEPENDENCY_RE = re.compile(r'^CQ-DEPEND=(.*)$', re.MULTILINE)
  _PALADIN_BUG_RE = re.compile('(\w+)')

  def __init__(self, patch_dict, internal):
    """Construct a GerritPatch object from Gerrit query results.

    Gerrit query JSON fields are documented at:
    http://gerrit-documentation.googlecode.com/svn/Documentation/2.2.1/json.html

    Args:
      patch_dict: A dictionary containing the parsed JSON gerrit query results.
      internal: Whether the CL is an internal CL.
    """
    super(GerritPatch, self).__init__(
        self._GetProjectUrl(patch_dict['project'], internal),
        patch_dict['project'],
        patch_dict['currentPatchSet']['ref'],
        patch_dict['branch'],
        sha1=patch_dict['currentPatchSet']['revision'])

    self.patch_dict = patch_dict
    self.internal = internal
    # id - The CL's ChangeId
    self.id = patch_dict['id']
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

  def __getnewargs__(self):
    """Used for pickling to re-create patch object."""
    return self.patch_dict, self.internal

  def IsAlreadyMerged(self):
    """Returns whether the patch has already been merged in Gerrit."""
    return self.status == 'MERGED'

  @classmethod
  def _GetProjectUrl(cls, project, internal):
    """Returns the url to the gerrit project."""
    if internal:
      url_prefix = constants.GERRIT_INT_SSH_URL
    else:
      url_prefix = cls._PUBLIC_URL

    return os.path.join(url_prefix, project)

  def CommitMessage(self, buildroot):
    """Returns the commit message for the patch as a string."""
    project_dir = self.ProjectDir(buildroot)

    rev = self.Fetch(project_dir)

    return_obj = cros_lib.RunCommand(['git', 'log', '--format=%B', '-n1', rev],
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
    logging.info('Checking for Gerrit dependencies for change %s', self)
    project_dir = self.ProjectDir(buildroot)

    rev = self.Fetch(project_dir)

    manifest_branch = _GetProjectManifestBranch(buildroot, self.project)
    return_obj = cros_lib.RunCommand(
        ['git', 'log', '--format=%B%x00', '%s..%s^' % (manifest_branch, rev)],
        cwd=project_dir, redirect_stdout=True, print_cmd=False)

    patches = []
    if return_obj.output:
      # Only do this if we have output; else it leads
      # to an invalid [''] result which we can't identify
      # as differing from actual output for a single patch that
      # lacks a commit message.
      # Because the explicit null addition, strip off the last record.
      patches = return_obj.output.split('\0')[:-1]

    for patch_output in patches:
      # Grab just the last pararaph.
      git_metadata = filter(None, re.split('\n{2,}', patch_output))[-1]
      change_id_match = self._GIT_CHANGE_ID_RE.findall(git_metadata)
      if not change_id_match:
        raise MissingChangeIDException('Missing Change-Id in %s' % patch_output)

      # Now, validate it.  This has no real effect on actual gerrit patches,
      # but for local patches the validation is useful for general sanity
      # enforcement.
      change_id_match = change_id_match[-1]
      if not self._VALID_CHANGE_ID_RE.match(change_id_match):
        raise BrokenChangeID(self, change_id_match)
      dependencies.append(change_id_match)

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
      chunks = ' '.join(match.split(','))
      chunks = chunks.split()
      for chunk in chunks:
        if not (self._VALID_CHANGE_ID_RE.match(chunk) or chunk.isdigit()):
          raise BrokenCQDepends(self, match, chunk)

      dependencies.extend(chunks)

    if dependencies:
      logging.info('Found %s Paladin dependencies for change %s', dependencies,
                   self)
    return dependencies

  def __str__(self):
    """Returns custom string to identify this patch."""
    s = '%s:%s' % (self.owner, self.gerrit_number)
    if self.sha1 is not None:
      s = '%s:%s' % (s, self.sha1[:8])
    return s

  # Define methods to use patches in sets.  We uniquely identify patches
  # by Gerrit change numbers.
  def __hash__(self):
    return hash(self.id)

  def __eq__(self, other):
    return self.id == other.id


def _GetRemoteTrackingBranch(project_dir, branch):
  """Get the remote tracking branch of a local branch.

  Raises:
    cros_lib.NoTrackingBranchException if branch does not track anything.
  """
  (remote, ref) = cros_lib.GetTrackingBranch(branch, project_dir)
  return '%s/%s' % (remote, cros_lib.StripLeadingRefsHeads(ref))


def _GetProjectManifestBranch(buildroot, project):
  """Get the branch specified in the manifest for the project."""
  (remote, ref) = cros_lib.GetProjectManifestBranch(buildroot,
                                                    project)
  return '%s/%s' % (remote, cros_lib.StripLeadingRefsHeads(ref))


def PrepareLocalPatches(patches, manifest_branch):
  """Finish validation of parameters, and save patches to a temp folder.

  Args:
    patches:  A list of user-specified patches, in project[:branch] form.
    manifest_branch: The manifest branch of the buildroot.

  Raises:
    PatchException if:
      1. The project branch isn't specified and the project isn't on a branch.
  """
  patch_info = []
  for patch in patches:
    project, branch = patch.split(':')
    project_dir = cros_lib.GetProjectDir(constants.SOURCE_ROOT, project)

    cmd = ['git', 'rev-list', '-n1', '%s..%s'
           % ('m/' + manifest_branch, branch)]
    result = cros_lib.RunCommand(cmd, redirect_stdout=True, cwd=project_dir)
    sha1 = result.output.strip()

    if not sha1:
      raise PatchException('No changes found in %s:%s' % (project, branch))

    # Store remote tracking branch for verification during patch stage.
    try:
      tracking_branch = _GetRemoteTrackingBranch(project_dir, branch)
    except cros_lib.NoTrackingBranchException:
      raise PatchException('%s:%s needs to track a remote branch!'
                           % (project, branch))

    patch_info.append(LocalGitRepoPatch(os.path.join(project_dir, '.git'),
                                        project, branch, tracking_branch,
                                        sha1))

  return patch_info
