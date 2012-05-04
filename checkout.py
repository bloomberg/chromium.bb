# coding=utf8
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Manages a project checkout.

Includes support for svn, git-svn and git.
"""

import ConfigParser
import fnmatch
import logging
import os
import re
import shutil
import subprocess
import sys
import tempfile

import patch
import scm
import subprocess2


def get_code_review_setting(path, key,
    codereview_settings_file='codereview.settings'):
  """Parses codereview.settings and return the value for the key if present.

  Don't cache the values in case the file is changed."""
  # TODO(maruel): Do not duplicate code.
  settings = {}
  try:
    settings_file = open(os.path.join(path, codereview_settings_file), 'r')
    try:
      for line in settings_file.readlines():
        if not line or line.startswith('#'):
          continue
        if not ':' in line:
          # Invalid file.
          return None
        k, v = line.split(':', 1)
        settings[k.strip()] = v.strip()
    finally:
      settings_file.close()
  except IOError:
    return None
  return settings.get(key, None)


class PatchApplicationFailed(Exception):
  """Patch failed to be applied."""
  def __init__(self, filename, status):
    super(PatchApplicationFailed, self).__init__(filename, status)
    self.filename = filename
    self.status = status


class CheckoutBase(object):
  # Set to None to have verbose output.
  VOID = subprocess2.VOID

  def __init__(self, root_dir, project_name, post_processors):
    """
    Args:
      post_processor: list of lambda(checkout, patches) to call on each of the
                      modified files.
    """
    super(CheckoutBase, self).__init__()
    self.root_dir = root_dir
    self.project_name = project_name
    if self.project_name is None:
      self.project_path = self.root_dir
    else:
      self.project_path = os.path.join(self.root_dir, self.project_name)
    # Only used for logging purposes.
    self._last_seen_revision = None
    self.post_processors = post_processors
    assert self.root_dir
    assert self.project_path

  def get_settings(self, key):
    return get_code_review_setting(self.project_path, key)

  def prepare(self, revision):
    """Checks out a clean copy of the tree and removes any local modification.

    This function shouldn't throw unless the remote repository is inaccessible,
    there is no free disk space or hard issues like that.

    Args:
      revision: The revision it should sync to, SCM specific.
    """
    raise NotImplementedError()

  def apply_patch(self, patches, post_processors=None):
    """Applies a patch and returns the list of modified files.

    This function should throw patch.UnsupportedPatchFormat or
    PatchApplicationFailed when relevant.

    Args:
      patches: patch.PatchSet object.
    """
    raise NotImplementedError()

  def commit(self, commit_message, user):
    """Commits the patch upstream, while impersonating 'user'."""
    raise NotImplementedError()


class RawCheckout(CheckoutBase):
  """Used to apply a patch locally without any intent to commit it.

  To be used by the try server.
  """
  def prepare(self, revision):
    """Stubbed out."""
    pass

  def apply_patch(self, patches, post_processors=None):
    """Ignores svn properties."""
    post_processors = post_processors or self.post_processors or []
    for p in patches:
      try:
        stdout = ''
        filename = os.path.join(self.project_path, p.filename)
        if p.is_delete:
          os.remove(filename)
        else:
          dirname = os.path.dirname(p.filename)
          full_dir = os.path.join(self.project_path, dirname)
          if dirname and not os.path.isdir(full_dir):
            os.makedirs(full_dir)

          filepath = os.path.join(self.project_path, p.filename)
          if p.is_binary:
            with open(filepath, 'wb') as f:
              f.write(p.get())
          else:
            if p.source_filename:
              if not p.is_new:
                raise PatchApplicationFailed(
                    p.filename,
                    'File has a source filename specified but is not new')
              # Copy the file first.
              if os.path.isfile(filepath):
                raise PatchApplicationFailed(
                    p.filename, 'File exist but was about to be overwriten')
              shutil.copy2(
                  os.path.join(self.project_path, p.source_filename), filepath)
            if p.diff_hunks:
              stdout = subprocess2.check_output(
                  ['patch', '-u', '--binary', '-p%s' % p.patchlevel],
                  stdin=p.get(False),
                  stderr=subprocess2.STDOUT,
                  cwd=self.project_path)
            elif p.is_new and not os.path.exists(filepath):
              # There is only a header. Just create the file.
              open(filepath, 'w').close()
        for post in post_processors:
          post(self, p)
      except OSError, e:
        raise PatchApplicationFailed(p.filename, '%s%s' % (stdout, e))
      except subprocess.CalledProcessError, e:
        raise PatchApplicationFailed(
            p.filename, '%s%s' % (stdout, getattr(e, 'stdout', None)))

  def commit(self, commit_message, user):
    """Stubbed out."""
    raise NotImplementedError('RawCheckout can\'t commit')


class SvnConfig(object):
  """Parses a svn configuration file."""
  def __init__(self, svn_config_dir=None):
    super(SvnConfig, self).__init__()
    self.svn_config_dir = svn_config_dir
    self.default = not bool(self.svn_config_dir)
    if not self.svn_config_dir:
      if sys.platform == 'win32':
        self.svn_config_dir = os.path.join(os.environ['APPDATA'], 'Subversion')
      else:
        self.svn_config_dir = os.path.join(os.environ['HOME'], '.subversion')
    svn_config_file = os.path.join(self.svn_config_dir, 'config')
    parser = ConfigParser.SafeConfigParser()
    if os.path.isfile(svn_config_file):
      parser.read(svn_config_file)
    else:
      parser.add_section('auto-props')
    self.auto_props = dict(parser.items('auto-props'))


class SvnMixIn(object):
  """MixIn class to add svn commands common to both svn and git-svn clients."""
  # These members need to be set by the subclass.
  commit_user = None
  commit_pwd = None
  svn_url = None
  project_path = None
  # Override at class level when necessary. If used, --non-interactive is
  # implied.
  svn_config = SvnConfig()
  # Set to True when non-interactivity is necessary but a custom subversion
  # configuration directory is not necessary.
  non_interactive = False

  def _add_svn_flags(self, args, non_interactive, credentials=True):
    args = ['svn'] + args
    if not self.svn_config.default:
      args.extend(['--config-dir', self.svn_config.svn_config_dir])
    if not self.svn_config.default or self.non_interactive or non_interactive:
      args.append('--non-interactive')
    if credentials:
      if self.commit_user:
        args.extend(['--username', self.commit_user])
      if self.commit_pwd:
        args.extend(['--password', self.commit_pwd])
    return args

  def _check_call_svn(self, args, **kwargs):
    """Runs svn and throws an exception if the command failed."""
    kwargs.setdefault('cwd', self.project_path)
    kwargs.setdefault('stdout', self.VOID)
    return subprocess2.check_call_out(
        self._add_svn_flags(args, False), **kwargs)

  def _check_output_svn(self, args, credentials=True, **kwargs):
    """Runs svn and throws an exception if the command failed.

     Returns the output.
    """
    kwargs.setdefault('cwd', self.project_path)
    return subprocess2.check_output(
        self._add_svn_flags(args, True, credentials),
        stderr=subprocess2.STDOUT,
        **kwargs)

  @staticmethod
  def _parse_svn_info(output, key):
    """Returns value for key from svn info output.

    Case insensitive.
    """
    values = {}
    key = key.lower()
    for line in output.splitlines(False):
      if not line:
        continue
      k, v = line.split(':', 1)
      k = k.strip().lower()
      v = v.strip()
      assert not k in values
      values[k] = v
    return values.get(key, None)


class SvnCheckout(CheckoutBase, SvnMixIn):
  """Manages a subversion checkout."""
  def __init__(self, root_dir, project_name, commit_user, commit_pwd, svn_url,
      post_processors=None):
    CheckoutBase.__init__(self, root_dir, project_name, post_processors)
    SvnMixIn.__init__(self)
    self.commit_user = commit_user
    self.commit_pwd = commit_pwd
    self.svn_url = svn_url
    assert bool(self.commit_user) >= bool(self.commit_pwd)

  def prepare(self, revision):
    # Will checkout if the directory is not present.
    assert self.svn_url
    if not os.path.isdir(self.project_path):
      logging.info('Checking out %s in %s' %
          (self.project_name, self.project_path))
    return self._revert(revision)

  def apply_patch(self, patches, post_processors=None):
    post_processors = post_processors or self.post_processors or []
    for p in patches:
      try:
        # It is important to use credentials=False otherwise credentials could
        # leak in the error message. Credentials are not necessary here for the
        # following commands anyway.
        stdout = ''
        if p.is_delete:
          stdout += self._check_output_svn(
              ['delete', p.filename, '--force'], credentials=False)
        else:
          # svn add while creating directories otherwise svn add on the
          # contained files will silently fail.
          # First, find the root directory that exists.
          dirname = os.path.dirname(p.filename)
          dirs_to_create = []
          while (dirname and
              not os.path.isdir(os.path.join(self.project_path, dirname))):
            dirs_to_create.append(dirname)
            dirname = os.path.dirname(dirname)
          for dir_to_create in reversed(dirs_to_create):
            os.mkdir(os.path.join(self.project_path, dir_to_create))
            stdout += self._check_output_svn(
                ['add', dir_to_create, '--force'], credentials=False)

          filepath = os.path.join(self.project_path, p.filename)
          if p.is_binary:
            with open(filepath, 'wb') as f:
              f.write(p.get())
          else:
            if p.source_filename:
              if not p.is_new:
                raise PatchApplicationFailed(
                    p.filename,
                    'File has a source filename specified but is not new')
              # Copy the file first.
              if os.path.isfile(filepath):
                raise PatchApplicationFailed(
                    p.filename, 'File exist but was about to be overwriten')
              shutil.copy2(
                  os.path.join(self.project_path, p.source_filename), filepath)
            if p.diff_hunks:
              cmd = ['patch', '-p%s' % p.patchlevel, '--forward', '--force']
              stdout += subprocess2.check_output(
                  cmd, stdin=p.get(False), cwd=self.project_path)
            elif p.is_new and not os.path.exists(filepath):
              # There is only a header. Just create the file if it doesn't
              # exist.
              open(filepath, 'w').close()
          if p.is_new:
            stdout += self._check_output_svn(
                ['add', p.filename, '--force'], credentials=False)
          for prop in p.svn_properties:
            stdout += self._check_output_svn(
                ['propset', prop[0], prop[1], p.filename], credentials=False)
          for prop, values in self.svn_config.auto_props.iteritems():
            if fnmatch.fnmatch(p.filename, prop):
              for value in values.split(';'):
                if '=' not in value:
                  params = [value, '*']
                else:
                  params = value.split('=', 1)
                stdout += self._check_output_svn(
                    ['propset'] + params + [p.filename], credentials=False)
        for post in post_processors:
          post(self, p)
      except OSError, e:
        raise PatchApplicationFailed(p.filename, '%s%s' % (stdout, e))
      except subprocess.CalledProcessError, e:
        raise PatchApplicationFailed(
            p.filename,
            'While running %s;\n%s%s' % (
              ' '.join(e.cmd), stdout, getattr(e, 'stdout', '')))

  def commit(self, commit_message, user):
    logging.info('Committing patch for %s' % user)
    assert self.commit_user
    assert isinstance(commit_message, unicode)
    handle, commit_filename = tempfile.mkstemp(text=True)
    try:
      # Shouldn't assume default encoding is UTF-8. But really, if you are using
      # anything else, you are living in another world.
      os.write(handle, commit_message.encode('utf-8'))
      os.close(handle)
      # When committing, svn won't update the Revision metadata of the checkout,
      # so if svn commit returns "Committed revision 3.", svn info will still
      # return "Revision: 2". Since running svn update right after svn commit
      # creates a race condition with other committers, this code _must_ parse
      # the output of svn commit and use a regexp to grab the revision number.
      # Note that "Committed revision N." is localized but subprocess2 forces
      # LANGUAGE=en.
      args = ['commit', '--file', commit_filename]
      # realauthor is parsed by a server-side hook.
      if user and user != self.commit_user:
        args.extend(['--with-revprop', 'realauthor=%s' % user])
      out = self._check_output_svn(args)
    finally:
      os.remove(commit_filename)
    lines = filter(None, out.splitlines())
    match = re.match(r'^Committed revision (\d+).$', lines[-1])
    if not match:
      raise PatchApplicationFailed(
          None,
          'Couldn\'t make sense out of svn commit message:\n' + out)
    return int(match.group(1))

  def _revert(self, revision):
    """Reverts local modifications or checks out if the directory is not
    present. Use depot_tools's functionality to do this.
    """
    flags = ['--ignore-externals']
    if revision:
      flags.extend(['--revision', str(revision)])
    if not os.path.isdir(self.project_path):
      logging.info(
          'Directory %s is not present, checking it out.' % self.project_path)
      self._check_call_svn(
          ['checkout', self.svn_url, self.project_path] + flags, cwd=None)
    else:
      scm.SVN.Revert(self.project_path, no_ignore=True)
      # Revive files that were deleted in scm.SVN.Revert().
      self._check_call_svn(['update', '--force'] + flags)
    return self._get_revision()

  def _get_revision(self):
    out = self._check_output_svn(['info', '.'])
    revision = int(self._parse_svn_info(out, 'revision'))
    if revision != self._last_seen_revision:
      logging.info('Updated to revision %d' % revision)
      self._last_seen_revision = revision
    return revision


class GitCheckoutBase(CheckoutBase):
  """Base class for git checkout. Not to be used as-is."""
  def __init__(self, root_dir, project_name, remote_branch,
      post_processors=None):
    super(GitCheckoutBase, self).__init__(
        root_dir, project_name, post_processors)
    # There is no reason to not hardcode it.
    self.remote = 'origin'
    self.remote_branch = remote_branch
    self.working_branch = 'working_branch'

  def prepare(self, revision):
    """Resets the git repository in a clean state.

    Checks it out if not present and deletes the working branch.
    """
    assert self.remote_branch
    assert os.path.isdir(self.project_path)
    self._check_call_git(['reset', '--hard', '--quiet'])
    if revision:
      try:
        revision = self._check_output_git(['rev-parse', revision])
      except subprocess.CalledProcessError:
        self._check_call_git(
            ['fetch', self.remote, self.remote_branch, '--quiet'])
        revision = self._check_output_git(['rev-parse', revision])
      self._check_call_git(['checkout', '--force', '--quiet', revision])
    else:
      branches, active = self._branches()
      if active != 'master':
        self._check_call_git(['checkout', '--force', '--quiet', 'master'])
      self._check_call_git(['pull', self.remote, self.remote_branch, '--quiet'])
      if self.working_branch in branches:
        self._call_git(['branch', '-D', self.working_branch])

  def apply_patch(self, patches, post_processors=None):
    """Applies a patch on 'working_branch' and switch to it.

    Also commits the changes on the local branch.

    Ignores svn properties and raise an exception on unexpected ones.
    """
    post_processors = post_processors or self.post_processors or []
    # It this throws, the checkout is corrupted. Maybe worth deleting it and
    # trying again?
    if self.remote_branch:
      self._check_call_git(
          ['checkout', '-b', self.working_branch,
            '%s/%s' % (self.remote, self.remote_branch), '--quiet'])
    for index, p in enumerate(patches):
      try:
        stdout = ''
        if p.is_delete:
          if (not os.path.exists(p.filename) and
              any(p1.source_filename == p.filename for p1 in patches[0:index])):
            # The file could already be deleted if a prior patch with file
            # rename was already processed. To be sure, look at all the previous
            # patches to see if they were a file rename.
            pass
          else:
            stdout += self._check_output_git(['rm', p.filename])
        else:
          dirname = os.path.dirname(p.filename)
          full_dir = os.path.join(self.project_path, dirname)
          if dirname and not os.path.isdir(full_dir):
            os.makedirs(full_dir)
          if p.is_binary:
            with open(os.path.join(self.project_path, p.filename), 'wb') as f:
              f.write(p.get())
            stdout += self._check_output_git(['add', p.filename])
          else:
            # No need to do anything special with p.is_new or if not
            # p.diff_hunks. git apply manages all that already.
            stdout += self._check_output_git(
                ['apply', '--index', '-p%s' % p.patchlevel], stdin=p.get(True))
          for prop in p.svn_properties:
            # Ignore some known auto-props flags through .subversion/config,
            # bails out on the other ones.
            # TODO(maruel): Read ~/.subversion/config and detect the rules that
            # applies here to figure out if the property will be correctly
            # handled.
            if not prop[0] in (
                'svn:eol-style', 'svn:executable', 'svn:mime-type'):
              raise patch.UnsupportedPatchFormat(
                  p.filename,
                  'Cannot apply svn property %s to file %s.' % (
                        prop[0], p.filename))
        for post in post_processors:
          post(self, p)
      except OSError, e:
        raise PatchApplicationFailed(p.filename, '%s%s' % (stdout, e))
      except subprocess.CalledProcessError, e:
        raise PatchApplicationFailed(
            p.filename, '%s%s' % (stdout, getattr(e, 'stdout', None)))
    # Once all the patches are processed and added to the index, commit the
    # index.
    self._check_call_git(['commit', '-m', 'Committed patch'])
    # TODO(maruel): Weirdly enough they don't match, need to investigate.
    #found_files = self._check_output_git(
    #    ['diff', 'master', '--name-only']).splitlines(False)
    #assert sorted(patches.filenames) == sorted(found_files), (
    #    sorted(out), sorted(found_files))

  def commit(self, commit_message, user):
    """Updates the commit message.

    Subclass needs to dcommit or push.
    """
    assert isinstance(commit_message, unicode)
    self._check_call_git(['commit', '--amend', '-m', commit_message])
    return self._check_output_git(['rev-parse', 'HEAD']).strip()

  def _check_call_git(self, args, **kwargs):
    kwargs.setdefault('cwd', self.project_path)
    kwargs.setdefault('stdout', self.VOID)
    return subprocess2.check_call_out(['git'] + args, **kwargs)

  def _call_git(self, args, **kwargs):
    """Like check_call but doesn't throw on failure."""
    kwargs.setdefault('cwd', self.project_path)
    kwargs.setdefault('stdout', self.VOID)
    return subprocess2.call(['git'] + args, **kwargs)

  def _check_output_git(self, args, **kwargs):
    kwargs.setdefault('cwd', self.project_path)
    return subprocess2.check_output(
        ['git'] + args, stderr=subprocess2.STDOUT, **kwargs)

  def _branches(self):
    """Returns the list of branches and the active one."""
    out = self._check_output_git(['branch']).splitlines(False)
    branches = [l[2:] for l in out]
    active = None
    for l in out:
      if l.startswith('*'):
        active = l[2:]
        break
    return branches, active


class GitSvnCheckoutBase(GitCheckoutBase, SvnMixIn):
  """Base class for git-svn checkout. Not to be used as-is."""
  def __init__(self,
      root_dir, project_name, remote_branch,
      commit_user, commit_pwd,
      svn_url, trunk, post_processors=None):
    """trunk is optional."""
    GitCheckoutBase.__init__(
        self, root_dir, project_name + '.git', remote_branch, post_processors)
    SvnMixIn.__init__(self)
    self.commit_user = commit_user
    self.commit_pwd = commit_pwd
    # svn_url in this case is the root of the svn repository.
    self.svn_url = svn_url
    self.trunk = trunk
    assert bool(self.commit_user) >= bool(self.commit_pwd)
    assert self.svn_url
    assert self.trunk
    self._cache_svn_auth()

  def prepare(self, revision):
    """Resets the git repository in a clean state."""
    self._check_call_git(['reset', '--hard', '--quiet'])
    if revision:
      try:
        revision = self._check_output_git(
            ['svn', 'find-rev', 'r%d' % revision])
      except subprocess.CalledProcessError:
        self._check_call_git(
            ['fetch', self.remote, self.remote_branch, '--quiet'])
        revision = self._check_output_git(
            ['svn', 'find-rev', 'r%d' % revision])
      super(GitSvnCheckoutBase, self).prepare(revision)
    else:
      branches, active = self._branches()
      if active != 'master':
        if not 'master' in branches:
          self._check_call_git(
              ['checkout', '--quiet', '-b', 'master',
              '%s/%s' % (self.remote, self.remote_branch)])
        else:
          self._check_call_git(['checkout', 'master', '--force', '--quiet'])
      # git svn rebase --quiet --quiet doesn't work, use two steps to silence
      # it.
      self._check_call_git_svn(['fetch', '--quiet', '--quiet'])
      self._check_call_git(
          ['rebase', '--quiet', '--quiet',
            '%s/%s' % (self.remote, self.remote_branch)])
      if self.working_branch in branches:
        self._call_git(['branch', '-D', self.working_branch])
    return self._get_revision()

  def _git_svn_info(self, key):
    """Calls git svn info. This doesn't support nor need --config-dir."""
    return self._parse_svn_info(self._check_output_git(['svn', 'info']), key)

  def commit(self, commit_message, user):
    """Commits a patch."""
    logging.info('Committing patch for %s' % user)
    # Fix the commit message and author. It returns the git hash, which we
    # ignore unless it's None.
    if not super(GitSvnCheckoutBase, self).commit(commit_message, user):
      return None
    # TODO(maruel): git-svn ignores --config-dir as of git-svn version 1.7.4 and
    # doesn't support --with-revprop.
    # Either learn perl and upstream or suck it.
    kwargs = {}
    if self.commit_pwd:
      kwargs['stdin'] = self.commit_pwd + '\n'
      kwargs['stderr'] = subprocess2.STDOUT
    self._check_call_git_svn(
        ['dcommit', '--rmdir', '--find-copies-harder',
          '--username', self.commit_user],
        **kwargs)
    revision = int(self._git_svn_info('revision'))
    return revision

  def _cache_svn_auth(self):
    """Caches the svn credentials. It is necessary since git-svn doesn't prompt
    for it."""
    if not self.commit_user or not self.commit_pwd:
      return
    # Use capture to lower noise in logs.
    self._check_output_svn(['ls', self.svn_url], cwd=None)

  def _check_call_git_svn(self, args, **kwargs):
    """Handles svn authentication while calling git svn."""
    args = ['svn'] + args
    if not self.svn_config.default:
      args.extend(['--config-dir', self.svn_config.svn_config_dir])
    return self._check_call_git(args, **kwargs)

  def _get_revision(self):
    revision = int(self._git_svn_info('revision'))
    if revision != self._last_seen_revision:
      logging.info('Updated to revision %d' % revision)
      self._last_seen_revision = revision
    return revision


class GitSvnPremadeCheckout(GitSvnCheckoutBase):
  """Manages a git-svn clone made out from an initial git-svn seed.

  This class is very similar to GitSvnCheckout but is faster to bootstrap
  because it starts right off with an existing git-svn clone.
  """
  def __init__(self,
      root_dir, project_name, remote_branch,
      commit_user, commit_pwd,
      svn_url, trunk, git_url, post_processors=None):
    super(GitSvnPremadeCheckout, self).__init__(
        root_dir, project_name, remote_branch,
        commit_user, commit_pwd,
        svn_url, trunk, post_processors)
    self.git_url = git_url
    assert self.git_url

  def prepare(self, revision):
    """Creates the initial checkout for the repo."""
    if not os.path.isdir(self.project_path):
      logging.info('Checking out %s in %s' %
          (self.project_name, self.project_path))
      assert self.remote == 'origin'
      # self.project_path doesn't exist yet.
      self._check_call_git(
          ['clone', self.git_url, self.project_name, '--quiet'],
          cwd=self.root_dir,
          stderr=subprocess2.STDOUT)
    try:
      configured_svn_url = self._check_output_git(
          ['config', 'svn-remote.svn.url']).strip()
    except subprocess.CalledProcessError:
      configured_svn_url = ''

    if configured_svn_url.strip() != self.svn_url:
      self._check_call_git_svn(
          ['init',
           '--prefix', self.remote + '/',
           '-T', self.trunk,
           self.svn_url])
    self._check_call_git_svn(['fetch'])
    return super(GitSvnPremadeCheckout, self).prepare(revision)


class GitSvnCheckout(GitSvnCheckoutBase):
  """Manages a git-svn clone.

  Using git-svn hides some of the complexity of using a svn checkout.
  """
  def __init__(self,
      root_dir, project_name,
      commit_user, commit_pwd,
      svn_url, trunk, post_processors=None):
    super(GitSvnCheckout, self).__init__(
        root_dir, project_name, 'trunk',
        commit_user, commit_pwd,
        svn_url, trunk, post_processors)

  def prepare(self, revision):
    """Creates the initial checkout for the repo."""
    assert not revision, 'Implement revision if necessary'
    if not os.path.isdir(self.project_path):
      logging.info('Checking out %s in %s' %
          (self.project_name, self.project_path))
      # TODO: Create a shallow clone.
      # self.project_path doesn't exist yet.
      self._check_call_git_svn(
          ['clone',
           '--prefix', self.remote + '/',
           '-T', self.trunk,
           self.svn_url, self.project_path,
           '--quiet'],
          cwd=self.root_dir,
          stderr=subprocess2.STDOUT)
    return super(GitSvnCheckout, self).prepare(revision)


class ReadOnlyCheckout(object):
  """Converts a checkout into a read-only one."""
  def __init__(self, checkout, post_processors=None):
    super(ReadOnlyCheckout, self).__init__()
    self.checkout = checkout
    self.post_processors = (post_processors or []) + (
        self.checkout.post_processors or [])

  def prepare(self, revision):
    return self.checkout.prepare(revision)

  def get_settings(self, key):
    return self.checkout.get_settings(key)

  def apply_patch(self, patches, post_processors=None):
    return self.checkout.apply_patch(
        patches, post_processors or self.post_processors)

  def commit(self, message, user):  # pylint: disable=R0201
    logging.info('Would have committed for %s with message: %s' % (
        user, message))
    return 'FAKE'

  @property
  def project_name(self):
    return self.checkout.project_name

  @property
  def project_path(self):
    return self.checkout.project_path
