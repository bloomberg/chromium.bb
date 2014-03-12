# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Monkeypatch IMapIterator so that Ctrl-C can kill everything properly.
# Derived from https://gist.github.com/aljungberg/626518
import multiprocessing.pool
from multiprocessing.pool import IMapIterator
def wrapper(func):
  def wrap(self, timeout=None):
    return func(self, timeout=timeout or 1e100)
  return wrap
IMapIterator.next = wrapper(IMapIterator.next)
IMapIterator.__next__ = IMapIterator.next
# TODO(iannucci): Monkeypatch all other 'wait' methods too.


import binascii
import contextlib
import functools
import logging
import signal
import sys
import tempfile
import threading

import subprocess2


GIT_EXE = 'git.bat' if sys.platform.startswith('win') else 'git'


class BadCommitRefException(Exception):
  def __init__(self, refs):
    msg = ('one of %s does not seem to be a valid commitref.' %
           str(refs))
    super(BadCommitRefException, self).__init__(msg)


def memoize_one(**kwargs):
  """Memoizes a single-argument pure function.

  Values of None are not cached.

  Kwargs:
    threadsafe (bool) - REQUIRED. Specifies whether to use locking around
      cache manipulation functions. This is a kwarg so that users of memoize_one
      are forced to explicitly and verbosely pick True or False.

  Adds three methods to the decorated function:
    * get(key, default=None) - Gets the value for this key from the cache.
    * set(key, value) - Sets the value for this key from the cache.
    * clear() - Drops the entire contents of the cache.  Useful for unittests.
    * update(other) - Updates the contents of the cache from another dict.
  """
  assert 'threadsafe' in kwargs, 'Must specify threadsafe={True,False}'
  threadsafe = kwargs['threadsafe']

  if threadsafe:
    def withlock(lock, f):
      def inner(*args, **kwargs):
        with lock:
          return f(*args, **kwargs)
      return inner
  else:
    def withlock(_lock, f):
      return f

  def decorator(f):
    # Instantiate the lock in decorator, in case users of memoize_one do:
    #
    # memoizer = memoize_one(threadsafe=True)
    #
    # @memoizer
    # def fn1(val): ...
    #
    # @memoizer
    # def fn2(val): ...

    lock = threading.Lock() if threadsafe else None
    cache = {}
    _get = withlock(lock, cache.get)
    _set = withlock(lock, cache.__setitem__)

    @functools.wraps(f)
    def inner(arg):
      ret = _get(arg)
      if ret is None:
        ret = f(arg)
        if ret is not None:
          _set(arg, ret)
      return ret
    inner.get = _get
    inner.set = _set
    inner.clear = withlock(lock, cache.clear)
    inner.update = withlock(lock, cache.update)
    return inner
  return decorator


def _ScopedPool_initer(orig, orig_args):  # pragma: no cover
  """Initializer method for ScopedPool's subprocesses.

  This helps ScopedPool handle Ctrl-C's correctly.
  """
  signal.signal(signal.SIGINT, signal.SIG_IGN)
  if orig:
    orig(*orig_args)


@contextlib.contextmanager
def ScopedPool(*args, **kwargs):
  """Context Manager which returns a multiprocessing.pool instance which
  correctly deals with thrown exceptions.

  *args - Arguments to multiprocessing.pool

  Kwargs:
    kind ('threads', 'procs') - The type of underlying coprocess to use.
    **etc - Arguments to multiprocessing.pool
  """
  if kwargs.pop('kind', None) == 'threads':
    pool = multiprocessing.pool.ThreadPool(*args, **kwargs)
  else:
    orig, orig_args = kwargs.get('initializer'), kwargs.get('initargs', ())
    kwargs['initializer'] = _ScopedPool_initer
    kwargs['initargs'] = orig, orig_args
    pool = multiprocessing.pool.Pool(*args, **kwargs)

  try:
    yield pool
    pool.close()
  except:
    pool.terminate()
    raise
  finally:
    pool.join()


class ProgressPrinter(object):
  """Threaded single-stat status message printer."""
  def __init__(self, fmt, enabled=None, stream=sys.stderr, period=0.5):
    """Create a ProgressPrinter.

    Use it as a context manager which produces a simple 'increment' method:

      with ProgressPrinter('(%%(count)d/%d)' % 1000) as inc:
        for i in xrange(1000):
          # do stuff
          if i % 10 == 0:
            inc(10)

    Args:
      fmt - String format with a single '%(count)d' where the counter value
        should go.
      enabled (bool) - If this is None, will default to True if
        logging.getLogger() is set to INFO or more verbose.
      stream (file-like) - The stream to print status messages to.
      period (float) - The time in seconds for the printer thread to wait
        between printing.
    """
    self.fmt = fmt
    if enabled is None:  # pragma: no cover
      self.enabled = logging.getLogger().isEnabledFor(logging.INFO)
    else:
      self.enabled = enabled

    self._count = 0
    self._dead = False
    self._dead_cond = threading.Condition()
    self._stream = stream
    self._thread = threading.Thread(target=self._run)
    self._period = period

  def _emit(self, s):
    if self.enabled:
      self._stream.write('\r' + s)
      self._stream.flush()

  def _run(self):
    with self._dead_cond:
      while not self._dead:
        self._emit(self.fmt % {'count': self._count})
        self._dead_cond.wait(self._period)
        self._emit((self.fmt + '\n') % {'count': self._count})

  def inc(self, amount=1):
    self._count += amount

  def __enter__(self):
    self._thread.start()
    return self.inc

  def __exit__(self, _exc_type, _exc_value, _traceback):
    self._dead = True
    with self._dead_cond:
      self._dead_cond.notifyAll()
    self._thread.join()
    del self._thread


def branches(*args):
  NO_BRANCH = ('* (no branch)', '* (detached from ')
  for line in run('branch', *args).splitlines():
    if line.startswith(NO_BRANCH):
      continue
    yield line.split()[-1]


def config_list(option):
  try:
    return run('config', '--get-all', option).split()
  except subprocess2.CalledProcessError:
    return []


def current_branch():
  return run('rev-parse', '--abbrev-ref', 'HEAD')


def parse_commitrefs(*commitrefs):
  """Returns binary encoded commit hashes for one or more commitrefs.

  A commitref is anything which can resolve to a commit. Popular examples:
    * 'HEAD'
    * 'origin/master'
    * 'cool_branch~2'
  """
  try:
    return map(binascii.unhexlify, hash_multi(*commitrefs))
  except subprocess2.CalledProcessError:
    raise BadCommitRefException(commitrefs)


def run(*cmd, **kwargs):
  """Runs a git command. Returns stdout as a string.

  If logging is DEBUG, we'll print the command before we run it.

  kwargs
    autostrip (bool) - Strip the output. Defaults to True.
  Output string is always strip()'d.
  """
  autostrip = kwargs.pop('autostrip', True)
  cmd = (GIT_EXE,) + cmd
  logging.debug('Running %s', ' '.join(repr(tok) for tok in cmd))
  ret = subprocess2.check_output(cmd, stderr=subprocess2.PIPE, **kwargs)
  if autostrip:
    ret = (ret or '').strip()
  return ret


def hash_one(reflike):
  return run('rev-parse', reflike)


def hash_multi(*reflike):
  return run('rev-parse', *reflike).splitlines()


def intern_f(f, kind='blob'):
  """Interns a file object into the git object store.

  Args:
    f (file-like object) - The file-like object to intern
    kind (git object type) - One of 'blob', 'commit', 'tree', 'tag'.

  Returns the git hash of the interned object (hex encoded).
  """
  ret = run('hash-object', '-t', kind, '-w', '--stdin', stdin=f)
  f.close()
  return ret


def tags(*args):
  return run('tag', *args).splitlines()


def tree(treeref, recurse=False):
  """Returns a dict representation of a git tree object.

  Args:
    treeref (str) - a git ref which resolves to a tree (commits count as trees).
    recurse (bool) - include all of the tree's decendants too. File names will
      take the form of 'some/path/to/file'.

  Return format:
    { 'file_name': (mode, type, ref) }

    mode is an integer where:
      * 0040000 - Directory
      * 0100644 - Regular non-executable file
      * 0100664 - Regular non-executable group-writeable file
      * 0100755 - Regular executable file
      * 0120000 - Symbolic link
      * 0160000 - Gitlink

    type is a string where it's one of 'blob', 'commit', 'tree', 'tag'.

    ref is the hex encoded hash of the entry.
  """
  ret = {}
  opts = ['ls-tree', '--full-tree']
  if recurse:
    opts.append('-r')
  opts.append(treeref)
  try:
    for line in run(*opts).splitlines():
      mode, typ, ref, name = line.split(None, 3)
      ret[name] = (mode, typ, ref)
  except subprocess2.CalledProcessError:
    return None
  return ret


def upstream(branch):
  try:
    return run('rev-parse', '--abbrev-ref', '--symbolic-full-name',
               branch+'@{upstream}')
  except subprocess2.CalledProcessError:
    return None


def mktree(treedict):
  """Makes a git tree object and returns its hash.

  See |tree()| for the values of mode, type, and ref.

  Args:
    treedict - { name: (mode, type, ref) }
  """
  with tempfile.TemporaryFile() as f:
    for name, (mode, typ, ref) in treedict.iteritems():
      f.write('%s %s %s\t%s\0' % (mode, typ, ref, name))
    f.seek(0)
    return run('mktree', '-z', stdin=f)
