# Copyright (c) 2011-2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module that handles the processing of patches to the source tree."""

import constants
import logging
import os
import random
import re

from chromite.lib import cros_build_lib


class PatchException(Exception):
  """Base exception class all patch exception derive from."""

  # Unless instances override it, default all exceptions to ToT.
  inflight = False

  def __init__(self, patch, message=None):
    Exception.__init__(self)
    self.patch = patch
    self.message = message
    self.args = (patch,)
    if message is not None:
      self.args += (message,)

  def __str__(self):
    return "Change %s failed: %s" % (self.patch, self.message)


class ApplyPatchException(PatchException):
  """Exception thrown if we fail to apply a patch."""

  def __init__(self, patch, inflight=False):
    PatchException.__init__(self, patch)
    self.inflight = inflight
    self.args += (inflight,)

  def __str__(self):
    return 'Change %s no longer cleanly applies against %s.' % (
        self.patch, 'current patch series' if self.inflight else 'ToT')


class DependencyError(PatchException):

  """Exception thrown when a change cannot be applied due to a failure in a
  dependency."""

  def __init__(self, patch, error):
    """
    Args:
      patch: The GitRepoPatch instance that this exception concerns.
      error: A PatchException object that can be stringified to describe
        the error.
    """
    PatchException.__init__(self, patch)
    self.inflight = error.inflight
    self.error = error
    self.args += (error,)

  def __str__(self):
    return "Patch %s depends on %s which has an error: %s" % (
        self.patch, self.error.patch.id, self.error)


class BrokenCQDepends(PatchException):
  """Raised if a patch has a CQ-DEPEND line that is ill formated."""

  def __init__(self, patch, text):
    PatchException.__init__(self, patch)
    self.text = text
    self.args += (text,)

  def __str__(self):
    return "Change %s has a malformed CQ-DEPEND target: %s" % (
        self.patch, self.text)


class BrokenChangeID(PatchException):
  """Raised if a patch has an invalid or missing Change-ID."""

  def __init__(self, patch, message):
    PatchException.__init__(self, patch)
    self.message = message
    self.args += (message,)

  def __str__(self):
    return "Change %s has a broken ChangeId: %s" % (self.patch, self.message)


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


def FormatChangeId(text, force_internal=False, force_external=False,
                   strict=False):
  """Format a Change-Id into a standardized form.

  Use this anywhere we're accepting user input of Change-IDs.

  Args:
    text: The given Change-Id to inspect and reformat.
    force_internal: If True, force the resultant Change-Id to be an internally
      formatted one.
    force_external: If True, force the resultant Change-Id to be an externally
      formatted one; this form is the gerrit standard, thus if you need to
      convert a ChangeId for passing directly to gerrit, this should be set to
      True.  Note that force_internal force_external are mutually exclusive.
    strict: If True, then it's an error if the text holds an internally
      formatted ChangeId.  Generally this is only useful if you're working w/
      a direct git commit message and are trying to ensure the message is
      gerrit compatible.
  """
  if force_internal and force_external:
    raise TypeError("FormatChangeId: either force_internal or force_external "
                    "can be set to True, but not both.")

  if not text:
    raise ValueError("FormatChangeId invoked w/ an empty value: %r" % (text,))

  original_text = text
  prefix = '*' if force_internal else ''
  if text[0].startswith('*'):
    if strict:
      raise ValueError("FormatChangeId invoked w/ an internally formatted "
                       "Change-Id while in strict mode: %r" % (original_text,))
    if not force_external:
      prefix = '*'
    text = text[1:]

  if text[0] not in 'iI' or len(text) > 41:
    raise ValueError("FormatChangeId invoked w/ a malformed Change-Id: %r" %
                     (original_text,))
  elif strict:
    if text[0] == 'i':
      raise ValueError("FormatChangeId invoked w/ a malformed Change-Id, "
                       "leading 'i' char needs to be I: %r" % (original_text,))
    elif len(text) != 41:
      raise ValueError("FormatChangeId invoked w/ a malformed Change-Id, "
                       "value isn't 41 characters: %r" % (original_text,))

  # Drop the leading I.
  text = text[1:].lower()
  try:
    _ = int(text, 16)
  except ValueError:
    raise ValueError("FormatChangeId invoked w/ a non hex ChangeId value: %s"
                     % original_text)

  return '%sI%s' % (prefix, text)


class GitRepoPatch(object):
  """Representing a patch from a branch of a local or remote git repository."""

  # Note the selective case insensitivity; gerrit allows only this.
  # TOOD(ferringb): back VALID_CHANGE_ID_RE down to {8,40}, requires
  # ensuring CQ's internals can do the translation (almost can now,
  # but will fail in the case of a CQ-DEPEND on a change w/in the
  # same pool).
  _VALID_CHANGE_ID_RE = re.compile(r'^\*?I[0-9a-fA-F]{40}$')
  _STRICT_VALID_CHANGE_ID_RE = re.compile(r'^I[0-9a-fA-F]{40}$')
  _GIT_CHANGE_ID_RE = re.compile(r'^Change-Id:[\t ]*(\w+)\s*$',
                                 re.I|re.MULTILINE)
  _PALADIN_DEPENDENCY_RE = re.compile(r'^CQ-DEPEND=(.*)$', re.MULTILINE)
  _PALADIN_BUG_RE = re.compile(r'(\w+)')

  def __init__(self, project_url, project, ref, tracking_branch, internal,
               sha1=None, change_id=None):
    """Initialization of abstract Patch class.

    Args:
      project_url: The url of the git repo (can be local or remote) to pull the
                   patch from.
      project: The name of the project that the patch applies to.
      ref: The refspec to pull from the git repo.
      tracking_branch:  The remote branch of the project the patch applies to.
      sha1: The sha1 of the commit, if known.  This *must* be accurate.  Can
        be None if not yet known- in which case Fetch will update it.
      change_id: If given, this must be a strict gerrit format (external format)
        Change-Id representing this change.
    """
    self._commit_message = None
    self.project_url = project_url
    self.project = project
    self.ref = ref
    self.tracking_branch = os.path.basename(tracking_branch)
    self.internal = internal
    self.sha1 = sha1
    # TODO(ferringb): remove this attribute.
    # apply_error_message is currently used by ValidationPool as a way to
    # track what changes have failed.  Patch objects shouldn't use this
    # attribute, instead leaving it purely for validation_pool and friends
    # to mutate.
    self.apply_error_message = None
    # change_id must always be a valid Change-Id (local or gerrit), or None,
    # and be in strict gerrit form (aka, external).
    # id can be in our form; internal or external.
    self.change_id = self.id = None
    self._is_fetched = set()
    self._projectdir_cache = {}

    # Do the ChangeId setting now that we have a fully setup instance.
    if change_id:
      self._SetChangeId(change_id)

  def ProjectDir(self, checkout_root):
    """Returns the local directory where this patch will be applied."""
    checkout_root = os.path.normpath(checkout_root)
    val = self._projectdir_cache.get(checkout_root)
    if val is None:
      val = cros_build_lib.GetProjectDir(checkout_root, self.project)
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
      ret = cros_build_lib.RunCommandCaptureOutput(
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

    cros_build_lib.RunCommand(['git', 'fetch', self.project_url, self.ref],
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
      cros_build_lib.RunCommandError:  If the rebase operation returns an error code.
        In this case, we still rebase --abort (or equivalent) before
        returning.
    """
    try:
      cmd = ['git', 'rebase']
      if trivial:
        cmd.extend(['--strategy', 'resolve', '-X', 'trivial'])
      cmd.extend(['--onto', branch, upstream, rev])

      # Run the rebase command.
      cros_build_lib.RunCommand(cmd, cwd=project_dir, print_cmd=False)

    except cros_build_lib.RunCommandError:
      cros_build_lib.RunCommand(['git', 'rebase', '--abort'],
                                cwd=project_dir, print_cmd=False,
                                error_code_ok=True)
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
    except cros_build_lib.RunCommandError:
      # Ignore this error; next we fiddle with the patch a bit to
      # discern what exception to throw.
      pass
    else:
      cros_build_lib.RunCommand(
          ['git', 'checkout', '-B', constants.PATCH_BRANCH, 'HEAD'],
          cwd=project_dir, print_cmd=False)
      return

    try:
      self._RebaseOnto(upstream, upstream, project_dir, rev, trivial)
      raise ApplyPatchException(self, inflight=True)
    except cros_build_lib.RunCommandError:
      # Cleanup the rebase attempt.
      cros_build_lib.RunCommandCaptureOutput(
          ['git', 'rebase', '--abort'], cwd=project_dir, print_cmd=False,
          error_code_ok=True)
      raise ApplyPatchException(self, inflight=False)

    finally:
      # Ensure we're on the correct branch on the way out.
      cros_build_lib.RunCommand(['git', 'checkout', constants.PATCH_BRANCH],
                                cwd=project_dir, print_cmd=False)

  def _GetUpstreamBranch(self, buildroot, force_default=False):
    """Get the branch specified in the manifest for this patch."""
    # TODO(ferringb): remove this via cherry-picking; it's broken
    # now since it assumes the tracking_branch (which is local
    # to the originating repo) is what should be used for rebasing
    # this patch into the current tree).
    manifest = cros_build_lib.ManifestCheckout.Cached(buildroot)
    if force_default:
      return manifest.default_branch
    return manifest.GetProjectsLocalRevision(self.project)

  def ApplyIntoGitRepo(self, project_dir, upstream, trivial=False,
                       do_check=True):
    """Apply patch into a standalone git repo.

    The git repo does not need to be part of a repo checkout.

    Arguments:
      project_dir: The directory of the project.
      upstream: The branch to base the patch on.
      trivial: Only allow trivial merges when applying change.
      do_check: Check if tracking is the same as the given upstream.
    """
    # Check that the patch is based on the same branch as project we are
    # patching into.
    # TODO(ferringb): remove this when cherry-pick lands; has questionable
    # value now, has no relevance/value once cherry-picking is in.
    branch_base = os.path.basename(upstream)
    if self.tracking_branch != branch_base and do_check:
      raise PatchException('branch %s for project %s is not tracking %s'
                           % (self.ref, self.project, branch_base))

    rev = self.Fetch(project_dir)

    if not cros_build_lib.DoesLocalBranchExist(project_dir,
                                               constants.PATCH_BRANCH):
      cmd = ['checkout', '-b', constants.PATCH_BRANCH, '-t', upstream]
      cros_build_lib.RunGitCommand(project_dir, cmd)

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
    do_check = manifest_branch.startswith('refs/')
    if not do_check:
      # Revision locked.  Use the manifest default branch which
      # is set to the manifest locked revision, and
      # suppress the tracking/upstream check.
      manifest_branch = self._GetUpstreamBranch(buildroot, True)
    project_dir = self.ProjectDir(buildroot)
    self.ApplyIntoGitRepo(project_dir, manifest_branch, trivial,
                          do_check=do_check)

  def GerritDependencies(self, buildroot):
    """Returns an ordered list of dependencies from Gerrit.

    The list of changes are in order from FETCH_HEAD back to m/master.

    Arguments:
      buildroot: The buildroot.
    Returns:
      An ordered list of Gerrit revisions that this patch depends on.
    Raises:
      BrokenChangeID: If a dependent change is missing its ChangeID.
    """
    dependencies = []
    logging.info('Checking for Gerrit dependencies for change %s', self)
    project_dir = self.ProjectDir(buildroot)

    rev = self.Fetch(project_dir)

    tracking_branch = self._GetUpstreamBranch(buildroot)
    try:
      return_obj = cros_build_lib.RunGitCommand(
          project_dir,
          ['log', '--format=%B%x00', '%s..%s^' % (tracking_branch, rev)])
    except cros_build_lib.RunCommandError, e:
      if e.result.returncode != 128:
        raise
      # Errorcode 128 means "object not found"; either we've got an
      # internal bug (tracking_branch was wrong, fetch somehow didn't get
      # that actual rev, etc), or... this is the first commit in a repository.
      # The following code checks for that, raising the original
      # exception if not.
      result = cros_build_lib.RunGitCommand(project_dir,
                                            ['rev-list', '-n2', rev])
      if len(result.output.split()) != 1:
        raise
      # First commit of a repository; obviously, it has no dependencies.
      return []

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
      # Ensure that our parent's ChangeId's are internal if we are.
      if self.internal:
        dependencies = [FormatChangeId(x, force_internal=True)
                        for x in dependencies]

    return dependencies

  def _SetChangeId(self, change_id):
    """Set this instances change_id, and id from the given ChangeId.

    Args:
      change_id: A strict gerrit changeId to set this instance to.  The id
        attribute is computed from this, and formatting/validation is done.
    """
    self.change_id = FormatChangeId(change_id, strict=True)
    self.id = FormatChangeId(self.change_id, force_internal=self.internal)

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
      self._SetChangeId(self._ParseChangeId(commit_message))
    except BrokenChangeID:
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
      raise BrokenChangeID(self, 'Missing Change-Id in %s' % (data,))

    # Now, validate it.  This has no real effect on actual gerrit patches,
    # but for local patches the validation is useful for general sanity
    # enforcement.
    change_id_match = change_id_match[-1]
    # Note that since we're parsing it from basically a commit message,
    # the gerrit standard format is required- no internal markings.
    if not self._STRICT_VALID_CHANGE_ID_RE.match(change_id_match):
      raise BrokenChangeID(self, change_id_match)

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
            raise BrokenCQDepends(self, chunk)
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

  def __init__(self, project_url, project, ref, tracking_branch, internal,
               sha1):
    GitRepoPatch.__init__(self, project_url, project, ref, tracking_branch,
                          internal, sha1=sha1)

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
    hash_fields = [('tree_hash', '%T'), ('parent_hash', '%P')]
    transfer_fields = [('GIT_AUTHOR_NAME', '%an'),
                       ('GIT_AUTHOR_EMAIL', '%ae'),
                       ('GIT_AUTHOR_DATE', '%ad'),
                       ('GIT_COMMITTER_NAME', '%cn'),
                       ('GIT_COMMITTER_EMAIL', '%ce'),
                       ('GIT_COMMITER_DATE', '%ct')]
    fields = hash_fields + transfer_fields

    format_string = '%n'.join([code for _, code in fields] + ['%B'])
    result = cros_build_lib.RunGitCommand(
        self.project_url,
        ['log', '--format=%s' % format_string, '-z', '-n1', self.sha1])
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

    result = cros_build_lib.RunGitCommand(
        self.project_url,
        ['commit-tree', field_value['tree_hash'], '-p',
         field_value['parent_hash']],
        extra_env=extra_env, input=commit_body)

    new_sha1 = result.output.strip()
    if new_sha1 == self.sha1:
      raise PatchException(
          'Internal error!  Carbon copy of %s is the same as original!'
          % self.sha1)

    return new_sha1

  def Upload(self, push_url, remote_ref, dryrun=False):
    """Upload the patch to a remote git branch.

    Arguments:
      push_url: Which url to push to.
      remote_ref: The ref on the remote host to push to.
      dryrun: Do the git push with --dry-run
    """
    carbon_copy = self._GetCarbonCopy()
    cmd = ['push', push_url, '%s:%s' % (carbon_copy, remote_ref)]
    if dryrun:
      cmd.append('--dry-run')

    cros_build_lib.RunGitCommand(self.project_url, cmd)


class UploadedLocalPatch(GitRepoPatch):
  """Represents an uploaded local patch passed in using --remote-patch."""
  def __init__(self, project_url, project, ref, tracking_branch,
               original_branch, original_sha1, internal,
               carbon_copy_sha1=None):
    GitRepoPatch.__init__(self, project_url, project, ref, tracking_branch,
                          internal, sha1=carbon_copy_sha1)
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
        internal,
        sha1=patch_dict['currentPatchSet']['revision'],
        change_id=patch_dict['id'])

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

    except BrokenChangeID:
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



def PrepareLocalPatches(manifest, patches):
  """Finish validation of parameters, and save patches to a temp folder.

  Args:
    manifest: The manifest object for the checkout in question.
    patches:  A list of user-specified patches, in project[:branch] form.
  """
  patch_info = []
  for patch in patches:
    project, branch = patch.split(':')
    project_dir = manifest.GetProjectPath(project, True)
    tracking_branch = manifest.GetProjectsLocalRevision(project)

    cmd = ['git', 'rev-list', '-n1', '%s..%s' % (tracking_branch, branch)]
    result = cros_build_lib.RunCommand(cmd, redirect_stdout=True,
                                       cwd=project_dir)
    sha1 = result.output.strip()

    if not sha1:
      cros_build_lib.Die('No changes found in %s:%s' % (project, branch))

    patch_info.append(LocalPatch(os.path.join(project_dir, '.git'),
                                 project, branch, tracking_branch,
                                 manifest.ProjectIsInternal(project), sha1))

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
    except ValueError, e:
      raise ValueError(
          "Unexpected tryjob format.  You may be running an "
          "older version of chromite.  Run 'repo sync "
          "chromiumos/chromite'.  Error was %s" % e)

    internal = False
    if tag == constants.EXTERNAL_PATCH_TAG:
      push_url = constants.GERRIT_SSH_URL
    elif tag == constants.INTERNAL_PATCH_TAG:
      push_url = constants.GERRIT_INT_SSH_URL
      internal = True
    else:
      raise ValueError('Bad remote patch format.  Unknown tag %s' % tag)

    patch_info.append(UploadedLocalPatch(os.path.join(push_url, project),
                                         project, ref, tracking_branch,
                                         original_branch,
                                         os.path.basename(ref), internal))

  return patch_info
