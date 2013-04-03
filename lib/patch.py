# Copyright (c) 2011-2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module that handles the processing of patches to the source tree."""

import logging
import os
import random
import re

from chromite.buildbot import constants
from chromite.lib import cros_build_lib
from chromite.lib import git

_MAXIMUM_GERRIT_NUMBER_LENGTH = 6


class PatchException(Exception):
  """Base exception class all patch exception derive from."""

  # Unless instances override it, default all exceptions to ToT.
  inflight = False

  def __init__(self, patch, message=None):
    if not isinstance(patch, GitRepoPatch):
      raise TypeError(
          "Patch must be a GitRepoPatch derivative; got type %s: %r"
          % (type(patch), patch))
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

  def __init__(self, patch, message=None, inflight=False, trivial=False,
               files=()):
    PatchException.__init__(self, patch, message=message)
    self.inflight = inflight
    self.trivial = trivial
    self.files = files = tuple(files)
    # Reset args; else serialization can break.
    self.args = (patch, message, inflight, trivial, files)

  def _stringify_inflight(self):
    return 'current patch series' if self.inflight else 'ToT'

  def __str__(self):
    s = 'Failed to cherry-pick patch %s to %s' % (
        self.patch, self._stringify_inflight())
    if self.trivial:
      s += (' because file content merging is disabled for this '
            'project.')
    else:
      s += '.'
    if self.files:
      s += '  The conflicting files were: %s\n' % ', '.join(sorted(self.files))
    if self.message:
      s += '  %s' % (self.message,)
    return s


class PatchAlreadyApplied(ApplyPatchException):

  def __str__(self):
    return "Failed to cherry-pick %s to %s since it's already committed" % (
        self.patch, self._stringify_inflight())


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

  def __init__(self, patch, text, msg=None):
    PatchException.__init__(self, patch)
    self.text = text
    self.msg = msg
    self.args += (text, msg)

  def __str__(self):
    s = "Change %s has a malformed CQ-DEPEND target: %s" % (
        self.patch, self.text)
    if self.msg is not None:
      s += '; %s' % (self.msg,)
    return s


class BrokenChangeID(PatchException):
  """Raised if a patch has an invalid or missing Change-ID."""

  def __init__(self, patch, message, missing=False):
    PatchException.__init__(self, patch)
    self.message = message
    self.missing = missing
    self.args += (message, missing)

  def __str__(self):
    return "Change %s has a broken ChangeId: %s" % (self.patch, self.message)


def MakeChangeId(unusable=False):
  """Create a random Change-Id.

  Args:
    unusable: If set to True, return a Change-Id like string that gerrit
      will explicitly fail on.  This is primarily used for internal ids,
      as a fallback when a Change-Id could not be parsed.
  """
  s = "%x" % (random.randint(0, 2 ** 160),)
  s = s.rjust(40, '0')
  if unusable:
    return 'Fake-ID %s' % s
  return 'I%s' % s


class PatchCache(object):
  """Dict-like object used for tracking a group of patches.

  This is usable both for existence checks against given string
  deps, and for change querying."""

  def __init__(self, initial=()):
    self._dict = {}
    self.Inject(*initial)

  def Inject(self, *changes):
    """Inject a sequence of changes into this cache."""
    for change in changes:
      for key in change.LookupAliases():
        self.InjectCustomKey(key, change)

  def InjectCustomKey(self, key, change):
    """Inject a change w/ a specific key.  Generally you want Inject instead."""
    self._dict[str(key)] = change

  def _GetAliases(self, value):
    if hasattr(value, 'LookupAliases'):
      return value.LookupAliases()
    elif not isinstance(value, basestring):
      # This isn't needed in production code; it however is
      # rather useful to flush out bugs in test code.
      raise ValueError("Value %r isn't a string" % (value,))
    return [value]

  def Remove(self, *changes):
    """Remove a change from this cache."""
    for change in changes:
      for alias in self._GetAliases(change):
        self._dict.pop(alias, None)

  def __iter__(self):
    return iter(set(self._dict.itervalues()))

  def __getitem__(self, key):
    """If the given key exists, return the Change, else None."""
    for alias in self._GetAliases(key):
      val = self._dict.get(alias)
      if val is not None:
        return val
    return None

  def __contains__(self, key):
    return self[key] is not None

  def copy(self):
    """Return a copy of this cache."""
    return self.__class__(list(self))


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
      True.  Note that force_internal and force_external are mutually exclusive.
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
  if not git.IsSHA1(text, False):
    raise ValueError("FormatChangeId invoked w/ a non hex ChangeId value: %s"
                     % original_text)

  return '%sI%s' % (prefix, text)


def FormatSha1(text, force_internal=False, force_external=False, strict=False):
  """Format a Sha1 used for dependencies into a standardized form.

  Use this anywhere we're accepting user input of Sha1s for dependencies.

  Args:
    text: The given sha1 to inspect and reformat.
    force_internal: If True, force the resultant sha1 to be an internally
      formatted one.
    force_external: If True, force the resultant sha1 to be an externally
      formatted one; this form is the gerrit standard, thus if you need to
      convert a sha1 for passing directly to git/gerrit, this should be set to
      True.  Note that force_internal and force_external are mutually exclusive.
    strict: If True, then it's an error if the text holds an internally
      formatted sha1.  Generally this is only useful if you're working w/
      a direct git commit message and are trying to ensure the message is
      gerrit compatible.
  """
  if force_internal and force_external:
    raise TypeError("FormatSha1: either force_internal or force_external "
                    "can be set to True, but not both.")

  if not text:
    raise ValueError("FormatSha1 invoked w/ an empty value: %r" % (text,))


  original_text = text
  prefix = '*' if force_internal else ''
  if text[0].startswith('*'):
    if strict:
      raise ValueError("FormatSha1 invoked w/ an internally formatted "
                       "sha1 while in strict mode: %r" % (original_text,))
    if not force_external:
      prefix = '*'
    text = text[1:]

  if not git.IsSHA1(text):
    raise ValueError("FormatSha1 invoked w/ a malformed value: %r "
                     % (original_text,))

  return '%s%s' % (prefix, text.lower())


def FormatGerritNumber(text, force_internal=False, force_external=False,
                       strict=False):
  """Format a Gerrit Number used for dependencies into a standardized form.

  Use this anywhere we're accepting user input of Gerrit Numbers for
  dependencies.

  Args:
    text: The given sha1 to inspect and reformat.
    force_internal: If True, force the resultant number to be an internally
      formatted one.
    force_external: If True, force the resultant number to be an externally
      formatted one; this form is the gerrit standard, thus if you need to
      convert a value for passing directly to gerrit, this should be set to
      True.  Note that force_internal and force_external are mutually exclusive.
    strict: If True, then it's an error if the text holds an internally
      formatted number.  Generally this is only useful if you're working w/
      a direct git commit message and are trying to ensure the message is
      gerrit compatible.
  """
  if force_internal and force_external:
    raise TypeError("FormatGerritNumber: either force_internal or "
                    "force_external can be set to True, but not both.")

  if not text:
    raise ValueError("FormatGerritNumber invoked w/ an empty value: %r"
                     % (text,))

  original_text = text
  prefix = '*' if force_internal else ''
  if text[0].startswith('*'):
    if strict:
      raise ValueError("FormatGerritNumber invoked w/ an internally formatted "
                       "value while in strict mode: %r" % (original_text,))
    if not force_external:
      prefix = '*'
    text = text[1:]

  if not text.isdigit():
    raise ValueError("FormatSha1 invoked w/ a value that isn't a number: %r" %
                     (original_text,))
  # Force an int conversion to strip any leading zeroes.
  text = str(int(text))
  if len(text) > _MAXIMUM_GERRIT_NUMBER_LENGTH:
    raise ValueError("FormatSha1 invoked w/ a value longer than %i digits: %r" %
                     (_MAXIMUM_GERRIT_NUMBER_LENGTH, original_text,))

  return '%s%s' % (prefix, text)


def FormatPatchDep(text, force_internal=False, force_external=False,
                   strict=False, sha1=True, changeId=True,
                   gerrit_number=True, allow_CL=False):
  """Given a patch dependency, ensure it's formatted correctly.

  This should be used when the consumer doesn't care what type of dep
  it is, just as long as it's formatted correctly (ValidationPool for
  example).

  Args:
    force_internal, force_external, strict: See FormatChangeId for
      details.
    sha1: If False, throw ValueError if the dep is a sha1.
    changeId: If False, throw ValueError if the dep is a ChangeId.
    gerrit_number: If False, throw ValueError if the dep is a gerrit number.
    allow_CL: If True, allow CL: prefix; else view it as an error.
      That format is primarily used for -g, and in CQ-DEPEND.
  """
  if not text:
    raise ValueError("FormatPatchDep invoked with an empty value: %r"
                     % (text,))
  original_text = target_text = text

  # Deal w/ CL: targets.
  if text.upper().startswith("CL:"):
    if not allow_CL:
      raise ValueError(
          "FormatPatchDep: 'CL:' is disallowed in this context: %r"
          % (original_text,))
    elif not text.startswith("CL:"):
      raise ValueError(
          "FormatPatchDep: 'CL:' must be upper case: %r"
          % (original_text,))
    target_text = text = text[3:]

  text = text.lstrip('*')
  if text[0:1] in 'Ii':
    if not changeId:
      raise ValueError(
          "FormatPatchDep: ChangeId isn't allowed in this context: %r"
          % (original_text,))
    target = FormatChangeId
  elif text.isdigit() and len(text) <= _MAXIMUM_GERRIT_NUMBER_LENGTH:
    if not gerrit_number:
      raise ValueError(
          "FormatPatchDep: ChangeId isn't allowed in this context: %r"
          % (original_text,))
    target = FormatGerritNumber
  elif not sha1:
    raise ValueError(
        "FormatPatchDep: sha1 isn't allowed in this context: %r"
        % (original_text,))
  else:
    target = FormatSha1
  return target(target_text, force_internal=force_internal,
                force_external=force_external, strict=strict)


def GetPaladinDeps(commit_message):
  """Get the paladin dependencies for the given |commit_message|."""
  PALADIN_DEPENDENCY_RE = re.compile(r'^(CQ\W?DEPEND.)(.*)$',
                                     re.MULTILINE | re.IGNORECASE)
  PATCH_RE = re.compile('[^, ]+')
  EXPECTED_PREFIX = 'CQ-DEPEND='
  matches = PALADIN_DEPENDENCY_RE.findall(commit_message)
  dependencies = []
  for prefix, match in matches:
    if prefix != EXPECTED_PREFIX:
      msg = 'Expected %r, but got %r' % (EXPECTED_PREFIX, prefix)
      raise ValueError(msg)
    for chunk in PATCH_RE.findall(match):
      chunk = FormatPatchDep(chunk, sha1=False, allow_CL=True)
      if chunk not in dependencies:
        dependencies.append(chunk)
  return dependencies


class GitRepoPatch(object):
  """Representing a patch from a branch of a local or remote git repository."""

  # Note the selective case insensitivity; gerrit allows only this.
  # TOOD(ferringb): back VALID_CHANGE_ID_RE down to {8,40}, requires
  # ensuring CQ's internals can do the translation (almost can now,
  # but will fail in the case of a CQ-DEPEND on a change w/in the
  # same pool).
  _STRICT_VALID_CHANGE_ID_RE = re.compile(r'^I[0-9a-fA-F]{40}$')
  _GIT_CHANGE_ID_RE = re.compile(r'^Change-Id:[\t ]*(\w+)\s*$',
                                 re.I | re.MULTILINE)

  def __init__(self, project_url, project, ref, tracking_branch, remote,
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
    self.commit_message = None
    self._subject_line = None
    self.project_url = project_url
    self.project = project
    self.ref = ref
    self.tracking_branch = os.path.basename(tracking_branch)
    self.remote = remote
    if sha1 is not None:
      sha1 = FormatSha1(sha1, strict=True)
    self.sha1 = sha1
    # change_id must always be a valid Change-Id (strict gerrit), or None,
    # and be in strict gerrit form (aka, external).
    # id can be in our form; internal or external.
    self.change_id = self.id = None
    self._is_fetched = set()

    # Do the ChangeId setting now that we have a fully setup instance.
    if change_id:
      self._SetChangeId(change_id)

  @property
  def internal(self):
    """Whether patch is to an internal cros project."""
    return self.remote == constants.INTERNAL_REMOTE

  def LookupAliases(self):
    """Return the list of lookup keys this change is known by."""
    l = [self.change_id, self.sha1]
    return [FormatPatchDep(x, gerrit_number=False,
                           force_internal=self.internal)
            for x in l if x is not None]

  def Fetch(self, git_repo):
    """Fetch this patch into the given git repository.

    FETCH_HEAD is implicitly reset by this operation.  Additionally,
    if the sha1 of the patch was not yet known, it is pulled and stored
    on this object and the git_repo is updated w/ the requested git
    object.

    While doing so, we'll load the commit message and Change-Id if not
    already known.

    Finally, if the sha1 is known and it's already available in the target
    repository, this will skip the actual fetch operation (it's unneeded).

    Args:
      git_repo: The git repository to fetch this patch into.
    Returns:
      The sha1 of the patch.
    """

    git_repo = os.path.normpath(git_repo)
    if git_repo in self._is_fetched:
      return self.sha1

    def _PullData(rev):
      ret = git.RunGit(
          git_repo, ['log', '--pretty=format:%H%x00%s%x00%B', '-n1', rev],
          error_code_ok=True)
      if ret.returncode != 0:
        return None, None, None
      output = ret.output.split('\0')
      if len(output) != 3:
        return None, None, None
      return [x.strip() for x in output]

    if self.sha1 is not None:
      # See if we've already got the object.
      sha1, subject, msg = _PullData(self.sha1)
      if sha1 is not None:
        sha1 = FormatSha1(sha1, strict=True)
        assert sha1 == self.sha1
        self._EnsureId(msg)
        self.commit_message = msg
        self._subject_line = subject
        self._is_fetched.add(git_repo)
        return self.sha1

    git.RunGit(git_repo, ['fetch', self.project_url, self.ref])

    sha1, subject, msg = _PullData('FETCH_HEAD')
    sha1 = FormatSha1(sha1, strict=True)

    # Even if we know the sha1, still do a sanity check to ensure we
    # actually just fetched it.
    if self.sha1 is not None:
      if sha1 != self.sha1:
        raise PatchException(self,
                             'Patch %s specifies sha1 %s, yet in fetching from '
                             '%s we could not find that sha1.  Internal error '
                             'most likely.' % (self, self.sha1, self.ref))
    else:
      self.sha1 = sha1
    self._EnsureId(msg)
    self.commit_message = msg
    self._subject_line = subject
    self._is_fetched.add(git_repo)
    return self.sha1

  def GetDiffStatus(self, git_repo):
    """Isolate the paths and modifications this patch induces.

    Note that detection of file renaming is explicitly turned off.
    This is intentional since the level of rename detection can vary
    by user configuration, and trying to have our code specify the
    minimum level is fairly messy from an API perspective.

    Args:
      git_repo: Git repository to operate upon.

    returns: A dictionary of path -> modification_type tuples.  See
      `git log --help`, specifically the --diff-filter section for details."""

    self.Fetch(git_repo)

    try:
      lines = git.RunGit(git_repo, ['diff', '--no-renames', '--name-status',
                                    '%s^..%s' % (self.sha1, self.sha1)])
    except cros_build_lib.RunCommandError, e:
      # If we get a 128, that means git couldn't find the the parent of our
      # sha1- meaning we're the first commit in the repository (there is no
      # parent).
      if e.result.returncode != 128:
        raise
      return {}
    lines = lines.output.splitlines()
    return dict(line.split('\t', 1)[::-1] for line in lines)

  def CherryPick(self, git_repo, trivial=False, inflight=False,
                 leave_dirty=False):
    """Attempts to cherry-pick the given rev into branch.

    Arguments:
      git_repo: The git repository to operate upon.
      trivial: Only allow trivial merges when applying change.
      inflight: If true, changes are already applied in this branch.
      leave_dirty: If True, if a CherryPick fails leaves partial commit behind.

    Raises:
      A ApplyPatchException if the request couldn't be handled.
    """
    # Note the --ff; we do *not* want the sha1 to change unless it
    # has to.
    cmd = ['cherry-pick', '--ff']
    if trivial:
      cmd += ['--strategy', 'resolve', '-X', 'trivial']
    cmd.append(self.sha1)

    reset_target = None if leave_dirty else 'HEAD'
    try:
      git.RunGit(git_repo, cmd)
      reset_target = None
      return
    except cros_build_lib.RunCommandError, error:
      ret = error.result.returncode
      if ret not in (1, 2):
        cros_build_lib.Error(
            "Unknown cherry-pick exit code %s; %s",
            ret, error)
        raise ApplyPatchException(
            self, inflight=inflight,
            message=("Unknown exit code %s returned from cherry-pick "
                     "command: %s" % (ret, error)))
      elif ret == 1:
        # This means merge resolution was fine, but there was content conflicts.
        # If there are no conflicts, then this is caused by the change already
        # being merged.
        result = git.RunGit(git_repo, ['status', '--porcelain'])

        # Porcelain format is line per file, first two chars are the status
        # code, then a space, then the filename.
        # ?? means "untracked"; everything else is git tracked conflicts.
        conflicts = [x[3:] for x in result.output.splitlines()
                     if x and not x[:2] == '??']
        if not conflicts:
          # No conflicts means the git repo is in a pristine state.
          reset_target = None
          raise PatchAlreadyApplied(self, inflight=inflight)

        # Making it here means that it wasn't trivial, nor was it already
        # applied.
        assert not trivial
        raise ApplyPatchException(self, inflight=inflight, files=conflicts)

      # ret=2 handling, this deals w/ trivial conflicts; including figuring
      # out if it was trivial induced or not.
      assert trivial
      # Here's the kicker; trivial conflicts can mask content conflicts.
      # We would rather state if it's a content conflict since in solving the
      # content conflict, the trivial conflict is solved.  Thus this
      # second run, where we let the exception fly through if one occurs.
      # Note that a trivial conflict means the tree is unmodified; thus
      # no need for cleanup prior to this invocation.
      reset_target = None
      self.CherryPick(git_repo, trivial=False, inflight=inflight)
      # Since it succeeded, we need to rewind.
      reset_target = 'HEAD^'

      raise ApplyPatchException(self, trivial=True, inflight=inflight)
    finally:
      if reset_target is not None:
        git.RunGit(git_repo, ['reset', '--hard', reset_target],
                   error_code_ok=True)

  def Apply(self, git_repo, upstream, trivial=False):
    """Apply patch into a standalone git repo.

    The git repo does not need to be part of a repo checkout.

    Arguments:
      git_repo: The git repository to operate upon.
      upstream: The branch to base the patch on.
      trivial: Only allow trivial merges when applying change.
    """

    self.Fetch(git_repo)

    logging.info('Attempting to cherry-pick change %s', self)

    if not git.DoesLocalBranchExist(git_repo, constants.PATCH_BRANCH):
      cmd = ['checkout', '-b', constants.PATCH_BRANCH, '-t', upstream]
      git.RunGit(git_repo, cmd)
      inflight = False
    else:
      # Figure out if we're inflight.
      upstream, head = [
          git.RunGit(git_repo, ['rev-list', '-n1', x]).output.strip()
          for x in (upstream, 'HEAD')]
      inflight = (head != upstream)

    # Run appropriate sanity checks.
    self._SanityChecks(git_repo, upstream, inflight=inflight)

    do_checkout = True
    try:
      self.CherryPick(git_repo, trivial=trivial, inflight=inflight)
      do_checkout = False
      return
    except ApplyPatchException:
      if not inflight:
        raise
      git.RunGit(git_repo, ['checkout', '-f', '--detach', upstream])

      self.CherryPick(git_repo, trivial=trivial, inflight=False)
      # Making it here means that it was an inflight issue; throw the original.
      raise
    finally:
      # Ensure we're on the correct branch on the way out.
      if do_checkout:
        git.RunGit(git_repo, ['checkout', '-f', constants.PATCH_BRANCH],
                   error_code_ok=True)

  def ApplyAgainstManifest(self, manifest, trivial=False):
    """Applies the patch against the specified manifest.

      manifest: A ManifestCheckout object which is used to discern which
        git repo to patch, what the upstream branch should be, etc.
      trivial:  Only allow trivial merges when applying change.

    Raises:
      ApplyPatchException: If the patch failed to apply.
    """
    manifest_branch = manifest.GetProjectsLocalRevision(self.project)
    self.Apply(manifest.GetProjectPath(self.project, True),
               manifest_branch, trivial=trivial)

  def GerritDependencies(self, git_repo, upstream):
    """Returns an ordered list of dependencies from Gerrit.

    The list of changes are in order from FETCH_HEAD back to m/master.

    Arguments:
      git_repo: The git repository to fetch/access this commit from.
    Returns:
      An ordered list of Gerrit ChangeIds that this patch depends on.
      Note that if the commit lacks a ChangeId, the parent's sha1 is returned.
    """
    dependencies = []
    logging.debug('Checking for Gerrit dependencies for change %s', self)

    rev = self.Fetch(git_repo)

    try:
      return_obj = git.RunGit(
          git_repo, ['log', '-z', '-n1', '--pretty=format:%H%n%B',
                     '%s..%s^' % (upstream, rev)])
    except cros_build_lib.RunCommandError, e:
      if e.result.returncode != 128:
        raise
      # Errorcode 128 means "object not found"; either we've got an
      # internal bug (tracking_branch was wrong, fetch somehow didn't get
      # that actual rev, etc), or... this is the first commit in a repository.
      # The following code checks for that, raising the original
      # exception if not.
      result = git.RunGit(git_repo, ['rev-list', '-n2', rev])
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
      patches = return_obj.output.split('\0')

    for patch_output in patches:
      sha1, commit_msg = patch_output.split('\n', 1)
      try:
        dep = FormatChangeId(self._ParseChangeId(commit_msg),
                             force_internal=self.internal, strict=True)
      except BrokenChangeID, e:
        if not e.missing:
          raise
        cros_build_lib.Warning(
            "Parent %s lacks a ChangeId; using the parent's sha1 as the "
            "dependency.", sha1)
        dep = FormatSha1(sha1, force_internal=self.internal, strict=True)
      dependencies.append(dep)

    if dependencies:
      logging.debug('Found %s Gerrit dependencies for change %s', dependencies,
                   self)
      # Ensure that our parent's ChangeId's are internal if we are.

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
          'message.  CQ-DEPEND against this rev may not work, nor '
          'will any gerrit querying.  Please add the appropriate '
          'Change-Id into the commit message to resolve this.',
          self, self.sha1)

      self.id = self.sha1

    return self.id

  def _ParseChangeId(self, data):
    """Parse a Change-Id out of a block of text.

    Note that the returned content is *not* ran through FormatChangeId;
    this is left up to the invoker.
    """
    # Grab just the last pararaph.
    git_metadata = re.split(r'\n{2,}', data.rstrip())[-1]
    change_id_match = self._GIT_CHANGE_ID_RE.findall(git_metadata)
    if not change_id_match:
      raise BrokenChangeID(self, 'Missing Change-Id in %s' % (data,),
                           missing=True)

    # Now, validate it.  This has no real effect on actual gerrit patches,
    # but for local patches the validation is useful for general sanity
    # enforcement.
    change_id_match = change_id_match[-1]
    # Note that since we're parsing it from basically a commit message,
    # the gerrit standard format is required- no internal markings.
    if not self._STRICT_VALID_CHANGE_ID_RE.match(change_id_match):
      raise BrokenChangeID(self, change_id_match)

    return change_id_match

  def PaladinDependencies(self, git_repo):
    """Returns an ordered list of dependencies based on the Commit Message.

    Parses the Commit message for this change looking for lines that follow
    the format:

    CQ-DEPEND=change_num+ e.g.

    A commit which depends on a couple others.

    BUG=blah
    TEST=blah
    CQ-DEPEND=10001,10002
    """
    dependencies = []
    logging.debug('Checking for CQ-DEPEND dependencies for change %s', self)

    self.Fetch(git_repo)

    try:
      dependencies = GetPaladinDeps(self.commit_message)
    except ValueError as e:
      raise BrokenCQDepends(self, str(e))

    if dependencies:
      logging.debug('Found %s Paladin dependencies for change %s', dependencies,
                   self)
    return dependencies

  def _SanityChecks(self, git_repo, upstream, inflight=False):
    ebuilds = [path for (path, mtype) in
               self.GetDiffStatus(git_repo).iteritems()
               if mtype == 'A' and path.endswith('.ebuild')]

    conflicts = self._FindTrivialConflicts(git_repo, 'HEAD', ebuilds)
    if not conflicts:
      return

    if inflight:
      # If we're inflight, test against ToT for an accurate error message.
      tot_conflicts = self._FindTrivialConflicts(git_repo, upstream, ebuilds)
      if tot_conflicts:
        inflight = False
        conflicts = tot_conflicts

    raise ApplyPatchException(
        self, inflight=inflight,
        message="Ebuild rename conflicts detected.",
        files=ebuilds)

  def _FindTrivialConflicts(self, git_repo, tree_revision, targets):
    if not targets:
      return []

    # Note the output of this ls-tree invocation is filename per line;
    # basically equivalent to ls -1.
    conflicts = git.RunGit(
        git_repo, ['ls-tree', '--full-name', '--name-only', '-z', tree_revision,
                   '--'] + targets, error_code_ok=True).output.split('\0')[:-1]

    return conflicts

  def __str__(self):
    """Returns custom string to identify this patch."""
    s = '%s:%s' % (self.project, self.ref)
    if self.sha1 is not None:
      s = '%s:%s%s' % (s, '*' if self.internal else '', self.sha1[:8])
    # TODO(ferringb,build): This gets a bit long in output; should likely
    # do some form of truncation to it.
    if self._subject_line:
      s += ' "%s"' % (self._subject_line,)
    return s


class LocalPatch(GitRepoPatch):
  """Represents patch coming from an on-disk git repo."""

  def __init__(self, project_url, project, ref, tracking_branch, remote,
               sha1):
    GitRepoPatch.__init__(self, project_url, project, ref, tracking_branch,
                          remote, sha1=sha1)
    # Initialize our commit message/ChangeId now, since we know we have
    # access to the data right now.
    self.Fetch(project_url)

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
    result = git.RunGit(self.project_url,
        ['log', '--format=%s' % format_string, '-n1', self.sha1])
    lines = result.output.splitlines()
    field_value = dict(zip([name for name, _ in fields],
                           [line.strip() for line in lines]))
    commit_body = '\n'.join(lines[len(fields):])

    if len(field_value['parent_hash'].split()) != 1:
      raise PatchException(self,
                           'Branch %s:%s contains merge result %s!'
                           % (self.project, self.ref, self.sha1))

    extra_env = dict([(field, field_value[field]) for field, _ in
                      transfer_fields])

    # Reset the commit date to a value that can't conflict; if we
    # leave this to git, it's possible for a fast moving set of commit/uploads
    # to all occur within the same second (thus the same commit date),
    # resulting in the same sha1.
    extra_env['GIT_COMMITTER_DATE'] = str(
        int(extra_env["GIT_COMMITER_DATE"]) - 1)

    result = git.RunGit(
        self.project_url,
        ['commit-tree', field_value['tree_hash'], '-p',
         field_value['parent_hash']],
        extra_env=extra_env, input=commit_body)

    new_sha1 = result.output.strip()
    if new_sha1 == self.sha1:
      raise PatchException(
          self,
          'Internal error!  Carbon copy of %s is the same as original!'
          % self.sha1)

    return new_sha1

  def Upload(self, push_url, remote_ref, carbon_copy=True, dryrun=False,
             reviewers=(), cc=()):
    """Upload the patch to a remote git branch.

    Arguments:
      push_url: Which url to push to.
      remote_ref: The ref on the remote host to push to.
      carbon_copy: Use a carbon_copy of the local commit.
      dryrun: Do the git push with --dry-run
      reviewers: Iterable of reviewers to add.
      cc: Iterable of people to add to cc.
    Returns:
      A list of gerrit URLs found in the output
    """
    if carbon_copy:
      ref_to_upload = self._GetCarbonCopy()
    else:
      ref_to_upload = self.sha1

    cmd = ['push']
    if reviewers or cc:
      pack = '--receive-pack=git receive-pack '
      if reviewers:
        pack += ' '.join(['--reviewer=' + x for x in reviewers])
      if cc:
        pack += ' '.join(['--cc=' + x for x in cc])
      cmd.append(pack)
    cmd += [push_url, '%s:%s' % (ref_to_upload, remote_ref)]
    if dryrun:
      cmd.append('--dry-run')

    lines = git.RunGit(self.project_url, cmd).error.splitlines()
    urls = []
    for num, line in enumerate(lines):
      # Look for output like:
      # remote: New Changes:
      # remote:   https://gerrit.chromium.org/gerrit/36756
      if 'New Changes:' in line:
        urls = []
        for line in lines[num + 1:]:
          line = line.split()
          if len(line) != 2 or not line[1].startswith('http'):
            break
          urls.append(line[-1])
        break
    return urls


class UploadedLocalPatch(GitRepoPatch):
  """Represents an uploaded local patch passed in using --remote-patch."""
  def __init__(self, project_url, project, ref, tracking_branch,
               original_branch, original_sha1, remote, carbon_copy_sha1=None):
    GitRepoPatch.__init__(self, project_url, project, ref, tracking_branch,
                          remote, sha1=carbon_copy_sha1)
    self.original_branch = original_branch

    self._original_sha1_valid = True
    try:
      original_sha1 = FormatSha1(original_sha1, strict=True)
    except ValueError:
      self._original_sha1_valid = False

    self.original_sha1 = original_sha1

  def LookupAliases(self):
    """Return the list of lookup keys this change is known by."""
    l = GitRepoPatch.LookupAliases(self)
    if self._original_sha1_valid:
      l.append(FormatSha1(self.original_sha1, force_internal=self.internal))
    return l

  def __str__(self):
    """Returns custom string to identify this patch."""
    s = '%s:%s:%s' % (self.project, self.original_branch,
                      self.original_sha1[:8])
    # TODO(ferringb,build): This gets a bit long in output; should likely
    # do some form of truncation to it.
    if self._subject_line:
      s += ':"%s"' % (self._subject_line,)
    return s


class GerritPatch(GitRepoPatch):
  """Object that represents a Gerrit CL."""

  def __init__(self, patch_dict, remote, url_prefix):
    """Construct a GerritPatch object from Gerrit query results.

    Gerrit query JSON fields are documented at:
    http://gerrit-documentation.googlecode.com/svn/Documentation/2.2.1/json.html

    Args:
      patch_dict: A dictionary containing the parsed JSON gerrit query results.
      remote: The manifest remote the patched project uses.
      url_prefix: The project name will be appended to this to get the full
                  repository URL.
    """
    self.patch_dict = patch_dict
    self.url_prefix = url_prefix
    super(GerritPatch, self).__init__(
        os.path.join(url_prefix, patch_dict['project']),
        patch_dict['project'],
        patch_dict['currentPatchSet']['ref'],
        patch_dict['branch'],
        remote,
        sha1=patch_dict['currentPatchSet']['revision'],
        change_id=patch_dict['id'])

    # id - The CL's ChangeId
    # revision - The CL's SHA1 hash.
    self.revision = patch_dict['currentPatchSet']['revision']
    self.patch_number = patch_dict['currentPatchSet']['number']
    self.commit = patch_dict['currentPatchSet']['revision']
    self.owner, _, _ = patch_dict['owner']['email'].partition('@')
    self.gerrit_number = FormatGerritNumber(str(patch_dict['number']),
                                            strict=True)
    self.url = patch_dict['url']
    # status - Current state of this change.  Can be one of
    # ['NEW', 'SUBMITTED', 'MERGED', 'ABANDONED'].
    self.status = patch_dict['status']
    approvals = self.patch_dict['currentPatchSet'].get('approvals', [])
    self.approval_timestamp = \
        max(x['grantedOn'] for x in approvals) if approvals else 0

  def __reduce__(self):
    """Used for pickling to re-create patch object."""
    return self.__class__, (self.patch_dict.copy(), self.remote,
                            self.url_prefix)

  def LookupAliases(self):
    """Return the list of lookup keys this change is known by."""
    l = GitRepoPatch.LookupAliases(self)
    l.append(FormatGerritNumber(self.gerrit_number,
                                force_internal=self.internal))
    return l

  def IsAlreadyMerged(self):
    """Returns whether the patch has already been merged in Gerrit."""
    return self.status == 'MERGED'

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
      parsed_id = FormatChangeId(self._ParseChangeId(commit_message),
                                 strict=True)
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
          'message.  This can break the ability for any children to depend on '
          'this Change as a parent.  Please add the appropriate '
          'Change-Id into the commit message to resolve this.',
          self, self.change_id, self.sha1)

    return self.id

  def __str__(self):
    """Returns custom string to identify this patch."""
    s = '%s:%s%s' % (self.owner, '*' if self.internal else '',
                     self.gerrit_number)
    if self.sha1 is not None:
      s = '%s:%s%s' % (s, '*' if self.internal else '', self.sha1[:8])
    if self._subject_line:
      s += ':"%s"' % (self._subject_line,)
    return s

  # Define methods to use patches in sets.  We uniquely identify patches
  # by Gerrit change numbers.
  def __hash__(self):
    return hash(self.id)

  def __eq__(self, other):
    return self.id == other.id


def GeneratePatchesFromRepo(git_repo, project, tracking_branch, branch,
                            remote, allow_empty=False, starting_ref=None):
  if starting_ref is None:
    starting_ref = branch

  result = git.RunGit(
      git_repo, ['rev-list', '%s..%s' % (tracking_branch, starting_ref)])

  sha1s = result.output.splitlines()
  if not sha1s:
    if not allow_empty:
      cros_build_lib.Die('No changes found in %s:%s' % (project, branch))
    return

  for sha1 in sha1s:
    yield LocalPatch(os.path.join(git_repo, '.git'),
                     project, branch, tracking_branch,
                     remote, sha1)


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
    remote = manifest.GetAttributeForProject(project, 'remote')

    patch_info.extend(GeneratePatchesFromRepo(
        project_dir, project, tracking_branch, branch, remote))

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

    if tag not in constants.PATCH_TAGS:
      raise ValueError('Bad remote patch format.  Unknown tag %s' % tag)

    remote = constants.EXTERNAL_REMOTE
    if tag == constants.INTERNAL_PATCH_TAG:
      remote = constants.INTERNAL_REMOTE

    push_url = constants.CROS_REMOTES[remote]
    patch_info.append(UploadedLocalPatch(os.path.join(push_url, project),
                                         project, ref, tracking_branch,
                                         original_branch,
                                         os.path.basename(ref), remote))

  return patch_info
