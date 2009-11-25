# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Gclient-specific SCM-specific operations."""

import logging
import os
import re
import subprocess

import scm
import gclient_utils


### SCM abstraction layer

# Factory Method for SCM wrapper creation

def CreateSCM(url=None, root_dir=None, relpath=None, scm_name='svn'):
  # TODO(maruel): Deduce the SCM from the url.
  scm_map = {
    'svn' : SVNWrapper,
    'git' : GitWrapper,
  }

  orig_url = url

  if url:
    url, _ = gclient_utils.SplitUrlRevision(url)
    if url.startswith('git:') or url.startswith('ssh:') or url.endswith('.git'):
      scm_name = 'git'

  if not scm_name in scm_map:
    raise gclient_utils.Error('Unsupported scm %s' % scm_name)
  return scm_map[scm_name](orig_url, root_dir, relpath, scm_name)


# SCMWrapper base class

class SCMWrapper(object):
  """Add necessary glue between all the supported SCM.

  This is the abstraction layer to bind to different SCM. Since currently only
  subversion is supported, a lot of subersionism remains. This can be sorted out
  once another SCM is supported."""
  def __init__(self, url=None, root_dir=None, relpath=None,
               scm_name='svn'):
    self.scm_name = scm_name
    self.url = url
    self._root_dir = root_dir
    if self._root_dir:
      self._root_dir = self._root_dir.replace('/', os.sep)
    self.relpath = relpath
    if self.relpath:
      self.relpath = self.relpath.replace('/', os.sep)
    if self.relpath and self._root_dir:
      self.checkout_path = os.path.join(self._root_dir, self.relpath)

  def FullUrlForRelativeUrl(self, url):
    # Find the forth '/' and strip from there. A bit hackish.
    return '/'.join(self.url.split('/')[:4]) + url

  def RunCommand(self, command, options, args, file_list=None):
    # file_list will have all files that are modified appended to it.
    if file_list is None:
      file_list = []

    commands = ['cleanup', 'export', 'update', 'revert', 'revinfo',
                'status', 'diff', 'pack', 'runhooks']

    if not command in commands:
      raise gclient_utils.Error('Unknown command %s' % command)

    if not command in dir(self):
      raise gclient_utils.Error('Command %s not implemnted in %s wrapper' % (
          command, self.scm_name))

    return getattr(self, command)(options, args, file_list)


class GitWrapper(SCMWrapper, scm.GIT):
  """Wrapper for Git"""

  def cleanup(self, options, args, file_list):
    """Cleanup working copy."""
    __pychecker__ = 'unusednames=args,file_list,options'
    self._Run(['prune'], redirect_stdout=False)
    self._Run(['fsck'], redirect_stdout=False)
    self._Run(['gc'], redirect_stdout=False)

  def diff(self, options, args, file_list):
    __pychecker__ = 'unusednames=args,file_list,options'
    merge_base = self._Run(['merge-base', 'HEAD', 'origin'])
    self._Run(['diff', merge_base], redirect_stdout=False)

  def export(self, options, args, file_list):
    __pychecker__ = 'unusednames=file_list,options'
    assert len(args) == 1
    export_path = os.path.abspath(os.path.join(args[0], self.relpath))
    if not os.path.exists(export_path):
      os.makedirs(export_path)
    self._Run(['checkout-index', '-a', '--prefix=%s/' % export_path],
              redirect_stdout=False)

  def update(self, options, args, file_list):
    """Runs git to update or transparently checkout the working copy.

    All updated files will be appended to file_list.

    Raises:
      Error: if can't get URL for relative path.
    """

    if args:
      raise gclient_utils.Error("Unsupported argument(s): %s" % ",".join(args))

    url, revision = gclient_utils.SplitUrlRevision(self.url)
    rev_str = ""
    if options.revision:
      # Override the revision number.
      revision = str(options.revision)
    if revision:
      rev_str = ' at %s' % revision

    if options.verbose:
      print("\n_____ %s%s" % (self.relpath, rev_str))

    if not os.path.exists(self.checkout_path):
      self._Run(['clone', url, self.checkout_path],
                cwd=self._root_dir, redirect_stdout=False)
      if revision:
        self._Run(['reset', '--hard', revision], redirect_stdout=False)
      files = self._Run(['ls-files']).split()
      file_list.extend([os.path.join(self.checkout_path, f) for f in files])
      return

    new_base = 'origin'
    if revision:
      new_base = revision
    cur_branch = self._Run(['symbolic-ref', 'HEAD']).split('/')[-1]
    merge_base = self._Run(['merge-base', 'HEAD', new_base])
    self._Run(['remote', 'update'], redirect_stdout=False)
    files = self._Run(['diff', new_base, '--name-only']).split()
    file_list.extend([os.path.join(self.checkout_path, f) for f in files])
    self._Run(['rebase', '-v', '--onto', new_base, merge_base, cur_branch],
                redirect_stdout=False)
    print "Checked out revision %s." % self.revinfo(options, (), None)

  def revert(self, options, args, file_list):
    """Reverts local modifications.

    All reverted files will be appended to file_list.
    """
    __pychecker__ = 'unusednames=args'
    path = os.path.join(self._root_dir, self.relpath)
    if not os.path.isdir(path):
      # revert won't work if the directory doesn't exist. It needs to
      # checkout instead.
      print("\n_____ %s is missing, synching instead" % self.relpath)
      # Don't reuse the args.
      return self.update(options, [], file_list)
    merge_base = self._Run(['merge-base', 'HEAD', 'origin'])
    files = self._Run(['diff', merge_base, '--name-only']).split()
    self._Run(['reset', '--hard', merge_base], redirect_stdout=False)
    file_list.extend([os.path.join(self.checkout_path, f) for f in files])

  def revinfo(self, options, args, file_list):
    """Display revision"""
    __pychecker__ = 'unusednames=args,file_list,options'
    return self._Run(['rev-parse', 'HEAD'])

  def runhooks(self, options, args, file_list):
    self.status(options, args, file_list)

  def status(self, options, args, file_list):
    """Display status information."""
    __pychecker__ = 'unusednames=args,options'
    if not os.path.isdir(self.checkout_path):
      print('\n________ couldn\'t run status in %s:\nThe directory '
            'does not exist.' % self.checkout_path)
    else:
      merge_base = self._Run(['merge-base', 'HEAD', 'origin'])
      self._Run(['diff', '--name-status', merge_base], redirect_stdout=False)
      files = self._Run(['diff', '--name-only', merge_base]).split()
      file_list.extend([os.path.join(self.checkout_path, f) for f in files])

  def _Run(self, args, cwd=None, checkrc=True, redirect_stdout=True):
    # TODO(maruel): Merge with Capture?
    stdout=None
    if redirect_stdout:
      stdout=subprocess.PIPE
    if cwd == None:
      cwd = self.checkout_path
    cmd = [self.COMMAND]
    cmd.extend(args)
    sp = subprocess.Popen(cmd, cwd=cwd, stdout=stdout)
    if checkrc and sp.returncode:
      raise gclient_utils.Error('git command %s returned %d' %
                                (args[0], sp.returncode))
    output = sp.communicate()[0]
    if output is not None:
      return output.strip()


class SVNWrapper(SCMWrapper, scm.SVN):
  """ Wrapper for SVN """

  def cleanup(self, options, args, file_list):
    """Cleanup working copy."""
    __pychecker__ = 'unusednames=file_list,options'
    command = ['cleanup']
    command.extend(args)
    self.Run(command, os.path.join(self._root_dir, self.relpath))

  def diff(self, options, args, file_list):
    # NOTE: This function does not currently modify file_list.
    __pychecker__ = 'unusednames=file_list,options'
    command = ['diff']
    command.extend(args)
    self.Run(command, os.path.join(self._root_dir, self.relpath))

  def export(self, options, args, file_list):
    __pychecker__ = 'unusednames=file_list,options'
    assert len(args) == 1
    export_path = os.path.abspath(os.path.join(args[0], self.relpath))
    try:
      os.makedirs(export_path)
    except OSError:
      pass
    assert os.path.exists(export_path)
    command = ['export', '--force', '.']
    command.append(export_path)
    self.Run(command, os.path.join(self._root_dir, self.relpath))

  def update(self, options, args, file_list):
    """Runs SCM to update or transparently checkout the working copy.

    All updated files will be appended to file_list.

    Raises:
      Error: if can't get URL for relative path.
    """
    # Only update if git is not controlling the directory.
    checkout_path = os.path.join(self._root_dir, self.relpath)
    git_path = os.path.join(self._root_dir, self.relpath, '.git')
    if os.path.exists(git_path):
      print("________ found .git directory; skipping %s" % self.relpath)
      return

    if args:
      raise gclient_utils.Error("Unsupported argument(s): %s" % ",".join(args))

    url, revision = gclient_utils.SplitUrlRevision(self.url)
    base_url = url
    forced_revision = False
    rev_str = ""
    if options.revision:
      # Override the revision number.
      revision = str(options.revision)
    if revision:
      forced_revision = True
      url = '%s@%s' % (url, revision)
      rev_str = ' at %s' % revision

    if not os.path.exists(checkout_path):
      # We need to checkout.
      command = ['checkout', url, checkout_path]
      if revision:
        command.extend(['--revision', str(revision)])
      self.RunAndGetFileList(options, command, self._root_dir, file_list)
      return

    # Get the existing scm url and the revision number of the current checkout.
    from_info = self.CaptureInfo(os.path.join(checkout_path, '.'), '.')
    if not from_info:
      raise gclient_utils.Error("Can't update/checkout %r if an unversioned "
                                "directory is present. Delete the directory "
                                "and try again." %
                                checkout_path)

    if options.manually_grab_svn_rev:
      # Retrieve the current HEAD version because svn is slow at null updates.
      if not revision:
        from_info_live = self.CaptureInfo(from_info['URL'], '.')
        revision = str(from_info_live['Revision'])
        rev_str = ' at %s' % revision

    if from_info['URL'] != base_url:
      to_info = self.CaptureInfo(url, '.')
      if not to_info.get('Repository Root') or not to_info.get('UUID'):
        # The url is invalid or the server is not accessible, it's safer to bail
        # out right now.
        raise gclient_utils.Error('This url is unreachable: %s' % url)
      can_switch = ((from_info['Repository Root'] != to_info['Repository Root'])
                    and (from_info['UUID'] == to_info['UUID']))
      if can_switch:
        print("\n_____ relocating %s to a new checkout" % self.relpath)
        # We have different roots, so check if we can switch --relocate.
        # Subversion only permits this if the repository UUIDs match.
        # Perform the switch --relocate, then rewrite the from_url
        # to reflect where we "are now."  (This is the same way that
        # Subversion itself handles the metadata when switch --relocate
        # is used.)  This makes the checks below for whether we
        # can update to a revision or have to switch to a different
        # branch work as expected.
        # TODO(maruel):  TEST ME !
        command = ["switch", "--relocate",
                   from_info['Repository Root'],
                   to_info['Repository Root'],
                   self.relpath]
        self.Run(command, self._root_dir)
        from_info['URL'] = from_info['URL'].replace(
            from_info['Repository Root'],
            to_info['Repository Root'])
      else:
        if self.CaptureStatus(checkout_path):
          raise gclient_utils.Error("Can't switch the checkout to %s; UUID "
                                    "don't match and there is local changes "
                                    "in %s. Delete the directory and "
                                    "try again." % (url, checkout_path))
        # Ok delete it.
        print("\n_____ switching %s to a new checkout" % self.relpath)
        gclient_utils.RemoveDirectory(checkout_path)
        # We need to checkout.
        command = ['checkout', url, checkout_path]
        if revision:
          command.extend(['--revision', str(revision)])
        self.RunAndGetFileList(options, command, self._root_dir, file_list)
        return


    # If the provided url has a revision number that matches the revision
    # number of the existing directory, then we don't need to bother updating.
    if not options.force and str(from_info['Revision']) == revision:
      if options.verbose or not forced_revision:
        print("\n_____ %s%s" % (self.relpath, rev_str))
      return

    command = ["update", checkout_path]
    if revision:
      command.extend(['--revision', str(revision)])
    self.RunAndGetFileList(options, command, self._root_dir, file_list)

  def revert(self, options, args, file_list):
    """Reverts local modifications. Subversion specific.

    All reverted files will be appended to file_list, even if Subversion
    doesn't know about them.
    """
    __pychecker__ = 'unusednames=args'
    path = os.path.join(self._root_dir, self.relpath)
    if not os.path.isdir(path):
      # svn revert won't work if the directory doesn't exist. It needs to
      # checkout instead.
      print("\n_____ %s is missing, synching instead" % self.relpath)
      # Don't reuse the args.
      return self.update(options, [], file_list)

    for file_status in self.CaptureStatus(path):
      file_path = os.path.join(path, file_status[1])
      if file_status[0][0] == 'X':
        # Ignore externals.
        logging.info('Ignoring external %s' % file_path)
        continue

      if logging.getLogger().isEnabledFor(logging.INFO):
        logging.info('%s%s' % (file[0], file[1]))
      else:
        print(file_path)
      if file_status[0].isspace():
        logging.error('No idea what is the status of %s.\n'
                      'You just found a bug in gclient, please ping '
                      'maruel@chromium.org ASAP!' % file_path)
      # svn revert is really stupid. It fails on inconsistent line-endings,
      # on switched directories, etc. So take no chance and delete everything!
      try:
        if not os.path.exists(file_path):
          pass
        elif os.path.isfile(file_path):
          logging.info('os.remove(%s)' % file_path)
          os.remove(file_path)
        elif os.path.isdir(file_path):
          logging.info('gclient_utils.RemoveDirectory(%s)' % file_path)
          gclient_utils.RemoveDirectory(file_path)
        else:
          logging.error('no idea what is %s.\nYou just found a bug in gclient'
                        ', please ping maruel@chromium.org ASAP!' % file_path)
      except EnvironmentError:
        logging.error('Failed to remove %s.' % file_path)

    try:
      # svn revert is so broken we don't even use it. Using
      # "svn up --revision BASE" achieve the same effect.
      self.RunAndGetFileList(options, ['update', '--revision', 'BASE'], path,
                           file_list)
    except OSError, e:
      # Maybe the directory disapeared meanwhile. We don't want it to throw an
      # exception.
      logging.error('Failed to update:\n%s' % str(e))

  def revinfo(self, options, args, file_list):
    """Display revision"""
    __pychecker__ = 'unusednames=args,file_list,options'
    return self.CaptureHeadRevision(self.url)

  def runhooks(self, options, args, file_list):
    self.status(options, args, file_list)

  def status(self, options, args, file_list):
    """Display status information."""
    path = os.path.join(self._root_dir, self.relpath)
    command = ['status']
    command.extend(args)
    if not os.path.isdir(path):
      # svn status won't work if the directory doesn't exist.
      print("\n________ couldn't run \'%s\' in \'%s\':\nThe directory "
            "does not exist."
            % (' '.join(command), path))
      # There's no file list to retrieve.
    else:
      self.RunAndGetFileList(options, command, path, file_list)

  def pack(self, options, args, file_list):
    """Generates a patch file which can be applied to the root of the
    repository."""
    __pychecker__ = 'unusednames=file_list,options'
    path = os.path.join(self._root_dir, self.relpath)
    command = ['diff']
    command.extend(args)
    # Simple class which tracks which file is being diffed and
    # replaces instances of its file name in the original and
    # working copy lines of the svn diff output.
    class DiffFilterer(object):
      index_string = "Index: "
      original_prefix = "--- "
      working_prefix = "+++ "

      def __init__(self, relpath):
        # Note that we always use '/' as the path separator to be
        # consistent with svn's cygwin-style output on Windows
        self._relpath = relpath.replace("\\", "/")
        self._current_file = ""
        self._replacement_file = ""

      def SetCurrentFile(self, file):
        self._current_file = file
        # Note that we always use '/' as the path separator to be
        # consistent with svn's cygwin-style output on Windows
        self._replacement_file = self._relpath + '/' + file

      def ReplaceAndPrint(self, line):
        print(line.replace(self._current_file, self._replacement_file))

      def Filter(self, line):
        if (line.startswith(self.index_string)):
          self.SetCurrentFile(line[len(self.index_string):])
          self.ReplaceAndPrint(line)
        else:
          if (line.startswith(self.original_prefix) or
              line.startswith(self.working_prefix)):
            self.ReplaceAndPrint(line)
          else:
            print line

    filterer = DiffFilterer(self.relpath)
    self.RunAndFilterOutput(command, path, False, False, filterer.Filter)
