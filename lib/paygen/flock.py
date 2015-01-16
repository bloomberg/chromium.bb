# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A library for managing file locks.

DEPRECATED: Should be merged into chromite.lib.locking.
"""

from __future__ import print_function

import errno
import fcntl
import os


LOCK_DIR = '/tmp/run_once'


class LockNotAcquired(Exception):
  """Raised when the run_once lock is already held by another lock object.

  Note that this can happen even within the same process.  A second lock
  object targeting the same lock file will fail to acquire the lock regardless
  of process.

  self.lock_file_path has path to lock file involved.
  self.owner_pid has pid of process that currently has lock.
  """

  def __init__(self, lock_file_path, owner_pid, *args, **kwargs):
    Exception.__init__(self, *args, **kwargs)
    self.lock_file_path = lock_file_path
    self.owner_pid = owner_pid

  def __str__(self):
    return 'Lock (%s) held by pid %s' % (self.lock_file_path, self.owner_pid)


class Lock(object):
  """This class grabs an exclusive flock on a file in a specified directory.

  This class can be used in combination with the "with" statement.

  Because the lock is associated with a file descriptor, the lock will
  continue to be held for as long as the file descriptor is open (even
  in subprocesses, exec'd executables, etc).

  For informational purposes only, the pid of the current process is
  written into the lock file when it is held, but it's never removed.
  """

  def __init__(self, lock_name, lock_dir=None, blocking=False, shared=False):
    """Setup our lock class, but don't do any work yet (or grab the lock).

    Args:
      lock_name: The file name of the lock to file. If it's a relative name,
                 it will be expanded based on lock_dir.
      lock_dir: is the directory in which to create lock files, it defaults
                to LOCK_DIR.
      blocking: When trying to acquire a lock, do we block until it's
                available, or raise "LockNotAcquired"? If we block,
                there is no timeout.
      shared: A value of False means get an exclusive lock, and True
              means to get a shared lock.
    """
    if lock_dir == None:
      lock_dir = LOCK_DIR

    # os.path.join will ignore lock_dir, if lock_name is an absolute path.
    self.lock_file = os.path.join(lock_dir, lock_name)

    self._blocking = blocking
    self._shared = shared
    self._fd = None

  def Acquire(self):
    """Acquire the flock.

    It's safe to call this multiple times, though the first Unlock will
    release the lock.
    """
    # Create the directory for our lock files if it doesn't already exist
    try:
      os.makedirs(os.path.dirname(self.lock_file))
    except OSError as e:
      if e.errno is not errno.EEXIST:
        raise

    if not self._fd:
      self._fd = open(self.lock_file, 'a')

    try:
      if self._shared:
        flags = fcntl.LOCK_SH
      else:
        flags = fcntl.LOCK_EX

      if not self._blocking:
        flags |= fcntl.LOCK_NB

      fcntl.flock(self._fd, flags)

      # We have the lock, write our pid into it for informational purposes.
      self._fd.truncate(0)
      self._fd.write(str(os.getpid()) + '\n')
      self._fd.flush()

    except IOError as e:
      self._fd.close()
      self._fd = None

      # We got the error that someone else already held the locked.
      # Can only happen if we are blocking == False.
      if e.errno == errno.EAGAIN:
        # To be helpful, grab pid of owner process from file.
        owner_pid = None
        with open(self.lock_file, 'r') as f:
          owner_pid = f.read().strip()

        raise LockNotAcquired(self.lock_file, owner_pid)

      # Pass along any other error for debugging
      raise

  def Release(self):
    """Release the flock."""

    if self._fd:
      fcntl.flock(self._fd, fcntl.LOCK_UN)
      self._fd.close()
      self._fd = None

  def IsLocked(self):
    """Return True if lock is currently acquired."""
    return bool(self._fd)

  # Lock objects can be used with "with" statements.
  def __enter__(self):
    self.Acquire()
    return self

  def __exit__(self, _type, _value, _traceback):
    self.Release()


def ExecWithLock(cmd, lock_name=None, lock_dir=None, blocking=False):
  """Helper method that execs another program with an flock.

  Args:
    cmd: The command to run through flock.
    lock_name: defaults to the name of the command.
    lock_dir: defaults to LOCK_DIR.
    blocking: Whether to take a blocking lock.

  Raises:
    LockNotAcquired: If the lock wasn't acquired
  """
  if not lock_name:
    lock_name = os.path.basename(cmd[0])

  with Lock(lock_name, lock_dir=lock_dir, blocking=blocking):
    # Our lock file is locked, exec our subprocess. It has an extra fd
    # in it's environment, and the lock on that fd will be held until
    # that fd is closed on exit.
    os.execvp(cmd[0], cmd)

    # Note that the above new process will not return here, which has
    # the effect of never exiting this 'with' context, which means
    # Lock.Unlock() is never called. The lock is released all the same.
