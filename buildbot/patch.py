# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module that handles the processing of patches to the source tree."""

import constants
import logging
import os
import random
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


def MakeChangeId(unusable=False):
  """Create a random Change-Id.

  Args:
    unusable: If set to True, return a Change-Id like string that gerrit
      will explicitly fail on.  This is primarily used for internal ids,
      as a fallback when a Change-Id could not be parsed.
  """
  s = "%x" % (random.randint(0, 2**160),)
  s = s.rjust(40, '0')
  if unusable:
    return 'Fake-ID %s' % s
  return 'I%s' % s


def FormatChangeId(text):
  """Format a Change-ID into a standardized form.  Use this anywhere
  we're accepting user input of Change-IDs."""
  if not text:
    raise ValueError("FormatChangeId invoked w/ an empty value: %r" % (text,))
  elif not text[0] in 'iI':
    raise ValueError("FormatChangeId invoked w/ a malformed Change-Id: %r" %
                     (text,))
  return 'I%s' % text[1:].lower()


class GitRepoPatch(object):
  """Representing a patch from a branch of a local or remote git repository."""

  # Note the selective case insensitivity; gerrit allows only this.
  _VALID_CHANGE_ID_RE = re.compile(r'^I[0-9a-fA-F]{40}$')
  _GIT_CHANGE_ID_RE = re.compile(r'^Change-Id:[\t ]*(\w+)\s*$',
                                 re.I|re.MULTILINE)
  _PALADIN_DEPENDENCY_RE = re.compile(r'^CQ-DEPEND=(.*)$', re.MULTILINE)
  _PALADIN_BUG_RE = re.compile(r'(\w+)')

  def __init__(self, project_url, project, ref, tracking_branch, sha1=None,
               change_id=None):
    """Initialization of abstract Patch class.

    Args:
      project_url: The url of the git repo (can be local or remote) to pull the
                   patch from.
      project: The name of the project that the patch applies to.
      ref: The refspec to pull from the git repo.
      tracking_branch:  The remote branch of the project the patch applies to.
      sha1: The sha1 of the commit, if known.  This *must* be accurate.  Can
        be None if not yet known- in which case Fetch will update it.
      change_id: The Change-Id of the commit, if known.  This *must* be
        accurate.  Can be None if not yet known- in which case Fetch will
        parse it.
    """
    self._commit_message = None
    self.project_url = project_url
    self.project = project
    self.ref = ref
    self.tracking_branch = os.path.basename(tracking_branch)
    self.sha1 = sha1
    # TODO(ferringb): remove this attribute.
    # apply_error_message is currently used by ValidationPool as a way to
    # track what changes have failed.  Patch objects shouldn't use this
    # attribute, instead leaving it purely for validation_pool and friends
    # to mutate.
    self.apply_error_message = None
    # change_id must always be a valid Change-Id (local or gerrit), or None.
    # id is an internal value- if change-id exists, we use that.  If not,
    # we make up a fake change-id to keep internal apis happy/simple.
    self.change_id = self.id = change_id
    self._is_fetched = set()
    self._projectdir_cache = {}

  def ProjectDir(self, checkout_root):
    """Returns the local directory where this patch will be applied."""
    checkout_root = os.path.normpath(checkout_root)
    val = self._projectdir_cache.get(checkout_root)
    if val is None:
      val = cros_lib.GetProjectDir(checkout_root, self.project)
      self._projectdir_cache[checkout_root] = val
    return val

  def Fetch(self, target_repo):
    """Fetch this patch into the target repository.

    FETCH_HEAD is implicitly reset by this operation.  Additionally,
    if the sha1 of the patch was not yet known, it is pulled and stored
    on this object and the target_repo is updated w/ the requested git
    object.

    While doing so, we'll load the commit message and Change-Id if not
    already known.

    Finally, if the sha1 is known and it's already available in the target
    repository, this will skip the actual fetch operation (it's unneeded).
    """

    target_repo = os.path.normpath(target_repo)
    if target_repo in self._is_fetched:
      return self.sha1

    def _PullData(rev):
      ret = cros_lib.RunCommandCaptureOutput(
          ['git', 'log', '--format=%H%x00%B', '-n1', rev], print_cmd=False,
          error_code_ok=True, cwd=target_repo)
      if ret.returncode != 0:
        return None, None
      output = ret.output.split('\0')
      if len(output) != 2:
        return None, None
      return [x.strip() for x in output]

    if self.sha1 is not None:
      # See if we've already got the object
      sha1, msg = _PullData(self.sha1)
      if sha1 is not None:
        assert sha1 == self.sha1
        self._commit_message = msg
        self._EnsureId(msg)
        return self.sha1

    cros_lib.RunCommand(['git', 'fetch', self.project_url, self.ref],
                        cwd=target_repo, print_cmd=False)

    sha1, msg = _PullData('FETCH_HEAD')

    # Even if we know the sha1, still do a sanity check to ensure we
    # actually just fetched it.
    if self.sha1 is not None:
      if sha1 != self.sha1:
        raise PatchException('Patch %s specifies sha1 %s, yet in fetching from '
                             '%s we could not find that sha1.  Internal error '
                             'most likely.' % (self, self.sha1, self.ref))
    else:
      self.sha1 = sha1
    self._commit_message = msg
    self._EnsureId(msg)
    self._is_fetched.add(target_repo)
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

  def _RebasePatch(self, upstream, project_dir, rev, trivial):
    """Rebase patch fetched from gerrit onto constants.PATCH_BRANCH.

    When the function completes, the constants.PATCH_BRANCH branch will be
    pointing to the rebased change.

    Arguments:
      upstream: The upstream branch to base patch on.
      project_dir: Directory of the project that is being patched.
      rev: The rev we're rebasing into the tree.
      trivial: Use trivial logic that only allows trivial merges.  Note:
        Requires Git >= 1.7.6 -- bug <.  Bots have 1.7.6 installed.

    Raises:
      ApplyPatchException: If the patch failed to apply.
    """
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

  def _GetUpstreamBranch(self, buildroot):
    """Get the branch specified in the manifest for this patch."""
    remote, ref = cros_lib.GetProjectManifestBranch(
        buildroot, self.project)
    return '%s/%s' % (remote, cros_lib.StripLeadingRefsHeads(ref))

  def ApplyIntoGitRepo(self, project_dir, upstream, trivial=False):
    """Apply patch into a standalone git repo.

    The git repo does not need to be part of a repo checkout.

    Arguments:
      project_dir: The directory of the project.
      upstream: The branch to base the patch on.
      trivial: Only allow trivial merges when applying change.
    """
    # Check that the patch is based on the same branch as project we are
    # patching into.
    # TODO(rcui): move this outside of the apply flow.
    branch_base = os.path.basename(upstream)
    if self.tracking_branch != branch_base:
      raise PatchException('branch %s for project %s is not tracking %s'
                           % (self.ref, self.project, branch_base))

    rev = self.Fetch(project_dir)

    if not cros_lib.DoesLocalBranchExist(project_dir, constants.PATCH_BRANCH):
      cros_lib.RunCommandCaptureOutput(
          ['git', 'checkout', '-b', constants.PATCH_BRANCH, '-t', upstream],
          cwd=project_dir, print_cmd=False)

    self._RebasePatch(upstream, project_dir, rev, trivial)

  def Apply(self, buildroot, trivial=False):
    """Applies the patch to specified buildroot.

      buildroot:  The buildroot.
      trivial:  Only allow trivial merges when applying change.

    Raises:
      ApplyPatchException: If the patch failed to apply.
    """
    logging.info('Attempting to apply change %s', self)
    manifest_branch = self._GetUpstreamBranch(buildroot)
    project_dir = self.ProjectDir(buildroot)
    self.ApplyIntoGitRepo(project_dir, manifest_branch, trivial)

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

    manifest_branch = self._GetUpstreamBranch(buildroot)
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
      change_id = self._ParseChangeId(patch_output)
      dependencies.append(change_id)

    if dependencies:
      logging.info('Found %s Gerrit dependencies for change %s', dependencies,
                   self)

    return dependencies

  def _EnsureId(self, commit_message):
    """Ensure we have a usable Change-Id.  This will parse the Change-Id out
    of the given commit message- if it cannot find one, it logs a warning
    and creates a fake ID.

    By it's nature, that fake ID is useless- it's created to simplify API
    usage for patch consumers.

    If CQ were to see and try operating on one of these, it would fail for
    example."""
    if self.id is not None:
      return self.id
    try:
      self.change_id = self.id = self._ParseChangeId(commit_message)
    except MissingChangeIDException:
      logging.warning(
          'Change %s, sha1 %s lacks a change-id in its commit '
          'message.  CQ-DEPEND against this rev will not work, nor '
          'will any gerrit querying.  Please add the appropriate '
          'Change-Id into the commit message to resolve this.',
          self, self.sha1)

      # We still need an internal id to address it via- thus we
      # make a fake one to keep our caches/resolvers happy, while
      # leaving the authoritive .change_id set to None.
      self.id = MakeChangeId(unusable=True)
    return self.id

  def _ParseChangeId(self, data):
    """Parse a Change-Id out of a block of text."""
    # Grab just the last pararaph.
    git_metadata = re.split('\n{2,}', data.rstrip())[-1]
    change_id_match = self._GIT_CHANGE_ID_RE.findall(git_metadata)
    if not change_id_match:
      raise MissingChangeIDException('Missing Change-Id in %s', data)

    # Now, validate it.  This has no real effect on actual gerrit patches,
    # but for local patches the validation is useful for general sanity
    # enforcement.
    change_id_match = change_id_match[-1]
    if not self._VALID_CHANGE_ID_RE.match(change_id_match):
      raise BrokenChangeID(self, change_id_match)

    # Force a standard for the formatting; I must be caps, force the rest
    # of the hex to lower case.
    return FormatChangeId(change_id_match)

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
    self.Fetch(self.ProjectDir(buildroot))
    matches = self._PALADIN_DEPENDENCY_RE.findall(self._commit_message)
    for match in matches:
      chunks = ' '.join(match.split(','))
      chunks = chunks.split()
      for chunk in chunks:
        if not chunk.isdigit():
          if not self._VALID_CHANGE_ID_RE.match(chunk):
            raise BrokenCQDepends(self, match, chunk)
          chunk = FormatChangeId(chunk)
        if chunk not in dependencies:
          dependencies.append(chunk)

    if dependencies:
      logging.info('Found %s Paladin dependencies for change %s', dependencies,
                   self)
    return dependencies

  def __str__(self):
    """Returns custom string to identify this patch."""
    s = '%s:%s' % (self.project, self.ref)
    if self.sha1 is not None:
      s = '%s:%s' % (s, self.sha1[:8])
    return s


class LocalPatch(GitRepoPatch):
  """Represents patch coming from an on-disk git repo."""

  def __init__(self, project_url, project, ref, tracking_branch, sha1,
               sourceroot):
    GitRepoPatch.__init__(self, project_url, project, ref, tracking_branch,
                          sha1=sha1)
    self.sourceroot = sourceroot

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
    project_dir = self.ProjectDir(self.sourceroot)
    hash_fields = [('tree_hash', '%T'), ('parent_hash', '%P')]
    transfer_fields = [('GIT_AUTHOR_NAME', '%an'),
                       ('GIT_AUTHOR_EMAIL', '%ae'),
                       ('GIT_AUTHOR_DATE', '%ad'),
                       ('GIT_COMMITTER_NAME', '%cn'),
                       ('GIT_COMMITTER_EMAIL', '%ce'),
                       ('GIT_COMMITER_DATE', '%ct')]
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

    # Reset the commit date to a value that can't conflict; if we
    # leave this to git, it's possible for a fast moving set of commit/uploads
    # to all occur within the same second (thus the same commit date),
    # resulting in the same sha1.
    extra_env['GIT_COMMITTER_DATE'] = str(
        int(extra_env["GIT_COMMITER_DATE"]) - 1)

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

  def Upload(self, remote_ref, dryrun=False, push_url=None):
    """Upload the patch to a remote git branch.

    Arguments:
      remote_ref: The ref on the remote host to push to.
      dryrun: Do the git push with --dry-run
      push_url: Which url to push to.  If unspecified, defaults to deciding
        between external gerrit or internal gerrit.
    """
    if push_url is None:
      push_url = constants.GERRIT_SSH_URL
      if cros_lib.IsProjectInternal(self.sourceroot, self.project):
        push_url = constants.GERRIT_INT_SSH_URL
      push_url = os.path.join(push_url, self.project)

    carbon_copy = self._GetCarbonCopy()
    cmd = ['git', 'push', push_url, '%s:%s' % (carbon_copy, remote_ref)]
    if dryrun:
      cmd.append('--dry-run')

    cros_lib.RunCommand(cmd, cwd=self.ProjectDir(self.sourceroot))


class UploadedLocalPatch(GitRepoPatch):
  """Represents an uploaded local patch passed in using --remote-patch."""
  def __init__(self, project_url, project, ref, tracking_branch,
               original_branch, original_sha1, carbon_copy_sha1):
    GitRepoPatch.__init__(self, project_url, project, ref, tracking_branch,
                          sha1=carbon_copy_sha1)
    self.original_branch = original_branch
    self.original_sha1 = original_sha1

  def __str__(self):
    """Returns custom string to identify this patch."""
    # TODO(ferringb): fold subject line in here.
    s = '%s:%s:%s' % (self.project, self.original_branch,
                      self.original_sha1[:8])
    return s


class GerritPatch(GitRepoPatch):
  """Object that represents a Gerrit CL."""
  _PUBLIC_URL = os.path.join(constants.GERRIT_HTTP_URL, 'gerrit/p')

  def __init__(self, patch_dict, internal):
    """Construct a GerritPatch object from Gerrit query results.

    Gerrit query JSON fields are documented at:
    http://gerrit-documentation.googlecode.com/svn/Documentation/2.2.1/json.html

    Args:
      patch_dict: A dictionary containing the parsed JSON gerrit query results.
      internal: Whether the CL is an internal CL.
    """
    self.patch_dict = patch_dict
    super(GerritPatch, self).__init__(
        self._GetProjectUrl(patch_dict['project'], internal),
        patch_dict['project'],
        patch_dict['currentPatchSet']['ref'],
        patch_dict['branch'],
        sha1=patch_dict['currentPatchSet']['revision'],
        change_id=FormatChangeId(patch_dict['id']))

    self.internal = internal
    # id - The CL's ChangeId
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

  def __reduce__(self):
    """Used for pickling to re-create patch object."""
    return self.__class__, (self.patch_dict.copy(), self.internal)

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

  def _EnsureId(self, commit_message):
    """Ensure we have a usable Change-Id, validating what we received
    from gerrit against what the commit message states."""
    # GerritPatch instances get their Change-Id from gerrit
    # directly; for this to fail, there is an internal bug.
    assert self.id is not None

    # For GerritPatches, we still parse the ID- this is
    # primarily so we can throw an appropriate warning,
    # and also validate our parsing against gerrit's in
    # the process.
    try:
      parsed_id = self._ParseChangeId(commit_message)
      if parsed_id != self.change_id:
        raise AssertionError(
            'For Change-Id %s, sha %s, our parsing of the Change-Id did not '
            'match what gerrit told us.  This is an internal bug: either our '
            "parsing no longer matches gerrit's, or somehow this instance's "
            'stored change_id was invalidly modified.  Our parsing of the '
            'Change-Id yielded: %s'
            % (self.change_id, self.sha1, parsed_id))

    except MissingChangeIDException:
      logging.warning(
          'Change %s, Change-Id %s, sha1 %s lacks a change-id in its commit '
          'message.  This breaks the ability for any dependencies '
          'to be committed as a batch (instead having to be committed one '
          'by one through CQ).  Please add the appropriate '
          'Change-Id into the commit message to resolve this.',
          self, self.change_id, self.sha1)

    return self.id

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


def PrepareLocalPatches(sourceroot, patches, manifest_branch):
  """Finish validation of parameters, and save patches to a temp folder.

  Args:
    sourceroot: The repo where local patches come from.
    patches:  A list of user-specified patches, in project[:branch] form.
    manifest_branch: The manifest branch of the buildroot.

  Raises:
    PatchException if:
      1. The project branch isn't specified and the project isn't on a branch.
  """
  patch_info = []
  for patch in patches:
    project, branch = patch.split(':')
    project_dir = cros_lib.GetProjectDir(sourceroot, project)

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

    patch_info.append(LocalPatch(os.path.join(project_dir, '.git'),
                                        project, branch, tracking_branch,
                                        sha1, sourceroot))

  return patch_info


def PrepareRemotePatches(patches):
  """Generate patch objects from list of --remote-patch parameters.

  Args:
    patches: A list of --remote-patches strings that the user specified on
             the commandline.  Patch strings are colon-delimited.  Patches come
             in the format
             <project>:<original_branch>:<ref>:<tracking_branch>:<tag>.
             A description of each element:
             project: The manifest project name that the patch is for.
             original_branch: The name of the development branch that the local
                              patch came from.
             ref: The remote ref that points to the patch.
             tracking_branch: The upstream branch that the original_branch was
                              tracking.  Should be a manifest branch.
             tag: Denotes whether the project is an internal or external
                  project.
  """
  patch_info = []
  for patch in patches:
    try:
      project, original_branch, ref, tracking_branch, tag = patch.split(':')
    except ValueError:
      raise PatchException("Unexpected tryjob format.  You may be running an "
                           "older version of chromite.  Run 'repo sync "
                           "chromiumos/chromite'.")

    if tag == constants.EXTERNAL_PATCH_TAG:
      push_url = constants.GERRIT_SSH_URL
    elif tag == constants.INTERNAL_PATCH_TAG:
      push_url = constants.GERRIT_INT_SSH_URL
    else:
      raise PatchException('Bad remote patch format.  Unknown tag %s' % tag)

    patch_info.append(UploadedLocalPatch(os.path.join(push_url, project),
                                         project, ref, tracking_branch,
                                         original_branch,
                                         os.path.basename(ref)))

  return patch_info
