#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A git command for managing a local cache of git repositories."""

import errno
import logging
import optparse
import os
import tempfile
import subprocess
import sys
import urlparse

import gclient_utils
import subcommand


GIT_EXECUTABLE = 'git.bat' if sys.platform.startswith('win') else 'git'


def NormalizeUrl(url):
  """Convert a git url to a normalized form."""
  parsed = urlparse.urlparse(url)
  norm_url = 'https://' + parsed.netloc + parsed.path
  if not norm_url.endswith('.git'):
    norm_url += '.git'
  return norm_url


def UrlToCacheDir(url):
  """Convert a git url to a normalized form for the cache dir path."""
  parsed = urlparse.urlparse(url)
  norm_url = parsed.netloc + parsed.path
  if norm_url.endswith('.git'):
    norm_url = norm_url[:-len('.git')]
  return norm_url.replace('-', '--').replace('/', '-').lower()


def RunGit(cmd, **kwargs):
  """Run git in a subprocess."""
  kwargs.setdefault('cwd', os.getcwd())
  if kwargs.get('filter_fn'):
    kwargs['filter_fn'] = gclient_utils.GitFilter(kwargs.get('filter_fn'))
    kwargs.setdefault('print_stdout', False)
    env = kwargs.get('env') or kwargs.setdefault('env', os.environ.copy())
    env.setdefault('GIT_ASKPASS', 'true')
    env.setdefault('SSH_ASKPASS', 'true')
  else:
    kwargs.setdefault('print_stdout', True)
  stdout = kwargs.get('stdout', sys.stdout)
  print >> stdout, 'running "git %s" in "%s"' % (' '.join(cmd), kwargs['cwd'])
  gclient_utils.CheckCallAndFilter([GIT_EXECUTABLE] + cmd, **kwargs)


class LockError(Exception):
  pass


class Lockfile(object):
  """Class to represent a cross-platform process-specific lockfile."""

  def __init__(self, path):
    self.path = os.path.abspath(path)
    self.lockfile = self.path + ".lock"
    self.pid = os.getpid()

  def _read_pid(self):
    """Read the pid stored in the lockfile.

    Note: This method is potentially racy. By the time it returns the lockfile
    may have been unlocked, removed, or stolen by some other process.
    """
    try:
      with open(self.lockfile, 'r') as f:
        pid = int(f.readline().strip())
    except (IOError, ValueError):
      pid = None
    return pid

  def _make_lockfile(self):
    """Safely creates a lockfile containing the current pid."""
    open_flags = (os.O_CREAT | os.O_EXCL | os.O_WRONLY)
    fd = os.open(self.lockfile, open_flags, 0o644)
    f = os.fdopen(fd, 'w')
    print >> f, self.pid
    f.close()

  def _remove_lockfile(self):
    """Delete the lockfile. Complains (implicitly) if it doesn't exist."""
    os.remove(self.lockfile)

  def lock(self):
    """Acquire the lock.

    Note: This is a NON-BLOCKING FAIL-FAST operation.
    Do. Or do not. There is no try.
    """
    try:
      self._make_lockfile()
    except OSError as e:
      if e.errno == errno.EEXIST:
        raise LockError("%s is already locked" % self.path)
      else:
        raise LockError("Failed to create %s (err %s)" % (self.path, e.errno))

  def unlock(self):
    """Release the lock."""
    if not self.is_locked():
      raise LockError("%s is not locked" % self.path)
    if not self.i_am_locking():
      raise LockError("%s is locked, but not by me" % self.path)
    self._remove_lockfile()

  def break_lock(self):
    """Remove the lock, even if it was created by someone else."""
    try:
      self._remove_lockfile()
      return True
    except OSError as exc:
      if exc.errno == errno.ENOENT:
        return False
      else:
        raise

  def is_locked(self):
    """Test if the file is locked by anyone.

    Note: This method is potentially racy. By the time it returns the lockfile
    may have been unlocked, removed, or stolen by some other process.
    """
    return os.path.exists(self.lockfile)

  def i_am_locking(self):
    """Test if the file is locked by this process."""
    return self.is_locked() and self.pid == self._read_pid()

  def __enter__(self):
    self.lock()
    return self

  def __exit__(self, *_exc):
    self.unlock()


@subcommand.usage('[url of repo to check for caching]')
def CMDexists(parser, args):
  """Check to see if there already is a cache of the given repo."""
  options, args = parser.parse_args(args)
  if not len(args) == 1:
    parser.error('git cache exists only takes exactly one repo url.')
  url = args[0]
  repo_dir = os.path.join(options.cache_dir, UrlToCacheDir(url))
  flag_file = os.path.join(repo_dir, 'config')
  if os.path.isdir(repo_dir) and os.path.isfile(flag_file):
    print repo_dir
    return 0
  return 1


@subcommand.usage('[url of repo to add to or update in cache]')
def CMDpopulate(parser, args):
  """Ensure that the cache has all up-to-date objects for the given repo."""
  parser.add_option('--depth', type='int',
                    help='Only cache DEPTH commits of history')
  parser.add_option('--shallow', '-s', action='store_true',
                    help='Only cache 10000 commits of history')
  parser.add_option('--ref', action='append',
                    help='Specify additional refs to be fetched')
  options, args = parser.parse_args(args)
  if options.shallow and not options.depth:
    options.depth = 10000
  if not len(args) == 1:
    parser.error('git cache populate only takes exactly one repo url.')
  url = args[0]

  gclient_utils.safe_makedirs(options.cache_dir)
  repo_dir = os.path.join(options.cache_dir, UrlToCacheDir(url))

  v = []
  filter_fn = lambda l: '[up to date]' not in l
  if options.verbose:
    v = ['-v', '--progress']
    filter_fn = None

  d = []
  if options.depth:
    d = ['--depth', '%d' % options.depth]

  def _config(directory):
    RunGit(['config', 'core.deltaBaseCacheLimit', '2g'],
           cwd=directory)
    RunGit(['config', 'remote.origin.url', NormalizeUrl(url)],
           cwd=directory)
    RunGit(['config', '--replace-all', 'remote.origin.fetch',
            '+refs/heads/*:refs/heads/*'],
           cwd=directory)
    RunGit(['config', '--add', 'remote.origin.fetch',
            '+refs/tags/*:refs/tags/*'],
           cwd=directory)
    for ref in options.ref or []:
      ref = ref.rstrip('/')
      refspec = '+refs/%s/*:refs/%s/*' % (ref, ref)
      RunGit(['config', '--add', 'remote.origin.fetch', refspec],
             cwd=directory)

  with Lockfile(repo_dir):
    # Setup from scratch if the repo is new or is in a bad state.
    if not os.path.exists(os.path.join(repo_dir, 'config')):
      gclient_utils.rmtree(repo_dir)
      tempdir = tempfile.mkdtemp(suffix=UrlToCacheDir(url),
                                 dir=options.cache_dir)
      RunGit(['init', '--bare'], cwd=tempdir)
      _config(tempdir)
      fetch_cmd = ['fetch'] + v + d + ['origin']
      RunGit(fetch_cmd, filter_fn=filter_fn, cwd=tempdir, retry=True)
      os.rename(tempdir, repo_dir)
    else:
      _config(repo_dir)
      if options.depth and os.path.exists(os.path.join(repo_dir, 'shallow')):
        logging.warn('Shallow fetch requested, but repo cache already exists.')
      fetch_cmd = ['fetch'] + v + ['origin']
      RunGit(fetch_cmd, filter_fn=filter_fn, cwd=repo_dir, retry=True)


@subcommand.usage('[url of repo to unlock, or -a|--all]')
def CMDunlock(parser, args):
  """Unlock one or all repos if their lock files are still around."""
  parser.add_option('--force', '-f', action='store_true',
                    help='Actually perform the action')
  parser.add_option('--all', '-a', action='store_true',
                    help='Unlock all repository caches')
  options, args = parser.parse_args(args)
  if len(args) > 1 or (len(args) == 0 and not options.all):
    parser.error('git cache unlock takes exactly one repo url, or --all')

  if not options.all:
    url = args[0]
    repo_dirs = [os.path.join(options.cache_dir, UrlToCacheDir(url))]
  else:
    repo_dirs = [os.path.join(options.cache_dir, path)
                 for path in os.listdir(options.cache_dir)
                 if os.path.isdir(os.path.join(options.cache_dir, path))]
    repo_dirs.extend([os.path.join(options.cache_dir,
                                   lockfile.replace('.lock', ''))
                      for lockfile in os.listdir(options.cache_dir)
                      if os.path.isfile(os.path.join(options.cache_dir,
                                                     lockfile))
                      and lockfile.endswith('.lock')
                      and os.path.join(options.cache_dir, lockfile)
                          not in repo_dirs])
  lockfiles = [repo_dir + '.lock' for repo_dir in repo_dirs
               if os.path.exists(repo_dir + '.lock')]

  if not options.force:
    parser.error('git cache unlock requires -f|--force to do anything. '
                 'Refusing to unlock the following repo caches: '
                 ', '.join(lockfiles))

  unlocked = []
  untouched = []
  for repo_dir in repo_dirs:
    lf = Lockfile(repo_dir)
    config_lock = os.path.join(repo_dir, 'config.lock')
    unlocked = False
    if os.path.exists(config_lock):
      os.remove(config_lock)
      unlocked = True
    if lf.break_lock():
      unlocked = True

    if unlocked:
      unlocked.append(repo_dir)      
    else:
      untouched.append(repo_dir)

  if unlocked:
    logging.info('Broke locks on these caches: %s' % unlocked)
  if untouched:
    logging.debug('Did not touch these caches: %s' % untouched)


class OptionParser(optparse.OptionParser):
  """Wrapper class for OptionParser to handle global options."""

  def __init__(self, *args, **kwargs):
    optparse.OptionParser.__init__(self, *args, prog='git cache', **kwargs)
    self.add_option('-c', '--cache-dir',
                    help='Path to the directory containing the cache')
    self.add_option('-v', '--verbose', action='count', default=0,
                    help='Increase verbosity (can be passed multiple times)')

  def parse_args(self, args=None, values=None):
    options, args = optparse.OptionParser.parse_args(self, args, values)

    try:
      global_cache_dir = subprocess.check_output(
          [GIT_EXECUTABLE, 'config', '--global', 'cache.cachepath']).strip()
      if options.cache_dir:
        logging.warn('Overriding globally-configured cache directory.')
      else:
        options.cache_dir = global_cache_dir
    except subprocess.CalledProcessError:
      if not options.cache_dir:
        self.error('No cache directory specified on command line '
                   'or in cache.cachepath.')
    options.cache_dir = os.path.abspath(options.cache_dir)

    levels = [logging.WARNING, logging.INFO, logging.DEBUG]
    logging.basicConfig(level=levels[min(options.verbose, len(levels) - 1)])

    return options, args


def main(argv):
  dispatcher = subcommand.CommandDispatcher(__name__)
  return dispatcher.execute(OptionParser(), argv)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
