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
# Now restore it so that relative scripts don't get cranky.
sys.path.pop(0)
del _path

EXTERNAL_GERRIT_SSH_REMOTE = 'gerrit'


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


def FindGitSubmoduleCheckoutRoot(path, remote, url):
  """Get the root of your git submodule checkout, looking up from |path|.

  This function goes up the tree starting from |path| and looks for a .git/ dir
  configured with a |remote| pointing at |url|.

  Arguments:
    path: The path to start searching from.
    remote: The remote to compare the |url| with.
    url: The exact URL the |remote| needs to be pointed at.
  """
  def test_config(path):
    if os.path.isdir(path):
      remote_url = cros_build_lib.RunCommand(
          ['git', '--git-dir', path, 'config', 'remote.%s.url' % remote],
          redirect_stdout=True, debug_level=logging.DEBUG).output.strip()
      if remote_url == url:
        return True
    return False

  root_dir = osutils.FindInPathParents('.git', path, test_func=test_config)
  if root_dir:
    return os.path.dirname(root_dir)
  return None


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


def GetProjectDir(cwd, project):
  """Returns the absolute path to a project.

  Args:
    cwd: a directory within a repo-managed checkout.
    project: the name of the project to get the path for.
  """
  return ManifestCheckout.Cached(cwd).GetProjectPath(project, True)


def IsGitRepo(cwd):
  """Checks if there's a git repo rooted at a directory."""
  return os.path.isdir(os.path.join(cwd, '.git'))


_HEX_CHARS = frozenset(string.hexdigits)
def IsSHA1(value, full=True):
  """Returns True if the given value looks like a sha1.

  If full is True, then it must be full length- 40 chars.  If False, >=6, and
  <40."""
  if not all(x in _HEX_CHARS for x in value):
    return False
  l = len(value)
  if full:
    return l == 40
  return l >= 6 and l <= 40


def IsRefsTags(value):
  """Return True if the given value looks like a tag.

  Currently this is identified via refs/tags/ prefixing."""
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
  except cros_build_lib.RunCommandError, e:
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


class Manifest(object):
  """SAX handler that parses the manifest document.

  Properties:
    default: the attributes of the <default> tag.
    projects: a dictionary keyed by project name containing the attributes of
              each <project> tag.
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
    self.projects = {}
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
      # Rewrite projects mixing defaults in and adding our attributes.
      for data in self.projects.itervalues():
        self._FinalizeProjectData(data)

  def _ProcessElement(self, name, attrs):
    """Stores the default manifest properties and per-project overrides."""
    attrs = dict(attrs.items())
    if name == 'default':
      self.default = attrs
    elif name == 'remote':
      attrs.setdefault('alias', attrs['name'])
      self.remotes[attrs['name']] = attrs
    elif name == 'project':
      self.projects[attrs['name']] = attrs
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

  def ProjectExists(self, project):
    """Returns True if a project is in this manifest."""
    return os.path.normpath(project) in self.projects

  def GetProjectPath(self, project):
    """Returns the relative path for a project.

    Raises:
      KeyError if the project isn't known."""
    return self.projects[os.path.normpath(project)]['path']

  def _FinalizeProjectData(self, attrs):
    for key in ('remote', 'revision'):
      attrs.setdefault(key, self.default.get(key))

    remote = attrs['remote']
    assert remote in self.remotes
    remote_name = self.remotes[remote]['alias']

    local_rev = rev = attrs['revision']
    if rev.startswith('refs/heads/'):
      local_rev = 'refs/remotes/%s/%s' % (remote_name, StripRefsHeads(rev))

    attrs['local_revision'] = local_rev

    attrs['pushable'] = remote in constants.CROS_REMOTES
    if attrs['pushable']:
      if remote == constants.EXTERNAL_REMOTE:
        attrs['push_remote'] = EXTERNAL_GERRIT_SSH_REMOTE
        if rev.startswith('refs/heads/'):
          attrs['push_remote_local'] = 'refs/remotes/%s/%s' % (
              EXTERNAL_GERRIT_SSH_REMOTE, StripRefsHeads(rev))
        else:
          attrs['push_remote_local'] = rev
      elif remote == constants.INTERNAL_REMOTE:
        # For cros-internal, it's already accessing gerrit directly; thus
        # just use that.
        attrs['push_remote'] = attrs['remote']
        attrs['push_remote_local'] = attrs['local_revision']

      attrs['push_remote_url'] = constants.CROS_REMOTES[remote]
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

  def GetAttributeForProject(self, project, attribute):
    """Gets an attribute for a project, falling back to defaults if needed."""
    return self.projects[project].get(attribute)

  def GetProjectsLocalRevision(self, project):
    """Returns the upstream defined revspec for a project.

    Args:
      project: Which project we're looking at.
    """
    return self.GetAttributeForProject(project, 'local_revision')

  def AssertProjectIsPushable(self, project):
    """Verify that the project has push_* attributes populated."""
    data = self.projects[project]
    if not data['pushable']:
      raise AssertionError('Remote %s is not pushable.' % data['remote'])

  @staticmethod
  def _GetManifestHash(source, ignore_missing=False):
    if isinstance(source, basestring):
      try:
        # TODO(build): convert this to osutils.ReadFile once these
        # classes are moved out into their own module (if possible;
        # may still be cyclic).
        with open(source, 'rb') as f:
          # pylint: disable=E1101
          return hashlib.md5(f.read()).hexdigest()
      except EnvironmentError, e:
        if e.errno != errno.ENOENT or not ignore_missing:
          raise
    source.seek(0)
    # pylint: disable=E1101
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
    self.default_branch = 'refs/remotes/m/%s' % self.manifest_branch
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

  def GetProjectsLocalRevision(self, project, fallback=True):
    """Returns the upstream defined revspec for a project.

    Args:
      project: Which project we're looking at.
      fallback: If True, return the revision for revision locked manifests.
        If False, remotes/m/<default_branch> is returned.
    """
    ref = Manifest.GetProjectsLocalRevision(self, project)
    if ref.startswith("refs/") or not fallback:
      return ref
    # Revlocked manifests return sha1s; use the repo defined branch
    # so tracking is supported.
    return self.default_branch

  def ProjectIsContentMerging(self, project):
    """Returns whether the given project has content merging enabled in git.

    Note this functionality should *only* be used against a remote that is
    known to be >=gerrit-2.2; <gerrit-2.2 lacks the required branch holding
    this data thus will trigger a RunCommandError.

    Returns:
      True if content merging is enabled.
    Raises:
      RunCommandError: If the branch can't be fetched due to network
        conditions or if this was invoked against a <gerrit-2.2 server,
        or a mirror that has refs/meta/config stripped from it."""
    result = self._content_merging.get(project)
    if result is None:
      self.AssertProjectIsPushable(project)
      data = self.projects[project]
      self._content_merging[project] = result = _GitRepoIsContentMerging(
          data['local_path'], data['push_remote'])
    return result

  def FindProjectFromPath(self, path):
    """Find the associated projects for a given pathway.

    The pathway can either be to the root of a project, or within the
    project itself (chromite/buildbot for example).  It may be relative
    to the repo root, or an absolute path.  If it is absolute path,
    it's the callers responsibility to ensure the pathway intersects
    the root of the checkout.

    Returns:
      None if no project is found, else the project."""
    # Realpath everything sans the target to keep people happy about
    # how symlinks are handled; exempt the final node since following
    # through that is unlikely even remotely desired.
    tmp = os.path.join(self.root, os.path.dirname(path))
    path = os.path.join(os.path.realpath(tmp), os.path.basename(path))
    path = os.path.normpath(path) + '/'
    candidates = [(x['path'], name) for name, x in self.projects.iteritems()
                  if path.startswith(x['local_path'] + '/')]
    if not candidates:
      return None
    # That which has the greatest common path prefix is the owner of
    # the given pathway, thus we return that.
    return sorted(candidates)[-1][1]

  def _FinalizeProjectData(self, attrs):
    Manifest._FinalizeProjectData(self, attrs)
    attrs['local_path'] = os.path.join(self.root, attrs['path'])

  @staticmethod
  def _GetManifestGroups(root):
    """Discern which manifest groups were enabled for this checkout."""
    path = os.path.join(root, '.repo', 'manifests.git')
    try:
      result = RunGit(path, ['config', '--get', 'manifest.groups'])
    except cros_build_lib.RunCommandError, e:
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

  def GetProjectPath(self, project, absolute=False):
    """Returns the path for a project.

    Args:
      project: Project to get the path for.
      absolute:  If True, return an absolute pathway.  If False,
        relative pathway.

    Raises:
      KeyError if the project isn't known."""
    path = Manifest.GetProjectPath(self, project)
    if absolute:
      return os.path.join(self.root, path)
    return path

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
      obj = cls(manifest_path)
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
  except cros_build_lib.RunCommandError, e:
    # If the field isn't set at all, exit code is 1.
    # Anything else is a bad invocation or an indecipherable state.
    if e.result.returncode != 1:
      raise

  return False


def RunGit(git_repo, cmd, **kwds):
  """RunCommandCaptureOutput wrapper for git commands.

  This suppresses print_cmd, and suppresses output by default.  Git
  functionality w/in this module should use this unless otherwise
  warranted, to standardize git output (primarily, keeping it quiet
  and being able to throw useful errors for it).

  Args:
    git_repo: Pathway to the git repo to operate on.
    cmd: A sequence of the git subcommand to run.  The 'git' prefix is
      added automatically.  If you wished to run 'git remote update',
      this would be ['remote', 'update'] for example.
    kwds: Any RunCommand options/overrides to use.
  Returns:
    A CommandResult object."""
  kwds.setdefault('print_cmd', False)
  cros_build_lib.Debug("RunGit(%r, %r, **%r)", git_repo, cmd, kwds)
  return cros_build_lib.RunCommandCaptureOutput(['git'] + cmd, cwd=git_repo,
                                                **kwds)


def GetProjectUserEmail(git_repo):
  """Get the email configured for the project ."""
  output = RunGit(git_repo, ['var', 'GIT_COMMITTER_IDENT']).output
  m = re.search('<([^>]*)>', output.strip())
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


def MatchSingleBranchName(*args, **kwds):
  """Match exactly one branch name, else throw an exception.

  Args:
    See MatchBranchName for more details; all args are passed on.
  Returns:
    The branch name.
  Raises:
    raise AmbiguousBranchName if we did not match exactly one branch.
  """
  ret = MatchBranchName(*args, **kwds)
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

    project = manifest.FindProjectFromPath(git_repo)
    if project is None:
      return None

    if for_push:
      manifest.AssertProjectIsPushable(project)

    data = manifest.projects[project]
    if for_push:
      remote = data['push_remote']
    else:
      remote = data['remote']

    if for_checkout:
      revision = data['local_revision']
      if for_push:
        revision = data['push_remote_local']
    else:
      revision = data['revision']
      if not revision.startswith("refs/heads/"):
        return None

    return remote, revision
  except EnvironmentError, e:
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
    cros_build_lib.RetryCommand(RunGit, 3, git_repo, cmd, sleep=10,
                                retry_on=(1,))

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
  cros_build_lib.RetryCommand(RunGit, 3, git_repo, cmd, sleep=10,
                              retry_on=(1,))

  try:
    RunGit(git_repo, ['rebase', rebase_target])
  except cros_build_lib.RunCommandError:
    # Looks like our change conflicts with upstream. Cleanup our failed
    # rebase.
    RunGit(git_repo, ['rebase', '--abort'], error_code_ok=True)
    raise


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
  RunGit(git_repo, ['am', '--abort'], error_code_ok=True)
  RunGit(git_repo, ['rebase', '--abort'], error_code_ok=True)
  if refresh_upstream:
    cmd = ['remote', 'update', remote]
    cros_build_lib.RetryCommand(RunGit, 3, git_repo, cmd, sleep=10,
                                retry_on=(1,))
  RunGit(git_repo, ['clean', '-df'])
  RunGit(git_repo, ['reset', '--hard', 'HEAD'])
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
    if manifest.FindProjectFromPath(cwd):
      return manifest.manifest_branch
  except EnvironmentError, e:
    if e.errno != errno.ENOENT:
      raise

  # Not a manifest checkout.
  Warning(
      "Chromite checkout at %s isn't controlled by repo, nor is it on a "
      "branch (or if it is, the tracking configuration is missing or broken).  "
      "Falling back to assuming the chromite checkout is derived from "
      "'master'; this *may* result in breakage." % cwd)
  return 'master'
