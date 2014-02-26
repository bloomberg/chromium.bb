# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common functions for interacting with git and repo."""

import errno
import hashlib
import logging
import os
import re
# pylint: disable=W0402
import string
import sys
import time
from xml import sax

# TODO(build): Fix this.
# This should be absolute import, but that requires fixing all
# relative imports first.
_path = os.path.realpath(__file__)
_path = os.path.normpath(os.path.join(os.path.dirname(_path), '..', '..'))
sys.path.insert(0, _path)
from chromite.buildbot import constants
from chromite.lib import cros_build_lib
from chromite.lib import osutils
from chromite.lib import retry_util
# Now restore it so that relative scripts don't get cranky.
sys.path.pop(0)
del _path

# Retry a git operation if git returns a error response with any of these
# messages. It's all observed 'bad' GoB responses so far.
GIT_TRANSIENT_ERRORS = (
    # crbug.com/285832
    r'! \[remote rejected\].* -> .* \(error in hook\)',

    # crbug.com/289932
    r'! \[remote rejected\].* -> .* \(failed to lock\)',

    # crbug.com/307156
    r'! \[remote rejected\].* -> .* \(error in Gerrit backend\)',

    # crbug.com/285832
    r'remote error: Internal Server Error',

    # crbug.com/294449
    r'fatal: Couldn\'t find remote ref ',

    # crbug.com/220543
    r'git fetch_pack: expected ACK/NAK, got',

    # crbug.com/189455
    r'protocol error: bad pack header',

    # crbug.com/202807
    r'The remote end hung up unexpectedly',

    # crbug.com/298189
    r'error: gnutls_handshake\(\) failed: A TLS packet with unexpected length '
    r'was received. while accessing',

    # crbug.com/187444
    r'RPC failed; result=\d+, HTTP code = \d+',

    # crbug.com/315421
    r'The requested URL returned error: 500 while accessing',
)

GIT_TRANSIENT_ERRORS_RE = re.compile('|'.join(GIT_TRANSIENT_ERRORS))

DEFAULT_RETRY_INTERVAL = 3
DEFAULT_RETRIES = 5


class RemoteRef(object):
  """Object representing a remote ref.

  A remote ref encapsulates both a remote (e.g., 'origin',
  'https://chromium.googlesource.com/chromiumos/chromite.git', etc.) and a ref
  name (e.g., 'refs/heads/master').
  """
  def __init__(self, remote, ref):
    self.remote = remote
    self.ref = ref


def FindRepoDir(path):
  """Returns the nearest higher-level repo dir from the specified path.

  Args:
    path: The path to use. Defaults to cwd.
  """
  return osutils.FindInPathParents(
      '.repo', path, test_func=os.path.isdir)


def FindRepoCheckoutRoot(path):
  """Get the root of your repo managed checkout."""
  repo_dir = FindRepoDir(path)
  if repo_dir:
    return os.path.dirname(repo_dir)
  else:
    return None


def IsSubmoduleCheckoutRoot(path, remote, url):
  """Tests to see if a directory is the root of a git submodule checkout.

  Args:
    path: The directory to test.
    remote: The remote to compare the |url| with.
    url: The exact URL the |remote| needs to be pointed at.
  """
  if os.path.isdir(path):
    remote_url = cros_build_lib.RunCommand(
        ['git', '--git-dir', path, 'config', 'remote.%s.url' % remote],
        redirect_stdout=True, debug_level=logging.DEBUG,
        error_code_ok=True).output.strip()
    if remote_url == url:
      return True
  return False


def ReinterpretPathForChroot(path):
  """Returns reinterpreted path from outside the chroot for use inside.

  Args:
    path: The path to reinterpret.  Must be in src tree.
  """
  root_path = os.path.join(FindRepoDir(path), '..')

  path_abs_path = os.path.abspath(path)
  root_abs_path = os.path.abspath(root_path)

  # Strip the repository root from the path and strip first /.
  relative_path = path_abs_path.replace(root_abs_path, '')[1:]

  if relative_path == path_abs_path:
    raise Exception('Error: path is outside your src tree, cannot reinterpret.')

  new_path = os.path.join('/home', os.getenv('USER'), 'trunk', relative_path)
  return new_path


def IsGitRepo(cwd):
  """Checks if there's a git repo rooted at a directory."""
  return os.path.isdir(os.path.join(cwd, '.git'))


def IsGitRepositoryCorrupted(cwd):
  """Verify that the specified git repository is not corrupted.

  Args:
    cwd: The git repository to verify.

  Returns:
    True if the repository is corrupted.
  """
  cmd = ['fsck', '--no-progress', '--no-dangling']
  try:
    RunGit(cwd, cmd)
    return False
  except cros_build_lib.RunCommandError as ex:
    logging.warn(str(ex))
    return True


_HEX_CHARS = frozenset(string.hexdigits)
def IsSHA1(value, full=True):
  """Returns True if the given value looks like a sha1.

  If full is True, then it must be full length- 40 chars.  If False, >=6, and
  <40.
  """
  if not all(x in _HEX_CHARS for x in value):
    return False
  l = len(value)
  if full:
    return l == 40
  return l >= 6 and l <= 40


def IsRefsTags(value):
  """Return True if the given value looks like a tag.

  Currently this is identified via refs/tags/ prefixing.
  """
  return value.startswith("refs/tags/")


def GetGitRepoRevision(cwd, branch='HEAD'):
  """Find the revision of a branch.

  Defaults to current branch.
  """
  return RunGit(cwd, ['rev-parse', branch]).output.strip()


def DoesCommitExistInRepo(cwd, commit_hash):
  """Determine if commit object exists in a repo.

  Args:
    cwd: A directory within the project repo.
    commit_hash: The hash of the commit object to look for.
  """
  return 0 == RunGit(cwd, ['rev-list', '-n1', commit_hash],
                     error_code_ok=True).returncode


def DoesLocalBranchExist(repo_dir, branch):
  """Returns True if the local branch exists.

  Args:
    repo_dir: Directory of the git repository to check.
    branch: The name of the branch to test for.
  """
  return os.path.isfile(
      os.path.join(repo_dir, '.git/refs/heads',
                   branch.lstrip('/')))


def GetCurrentBranch(cwd):
  """Returns current branch of a repo, and None if repo is on detached HEAD."""
  try:
    ret = RunGit(cwd, ['symbolic-ref', '-q', 'HEAD'])
    return StripRefsHeads(ret.output.strip(), False)
  except cros_build_lib.RunCommandError as e:
    if e.result.returncode != 1:
      raise
    return None


def StripRefsHeads(ref, strict=True):
  """Remove leading 'refs/heads/' from a ref name.

  If strict is True, an Exception is thrown if the ref doesn't start with
  refs/heads.  If strict is False, the original ref is returned.
  """
  if not ref.startswith('refs/heads/') and strict:
    raise Exception('Ref name %s does not start with refs/heads/' % ref)

  return ref.replace('refs/heads/', '')


def StripRefs(ref):
  """Remove leading 'refs/heads', 'refs/remotes/[^/]+/' from a ref name."""
  ref = StripRefsHeads(ref, False)
  if ref.startswith("refs/remotes/"):
    return ref.split("/", 3)[-1]
  return ref


def NormalizeRef(ref):
  """Convert git branch refs into fully qualified form."""
  if ref and not ref.startswith('refs/'):
    ref = 'refs/heads/%s' % ref
  return ref


def NormalizeRemoteRef(remote, ref):
  """Convert git branch refs into fully qualified remote form."""
  if ref:
    # Support changing local ref to remote ref, or changing the remote
    # for a remote ref.
    ref = StripRefs(ref)

    if not ref.startswith('refs/'):
      ref = 'refs/remotes/%s/%s' % (remote, ref)

  return ref


class ProjectCheckout(dict):
  """Attributes of a given project in the manifest checkout.

  TODO(davidjames): Convert this into an ordinary object instead of a dict.
  """

  def __init__(self, attrs):
    """Constructor.

    Args:
      attrs: The attributes associated with this checkout, as a dictionary.
    """
    dict.__init__(self, attrs)

  def AssertPushable(self):
    """Verify that it is safe to push changes to this repository."""
    if not self['pushable']:
      remote = self['remote']
      raise AssertionError('Remote %s is not pushable.' % (remote,))

  def IsBranchableProject(self):
    """Return whether this project is hosted on ChromeOS git servers."""
    return (self['remote'] in constants.CROS_REMOTES and
        re.match(constants.BRANCHABLE_PROJECTS[self['remote']], self['name']))

  def IsPatchable(self):
    """Returns whether this project is patchable.

    For projects that get checked out at multiple paths and/or branches,
    this method can be used to determine which project path a patch
    should be applied to.

    Returns:
      True if the project corresponding to the checkout is patchable.
    """
    # There are 2 ways we determine if a project is patchable.
    # - For an unversioned manifest, if the 'revision' is a raw SHA1 hash
    #   and not a named branch, assume it is a pinned project path and can not
    #   be patched.
    # - For a versioned manifest (generated via repo -r), repo will sets
    #   revision to the actual git sha1 ref, and adds an 'upstream'
    #   field corresponding to branch name in the original manifest. For
    #   a project with a SHA1 'revision' but no named branch in the
    #   'upstream' field, assume it can not be patched.
    return not IsSHA1(self.get('upstream', self['revision']))

  def GetPath(self, absolute=False):
    """Get the path to the checkout.

    Args:
      absolute: If True, return an absolute path. If False,
        return a path relative to the repo root.
    """
    return self['local_path'] if absolute else self['path']


class Manifest(object):
  """SAX handler that parses the manifest document.

  Properties:
    checkouts_by_name: A dictionary mapping the names for <project> tags to a
      list of ProjectCheckout objects.
    checkouts_by_path: A dictionary mapping paths for <project> tags to a single
      ProjectCheckout object.
    default: The attributes of the <default> tag.
    includes: A list of XML files that should be pulled in to the manifest.
      These includes are represented as a list of (name, path) tuples.
    manifest_include_dir: If given, this is where to start looking for
      include targets.
    projects: DEPRECATED. A dictionary mapping the names for <project> tags to
      a single ProjectCheckout object. This is now deprecated, since each
      project can map to multiple ProjectCheckout objects.
    remotes: A dictionary mapping <remote> tags to the associated attributes.
    revision: The revision of the manifest repository. If not specified, this
      will be TOT.
  """

  _instance_cache = {}

  def __init__(self, source, manifest_include_dir=None):
    """Initialize this instance.

    Args:
      source: The path to the manifest to parse.  May be a file handle.
      manifest_include_dir: If given, this is where to start looking for
        include targets.
    """

    self.default = {}
    self.checkouts_by_path = {}
    self.checkouts_by_name = {}
    self.remotes = {}
    self.includes = []
    self.revision = None
    self.manifest_include_dir = manifest_include_dir
    self._RunParser(source)
    self.includes = tuple(self.includes)

  def _RunParser(self, source, finalize=True):
    parser = sax.make_parser()
    handler = sax.handler.ContentHandler()
    handler.startElement = self._ProcessElement
    parser.setContentHandler(handler)
    parser.parse(source)
    if finalize:
      self._FinalizeAllProjectData()

  def _ProcessElement(self, name, attrs):
    """Stores the default manifest properties and per-project overrides."""
    attrs = dict(attrs.items())
    if name == 'default':
      self.default = attrs
    elif name == 'remote':
      attrs.setdefault('alias', attrs['name'])
      self.remotes[attrs['name']] = attrs
    elif name == 'project':
      self.checkouts_by_path[attrs['path']] = attrs
      self.checkouts_by_name.setdefault(attrs['name'], []).append(attrs)
    elif name == 'manifest':
      self.revision = attrs.get('revision')
    elif name == 'include':
      if self.manifest_include_dir is None:
        raise OSError(
            errno.ENOENT, "No manifest_include_dir given, but an include was "
            "encountered; attrs=%r" % (attrs,))
      # Include is calculated relative to the manifest that has the include;
      # thus set the path temporarily to the dirname of the target.
      original_include_dir = self.manifest_include_dir
      include_path = os.path.realpath(
          os.path.join(original_include_dir, attrs['name']))
      self.includes.append((attrs['name'], include_path))
      self._RunParser(include_path, finalize=False)

  def _FinalizeAllProjectData(self):
    """Rewrite projects mixing defaults in and adding our attributes."""
    for path_data in self.checkouts_by_path.itervalues():
      self._FinalizeProjectData(path_data)

  def _FinalizeProjectData(self, attrs):
    """Sets up useful properties for a project.

    Args:
      attrs: The attribute dictionary of a <project> tag.
    """
    for key in ('remote', 'revision'):
      attrs.setdefault(key, self.default.get(key))

    remote = attrs['remote']
    assert remote in self.remotes
    remote_name = attrs['remote_alias'] = self.remotes[remote]['alias']

    # 'repo manifest -r' adds an 'upstream' attribute to the project tag for the
    # manifests it generates.  We can use the attribute to get a valid branch
    # instead of a sha1 for these types of manifests.
    upstream = attrs.get('upstream', attrs['revision'])
    if IsSHA1(upstream):
      # The current version of repo we use has a bug: When you create a new
      # repo checkout from a revlocked manifest, the 'upstream' attribute will
      # just point at a SHA1. The default revision will still be correct,
      # however. For now, return the default revision as our best guess as to
      # what the upstream branch for this repository would be. This guess may
      # sometimes be wrong, but it's correct for all of the repositories where
      # we need to push changes (e.g., the overlays).
      # TODO(davidjames): Either fix the repo bug, or update our logic here to
      # check the manifest repository to find the right tracking branch.
      upstream = self.default.get('revision', 'refs/heads/master')

    attrs['tracking_branch'] = 'refs/remotes/%s/%s' % (
        remote_name, StripRefs(upstream),
    )

    attrs['pushable'] = remote in constants.GIT_REMOTES
    if attrs['pushable']:
      attrs['push_remote'] = remote
      attrs['push_remote_url'] = constants.GIT_REMOTES[remote]
      attrs['push_url'] = '%s/%s' % (attrs['push_remote_url'], attrs['name'])
    groups = set(attrs.get('groups', 'default').replace(',', ' ').split())
    groups.add('default')
    attrs['groups'] = frozenset(groups)

    # Compute the local ref space.
    # Sanitize a couple path fragments to simplify assumptions in this
    # class, and in consuming code.
    attrs.setdefault('path', attrs['name'])
    for key in ('name', 'path'):
      attrs[key] = os.path.normpath(attrs[key])

  @staticmethod
  def _GetManifestHash(source, ignore_missing=False):
    if isinstance(source, basestring):
      try:
        # TODO(build): convert this to osutils.ReadFile once these
        # classes are moved out into their own module (if possible;
        # may still be cyclic).
        with open(source, 'rb') as f:
          return hashlib.md5(f.read()).hexdigest()
      except EnvironmentError as e:
        if e.errno != errno.ENOENT or not ignore_missing:
          raise
    source.seek(0)
    md5 = hashlib.md5(source.read()).hexdigest()
    source.seek(0)
    return md5

  @classmethod
  def Cached(cls, source, manifest_include_dir=None):
    """Return an instance, reusing an existing one if possible.

    May be a seekable filehandle, or a filepath.
    See __init__ for an explanation of these arguments.
    """

    md5 = cls._GetManifestHash(source)
    obj, sources = cls._instance_cache.get(md5, (None, ()))
    if manifest_include_dir is None and sources:
      # We're being invoked in a different way than the orignal
      # caching; disregard the cached entry.
      # Most likely, the instantiation will explode; let it fly.
      obj, sources = None, ()
    for include_target, target_md5 in sources:
      if cls._GetManifestHash(include_target, True) != target_md5:
        obj = None
        break
    if obj is None:
      obj = cls(source, manifest_include_dir=manifest_include_dir)
      sources = tuple((abspath, cls._GetManifestHash(abspath))
                      for (target, abspath) in obj.includes)
      cls._instance_cache[md5] = (obj, sources)

    return obj


class ManifestCheckout(Manifest):
  """A Manifest Handler for a specific manifest checkout."""

  _instance_cache = {}

  # pylint: disable=W0221
  def __init__(self, path, manifest_path=None, search=True):
    """Initialize this instance.

    Args:
      path: Path into a manifest checkout (doesn't have to be the root).
      manifest_path: If supplied, the manifest to use.  Else the manifest
        in the root of the checkout is used.  May be a seekable file handle.
      search: If True, the path can point into the repo, and the root will
        be found automatically.  If False, the path *must* be the root, else
        an OSError ENOENT will be thrown.

    Raises:
      OSError: if a failure occurs.
    """
    self.root, manifest_path = self._NormalizeArgs(
        path, manifest_path, search=search)

    self.manifest_path = os.path.realpath(manifest_path)
    manifest_include_dir = os.path.dirname(self.manifest_path)
    self.manifest_branch = self._GetManifestsBranch(self.root)
    self._content_merging = {}
    self.configured_groups = self._GetManifestGroups(self.root)
    Manifest.__init__(self, self.manifest_path,
                      manifest_include_dir=manifest_include_dir)

  @staticmethod
  def _NormalizeArgs(path, manifest_path=None, search=True):
    root = FindRepoCheckoutRoot(path)
    if root is None:
      raise OSError(errno.ENOENT, "Couldn't find repo root: %s" % (path,))
    root = os.path.normpath(os.path.realpath(root))
    if not search:
      if os.path.normpath(os.path.realpath(path)) != root:
        raise OSError(errno.ENOENT, "Path %s is not a repo root, and search "
                      "is disabled." % path)
    if manifest_path is None:
      manifest_path = os.path.join(root, '.repo', 'manifest.xml')
    return root, manifest_path

  def ProjectIsContentMerging(self, project):
    """Returns whether the given project has content merging enabled in git.

    Note this functionality should *only* be used against a remote that is
    known to be >=gerrit-2.2; <gerrit-2.2 lacks the required branch holding
    this data thus will trigger a RunCommandError.

    Returns:
      True if content merging is enabled.

    Raises:
      AssertionError: If no patchable checkout was found or if the patchable
        checkout does not have a pushable project remote.
      RunCommandError: If the branch can't be fetched due to network
        conditions or if this was invoked against a <gerrit-2.2 server,
        or a mirror that has refs/meta/config stripped from it.
    """
    result = self._content_merging.get(project)
    if result is None:
      checkouts = self.FindCheckouts(project, only_patchable=True)
      if len(checkouts) < 1:
        raise AssertionError('No patchable checkout of %s was found' % project)
      for checkout in checkouts:
        checkout.AssertPushable()
        self._content_merging[project] = result = _GitRepoIsContentMerging(
            checkout['local_path'], checkout['push_remote'])
    return result

  def FindCheckouts(self, project, branch=None, only_patchable=False):
    """Returns the list of checkouts for a given |project|/|branch|.

    Args:
      project: Project name to search for.
      branch: Branch to use.
      only_patchable: Restrict search to patchable project paths.

    Returns:
      A list of ProjectCheckout objects.
    """
    checkouts = []
    for checkout in self.checkouts_by_name.get(project, []):
      if project == checkout['name']:
        if only_patchable and not checkout.IsPatchable():
          continue
        tracking_branch = checkout['tracking_branch']
        if branch is None or StripRefs(branch) == StripRefs(tracking_branch):
          checkouts.append(checkout)
    return checkouts

  def FindCheckout(self, project, branch=None, strict=True):
    """Returns the checkout associated with a given project/branch.

    Args:
      project: The project to look for.
      branch: The branch that the project is tracking.
      strict: Raise AssertionError if a checkout cannot be found.

    Returns:
      A ProjectCheckout object.

    Raises:
      AssertionError if there is more than one checkout associated with the
      given project/branch combination.
    """
    checkouts = self.FindCheckouts(project, branch)
    if len(checkouts) < 1:
      if strict:
        raise AssertionError('Could not find checkout of %s' % (project,))
      return None
    elif len(checkouts) > 1:
      raise AssertionError('Too many checkouts found for %s' % project)
    return checkouts[0]

  def ListCheckouts(self):
    """List the checkouts in the manifest.

    Returns:
      A list of ProjectCheckout objects.
    """
    return self.checkouts_by_path.values()

  def FindCheckoutFromPath(self, path, strict=True):
    """Find the associated checkouts for a given |path|.

    The |path| can either be to the root of a project, or within the
    project itself (chromite/buildbot for example).  It may be relative
    to the repo root, or an absolute path.  If |path| is not within a
    checkout, return None.

    Args:
      path: Path to examine.
      strict: If True, fail when no checkout is found.

    Returns:
      None if no checkout is found, else the checkout.
    """
    # Realpath everything sans the target to keep people happy about
    # how symlinks are handled; exempt the final node since following
    # through that is unlikely even remotely desired.
    tmp = os.path.join(self.root, os.path.dirname(path))
    path = os.path.join(os.path.realpath(tmp), os.path.basename(path))
    path = os.path.normpath(path) + '/'
    candidates = []
    for checkout in self.ListCheckouts():
      if path.startswith(checkout['local_path'] + '/'):
        candidates.append((checkout['path'], checkout))

    if not candidates:
      if strict:
        raise AssertionError('Could not find repo project at %s' % (path,))
      return None

    # The checkout with the greatest common path prefix is the owner of
    # the given pathway. Return that.
    return max(candidates)[1]

  def _FinalizeAllProjectData(self):
    """Rewrite projects mixing defaults in and adding our attributes."""
    Manifest._FinalizeAllProjectData(self)
    for key, value in self.checkouts_by_path.iteritems():
      self.checkouts_by_path[key] = ProjectCheckout(value)
    for key, value in self.checkouts_by_name.iteritems():
      self.checkouts_by_name[key] = \
          [ProjectCheckout(x) for x in value]

  def _FinalizeProjectData(self, attrs):
    Manifest._FinalizeProjectData(self, attrs)
    attrs['local_path'] = os.path.join(self.root, attrs['path'])

  @staticmethod
  def _GetManifestGroups(root):
    """Discern which manifest groups were enabled for this checkout."""
    path = os.path.join(root, '.repo', 'manifests.git')
    try:
      result = RunGit(path, ['config', '--get', 'manifest.groups'])
    except cros_build_lib.RunCommandError as e:
      if e.result.returncode == 1:
        # Value wasn't found, which is fine.
        return frozenset(['default'])
      # If exit code 2, multiple values matched (broken checkout).  Anything
      # else, git internal error.
      raise

    result = result.output.replace(',', ' ').split()
    if not result:
      result = ['default']
    return frozenset(result)

  @staticmethod
  def _GetManifestsBranch(root):
    """Get the tracking branch of the manifest repository.

    Returns:
      The branch name.
    """
    # Suppress the normal "if it ain't refs/heads, we don't want none o' that"
    # check for the merge target; repo writes the ambigious form of the branch
    # target for `repo init -u url -b some-branch` usages (aka, 'master'
    # instead of 'refs/heads/master').
    path = os.path.join(root, '.repo', 'manifests')
    current_branch = GetCurrentBranch(path)
    if current_branch != 'default':
      raise OSError(errno.ENOENT,
                    "Manifest repository at %s is checked out to %s.  "
                    "It should be checked out to 'default'."
                    % (root, 'detached HEAD' if current_branch is None
                       else current_branch))

    result = GetTrackingBranchViaGitConfig(
        path, 'default', allow_broken_merge_settings=True, for_checkout=False)

    if result is not None:
      return StripRefsHeads(result[1], False)

    raise OSError(errno.ENOENT,
                  "Manifest repository at %s is checked out to 'default', but "
                  "the git tracking configuration for that branch is broken; "
                  "failing due to that." % (root,))

  # pylint: disable=W0221
  @classmethod
  def Cached(cls, path, manifest_path=None, search=True):
    """Return an instance, reusing an existing one if possible.

    Args:
      path: The pathway into a checkout; the root will be found automatically.
      manifest_path: if given, the manifest.xml to use instead of the
        checkouts internal manifest.  Use with care.
      search: If True, the path can point into the repo, and the root will
        be found automatically.  If False, the path *must* be the root, else
        an OSError ENOENT will be thrown.
    """
    root, manifest_path = cls._NormalizeArgs(path, manifest_path,
                                             search=search)

    md5 = cls._GetManifestHash(manifest_path)
    obj, sources = cls._instance_cache.get((root, md5), (None, ()))
    for include_target, target_md5 in sources:
      if cls._GetManifestHash(include_target, True) != target_md5:
        obj = None
        break
    if obj is None:
      obj = cls(root, manifest_path=manifest_path)
      sources = tuple((abspath, cls._GetManifestHash(abspath))
                      for (target, abspath) in obj.includes)
      cls._instance_cache[(root, md5)] = (obj, sources)
    return obj


def _GitRepoIsContentMerging(git_repo, remote):
  """Identify if the given git repo has content merging marked.

  This is a gerrit >=2.2 bit of functionality; specifically, the content
  merging configuration is stored in a specially crafted branch which
  we access.  If the branch is fetchable, we either return True or False.

  Args:
    git_repo: The local path to the git repository to inspect.
    remote: The configured remote to use from the given git repository.

  Returns:
    True if content merging is enabled, False if not.

  Raises:
    RunCommandError: Thrown if fetching fails due to either the namespace
      not existing, or a network error intervening.
  """
  # Need to use the manifest to get upstream gerrit; also, if upstream
  # doesn't provide a refs/meta/config for the repo, this will fail.
  RunGit(git_repo, ['fetch', remote, 'refs/meta/config:refs/meta/config'])

  content = RunGit(git_repo, ['show', 'refs/meta/config:project.config'],
                   error_code_ok=True)

  if content.returncode != 0:
    return False

  try:
    result = RunGit(git_repo, ['config', '-f', '/dev/stdin', '--get',
                               'submit.mergeContent'], input=content.output)
    return result.output.strip().lower() == 'true'
  except cros_build_lib.RunCommandError as e:
    # If the field isn't set at all, exit code is 1.
    # Anything else is a bad invocation or an indecipherable state.
    if e.result.returncode != 1:
      raise

  return False


def RunGit(git_repo, cmd, retry=True, **kwargs):
  """RunCommand wrapper for git commands.

  This suppresses print_cmd, and suppresses output by default.  Git
  functionality w/in this module should use this unless otherwise
  warranted, to standardize git output (primarily, keeping it quiet
  and being able to throw useful errors for it).

  Args:
    git_repo: Pathway to the git repo to operate on.
    cmd: A sequence of the git subcommand to run.  The 'git' prefix is
      added automatically.  If you wished to run 'git remote update',
      this would be ['remote', 'update'] for example.
    retry: If set, retry on transient errors. Defaults to True.
    kwargs: Any RunCommand or GenericRetry options/overrides to use.

  Returns:
    A CommandResult object.
  """

  def _ShouldRetry(exc):
    """Returns True if push operation failed with a transient error."""
    if (isinstance(exc, cros_build_lib.RunCommandError)
        and exc.result and exc.result.error and
        GIT_TRANSIENT_ERRORS_RE.search(exc.result.error)):
      cros_build_lib.Warning('git reported transient error (cmd=%s); retrying',
                             cros_build_lib.CmdToStr(cmd), exc_info=True)
      return True
    return False

  max_retry = kwargs.pop('max_retry', DEFAULT_RETRIES if retry else 0)
  kwargs.setdefault('print_cmd', False)
  kwargs.setdefault('sleep', DEFAULT_RETRY_INTERVAL)
  kwargs.setdefault('cwd', git_repo)
  kwargs.setdefault('capture_output', True)
  return retry_util.GenericRetry(
      _ShouldRetry, max_retry, cros_build_lib.RunCommand,
      ['git'] + cmd, **kwargs)


def GetProjectUserEmail(git_repo):
  """Get the email configured for the project."""
  output = RunGit(git_repo, ['var', 'GIT_COMMITTER_IDENT']).output
  m = re.search(r'<([^>]*)>', output.strip())
  return m.group(1) if m else None


def MatchBranchName(git_repo, pattern, namespace=''):
  """Return branches who match the specified regular expression.

  Args:
    git_repo: The git repository to operate upon.
    pattern: The regexp to search with.
    namespace: The namespace to restrict search to (e.g. 'refs/heads/').

  Returns:
    List of matching branch names (with |namespace| trimmed).
  """
  match = re.compile(pattern, flags=re.I)
  output = RunGit(git_repo, ['ls-remote', git_repo, namespace + '*']).output
  branches = [x.split()[1] for x in output.splitlines()]
  branches = [x[len(namespace):] for x in branches if x.startswith(namespace)]
  return [x for x in branches if match.search(x)]


class AmbiguousBranchName(Exception):
  """Error if given branch name matches too many branches."""


def MatchSingleBranchName(*args, **kwargs):
  """Match exactly one branch name, else throw an exception.

  Args:
    See MatchBranchName for more details; all args are passed on.

  Returns:
    The branch name.

  Raises:
    raise AmbiguousBranchName if we did not match exactly one branch.
  """
  ret = MatchBranchName(*args, **kwargs)
  if len(ret) != 1:
    raise AmbiguousBranchName('Did not match exactly 1 branch: %r' % ret)
  return ret[0]


def GetTrackingBranchViaGitConfig(git_repo, branch, for_checkout=True,
                                  allow_broken_merge_settings=False,
                                  recurse=10):
  """Pull the remote and upstream branch of a local branch

  Args:
    git_repo: The git repository to operate upon.
    branch: The branch to inspect.
    for_checkout: Whether to return localized refspecs, or the remote's
      view of it.
    allow_broken_merge_settings: Repo in a couple of spots writes invalid
      branch.mybranch.merge settings; if these are encountered, they're
      normally treated as an error and this function returns None.  If
      this option is set to True, it suppresses this check.
    recurse: If given and the target is local, then recurse through any
      remote=. (aka locals).  This is enabled by default, and is what allows
      developers to have multiple local branches of development dependent
      on one another; disabling this makes that work flow impossible,
      thus disable it only with good reason.  The value given controls how
      deeply to recurse.  Defaults to tracing through 10 levels of local
      remotes. Disabling it is a matter of passing 0.

  Returns:
    A tuple of the remote and the ref name of the tracking branch, or
    None if it couldn't be found.
  """
  try:
    cmd = ['config', '--get-regexp',
           r'branch\.%s\.(remote|merge)' % re.escape(branch)]
    data = RunGit(git_repo, cmd).output.splitlines()

    prefix = 'branch.%s.' % (branch,)
    data = [x.split() for x in data]
    vals = dict((x[0][len(prefix):], x[1]) for x in data)
    if len(vals) != 2:
      if not allow_broken_merge_settings:
        return None
      elif 'merge' not in vals:
        # There isn't anything we can do here.
        return None
      elif 'remote' not in vals:
        # Repo v1.9.4 and up occasionally invalidly leave the remote out.
        # Only occurs for the manifest repo fortunately.
        vals['remote'] = 'origin'
    remote, rev = vals['remote'], vals['merge']
    # Suppress non branches; repo likes to write revisions and tags here,
    # which is wrong (git hates it, nor will it honor it).
    if rev.startswith('refs/remotes/'):
      if for_checkout:
        return remote, rev
      # We can't backtrack from here, or at least don't want to.
      # This is likely refs/remotes/m/ which repo writes when dealing
      # with a revision locked manifest.
      return None
    if not rev.startswith('refs/heads/'):
      # We explicitly don't allow pushing to tags, nor can one push
      # to a sha1 remotely (makes no sense).
      if not allow_broken_merge_settings:
        return None
    elif remote == '.':
      if recurse == 0:
        raise Exception(
            "While tracing out tracking branches, we recursed too deeply: "
            "bailing at %s" % branch)
      return GetTrackingBranchViaGitConfig(
          git_repo, StripRefsHeads(rev), for_checkout=for_checkout,
          allow_broken_merge_settings=allow_broken_merge_settings,
          recurse=recurse - 1)
    elif for_checkout:
      rev = 'refs/remotes/%s/%s' % (remote, StripRefsHeads(rev))
    return remote, rev
  except cros_build_lib.RunCommandError as e:
    # 1 is the retcode for no matches.
    if e.result.returncode != 1:
      raise
  return None


def GetTrackingBranchViaManifest(git_repo, for_checkout=True, for_push=False,
                                 manifest=None):
  """Gets the appropriate push branch via the manifest if possible.

  Args:
    git_repo: The git repo to operate upon.
    for_checkout: Whether to return localized refspecs, or the remote's
      view of it.  Note that depending on the remote, the remote may differ
      if for_push is True or set to False.
    for_push: Controls whether the remote and refspec returned is explicitly
      for pushing.
    manifest: A Manifest instance if one is available, else a
      ManifestCheckout is created and used.

  Returns:
    A tuple of a git target repo and the remote ref to push to, or
    None if it couldnt be found.  If for_checkout, then it returns
    the localized version of it.
  """
  try:
    if manifest is None:
      manifest = ManifestCheckout.Cached(git_repo)

    checkout = manifest.FindCheckoutFromPath(git_repo, strict=False)

    if checkout is None:
      return None

    if for_push:
      checkout.AssertPushable()

    if for_push:
      remote = checkout['push_remote']
    else:
      remote = checkout['remote']

    if for_checkout:
      revision = checkout['tracking_branch']
    else:
      revision = checkout['revision']
      if not revision.startswith('refs/heads/'):
        return None

    return remote, revision
  except EnvironmentError as e:
    if e.errno != errno.ENOENT:
      raise
  return None


def GetTrackingBranch(git_repo, branch=None, for_checkout=True, fallback=True,
                      manifest=None, for_push=False):
  """Gets the appropriate push branch for the specified directory.

  This function works on both repo projects and regular git checkouts.

  Assumptions:
   1. We assume the manifest defined upstream is desirable.
   2. No manifest?  Assume tracking if configured is accurate.
   3. If none of the above apply, you get 'origin', 'master' or None,
      depending on fallback.

  Args:
    git_repo: Git repository to operate upon.
    branch: Find the tracking branch for this branch.  Defaults to the
      current branch for |git_repo|.
    for_checkout: Whether to return localized refspecs, or the remotes
      view of it.
    fallback: If true and no remote/branch could be discerned, return
      'origin', 'master'.  If False, you get None.
      Note that depending on the remote, the remote may differ
      if for_push is True or set to False.
    for_push: Controls whether the remote and refspec returned is explicitly
      for pushing.
    manifest: A Manifest instance if one is available, else a
      ManifestCheckout is created and used.

  Returns:
    A tuple of a git target repo and the remote ref to push to.
  """

  result = GetTrackingBranchViaManifest(git_repo, for_checkout=for_checkout,
                                        manifest=manifest, for_push=for_push)
  if result is not None:
    return result

  if branch is None:
    branch = GetCurrentBranch(git_repo)
  if branch:
    result = GetTrackingBranchViaGitConfig(git_repo, branch,
                                           for_checkout=for_checkout)
    if result is not None:
      if (result[1].startswith('refs/heads/') or
          result[1].startswith('refs/remotes/')):
        return result

  if not fallback:
    return None
  if for_checkout:
    return 'origin', 'refs/remotes/origin/master'
  return 'origin', 'master'


def CreateBranch(git_repo, branch, branch_point='HEAD', track=False):
  """Create a branch.

  Args:
    git_repo: Git repository to act on.
    branch: Name of the branch to create.
    branch_point: The ref to branch from.  Defaults to 'HEAD'.
    track: Whether to setup the branch to track its starting ref.
  """
  cmd = ['checkout', '-B', branch, branch_point]
  if track:
    cmd.append('--track')
  RunGit(git_repo, cmd)


def GitPush(git_repo, refspec, push_to, dryrun=False, force=False, retry=True):
  """Wrapper for pushing to a branch.

  Args:
    git_repo: Git repository to act on.
    refspec: The local ref to push to the remote.
    push_to: A RemoteRef object representing the remote ref to push to.
    dryrun: Do not actually push anything.  Uses the --dry-run option
      built into git.
    force: Whether to bypass non-fastforward checks.
    retry: Retry a push in case of transient errors.
  """
  cmd = ['push', push_to.remote, '%s:%s' % (refspec, push_to.ref)]

  if dryrun:
    # The 'git push' command has --dry-run support built in, so leverage that.
    cmd.append('--dry-run')

  if force:
    cmd.append('--force')

  RunGit(git_repo, cmd, retry=retry)


# TODO(build): Switch callers of this function to use CreateBranch instead.
def CreatePushBranch(branch, git_repo, sync=True, remote_push_branch=None):
  """Create a local branch for pushing changes inside a repo repository.

  Args:
    branch: Local branch to create.
    git_repo: Git repository to create the branch in.
    sync: Update remote before creating push branch.
    remote_push_branch: A tuple of the (remote, branch) to push to. i.e.,
                        ('cros', 'master').  By default it tries to
                        automatically determine which tracking branch to use
                        (see GetTrackingBranch()).
  """
  if not remote_push_branch:
    remote, push_branch = GetTrackingBranch(git_repo, for_push=True)
  else:
    remote, push_branch = remote_push_branch

  if sync:
    cmd = ['remote', 'update', remote]
    RunGit(git_repo, cmd)

  RunGit(git_repo, ['checkout', '-B', branch, '-t', push_branch])


def SyncPushBranch(git_repo, remote, rebase_target):
  """Sync and rebase a local push branch to the latest remote version.

  Args:
    git_repo: Git repository to rebase in.
    remote: The remote returned by GetTrackingBranch(for_push=True)
    rebase_target: The branch name returned by GetTrackingBranch().  Must
      start with refs/remotes/ (specifically must be a proper remote
      target rather than an ambiguous name).
  """
  if not rebase_target.startswith("refs/remotes/"):
    raise Exception(
        "Was asked to rebase to a non branch target w/in the push pathways.  "
        "This is highly indicative of an internal bug.  remote %s, rebase %s"
        % (remote, rebase_target))

  cmd = ['remote', 'update', remote]
  RunGit(git_repo, cmd)

  try:
    RunGit(git_repo, ['rebase', rebase_target])
  except cros_build_lib.RunCommandError:
    # Looks like our change conflicts with upstream. Cleanup our failed
    # rebase.
    RunGit(git_repo, ['rebase', '--abort'], error_code_ok=True)
    raise


# TODO(build): Switch this to use the GitPush function.
def PushWithRetry(branch, git_repo, dryrun=False, retries=5):
  """General method to push local git changes.

  This method only works with branches created via the CreatePushBranch
  function.

  Args:
    branch: Local branch to push.  Branch should have already been created
      with a local change committed ready to push to the remote branch.  Must
      also already be checked out to that branch.
    git_repo: Git repository to push from.
    dryrun: Git push --dry-run if set to True.
    retries: The number of times to retry before giving up, default: 5

  Raises:
    GitPushFailed if push was unsuccessful after retries
  """
  remote, ref = GetTrackingBranch(git_repo, branch, for_checkout=False,
                                  for_push=True)
  # Don't like invoking this twice, but there is a bit of API
  # impedence here; cros_mark_as_stable
  _, local_ref = GetTrackingBranch(git_repo, branch, for_push=True)

  if not ref.startswith("refs/heads/"):
    raise Exception("Was asked to push to a non branch namespace: %s" % (ref,))

  push_command = ['push', remote, '%s:%s' % (branch, ref)]
  cros_build_lib.Debug("Trying to push %s to %s:%s", git_repo, branch, ref)

  if dryrun:
    push_command.append('--dry-run')
  for retry in range(1, retries + 1):
    SyncPushBranch(git_repo, remote, local_ref)
    try:
      RunGit(git_repo, push_command)
      break
    except cros_build_lib.RunCommandError:
      if retry < retries:
        Warning('Error pushing changes trying again (%s/%s)', retry, retries)
        time.sleep(5 * retry)
        continue
      raise

  cros_build_lib.Info("Successfully pushed %s to %s:%s", git_repo, branch, ref)


def CleanAndDetachHead(git_repo):
  """Remove all local changes and checkout a detached head.

  Args:
    git_repo: Directory of git repository.
  """
  RunGit(git_repo, ['am', '--abort'], error_code_ok=True)
  RunGit(git_repo, ['rebase', '--abort'], error_code_ok=True)
  RunGit(git_repo, ['clean', '-dfx'])
  RunGit(git_repo, ['checkout', '--detach', '-f', 'HEAD'])


def CleanAndCheckoutUpstream(git_repo, refresh_upstream=True):
  """Remove all local changes and checkout the latest origin.

  All local changes in the supplied repo will be removed. The branch will
  also be switched to a detached head pointing at the latest origin.

  Args:
    git_repo: Directory of git repository.
    refresh_upstream: If True, run a remote update prior to checking it out.
  """
  remote, local_upstream = GetTrackingBranch(git_repo,
                                             for_push=refresh_upstream)
  CleanAndDetachHead(git_repo)
  if refresh_upstream:
    RunGit(git_repo, ['remote', 'update', remote])
  RunGit(git_repo, ['checkout', local_upstream])


def GetChromiteTrackingBranch():
  """Returns the remote branch associated with chromite."""
  cwd = os.path.dirname(os.path.realpath(__file__))
  result = GetTrackingBranch(cwd, for_checkout=False, fallback=False)
  if result:
    _remote, branch = result
    if branch.startswith('refs/heads/'):
      # Normal scenario.
      return StripRefsHeads(branch)
    # Reaching here means it was refs/remotes/m/blah, or just plain invalid,
    # or that we're on a detached head in a repo not managed by chromite.

  # Manually try the manifest next.
  try:
    manifest = ManifestCheckout.Cached(cwd)
    # Ensure the manifest knows of this checkout.
    if manifest.FindCheckoutFromPath(cwd, strict=False):
      return manifest.manifest_branch
  except EnvironmentError as e:
    if e.errno != errno.ENOENT:
      raise

  # Not a manifest checkout.
  Warning(
      "Chromite checkout at %s isn't controlled by repo, nor is it on a "
      "branch (or if it is, the tracking configuration is missing or broken).  "
      "Falling back to assuming the chromite checkout is derived from "
      "'master'; this *may* result in breakage." % cwd)
  return 'master'
