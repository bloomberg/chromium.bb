# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Basic locking functionality.
"""

import os
import errno
import fcntl
import tempfile
from chromite.lib import cros_build_lib


class _Lock(cros_build_lib.MasterPidContextManager):

  """Base lockf based locking.  Derivatives need to override _GetFd"""

  def __init__(self, description=None, verbose=True):
    """Initialize this instance.

    Args:
      path: On disk pathway to lock.  Can be a directory or a file.
      description: A description for this lock- what is it protecting?
    """
    cros_build_lib.MasterPidContextManager.__init__(self)
    self._verbose = verbose
    self.description = description
    self._fd = None

  @property
  def fd(self):
    if self._fd is None:
      self._fd = self._GetFd()
      # Ensure that all derivatives of this lock can't bleed the fd
      # across execs.
      fcntl.fcntl(self._fd, fcntl.F_SETFD,
                  fcntl.fcntl(self._fd, fcntl.F_GETFD) | fcntl.FD_CLOEXEC)
    return self._fd

  def _GetFd(self):
    raise NotImplementedError(self, '_GetFd')

  def _enforce_lock(self, flags, message):
    # Try nonblocking first, if it fails, display the context/message,
    # and then wait on the lock.
    try:
      fcntl.lockf(self.fd, flags|fcntl.LOCK_NB)
      return
    except EnvironmentError, e:
      if e.errno == errno.EDEADLOCK:
        self.unlock()
      elif e.errno != errno.EAGAIN:
        raise
    if self.description:
      message = '%s: blocking while %s' % (self.description, message)
    if self._verbose:
      cros_build_lib.Info(message)
    try:
      fcntl.lockf(self.fd, flags)
    except EnvironmentError, e:
      if e.errno != errno.EDEADLOCK:
        raise
      self.unlock()
      fcntl.lockf(self.fd, flags)

  def read_lock(self, message="taking read lock"):
    """
    Take a read lock (shared), downgrading from write if required.

    Args:
      message: A description of what/why this lock is being taken.
    Returns:
      self, allowing it to be used as a `with` target.
    Raises:
      IOError if the operation fails in some way.
    """
    self._enforce_lock(fcntl.LOCK_SH, message)
    return self

  def write_lock(self, message="taking write lock"):
    """
    Take a write lock (exclusive), upgrading from read if required.

    Note that if the lock state is being upgraded from read to write,
    a deadlock potential exists- as such we *will* release the lock
    to work around it.  Any consuming code should not assume that
    transitioning from shared to exclusive means no one else has
    gotten at the critical resource in between for this reason.

    Args:
      message: A description of what/why this lock is being taken.
    Returns:
      self, allowing it to be used as a `with` target.
    Raises:
      IOError if the operation fails in some way.
    """
    self._enforce_lock(fcntl.LOCK_EX, message)
    return self

  def unlock(self):
    """
    Release any locks held.  Noop if no locks are held.

    Raises:
      IOError if the operation fails in some way.
    """
    if self._fd is not None:
      fcntl.lockf(self._fd, fcntl.LOCK_UN)

  def __del__(self):
    # TODO(ferringb): Convert this to snakeoil.weakref.WeakRefFinalizer
    # if/when that rebasing occurs.
    self.close()

  def close(self):
    """
    Release the underlying lock and close the fd.
    """
    if self._fd is not None:
      self.unlock()
      os.close(self._fd)
      self._fd = None

  def _enter(self):
    # Force the fd to be opened via touching the property.
    # We do this to ensure that even if entering a context w/out a lock
    # held, we can do locking in that critical section if the code requests it.
    # pylint: disable=W0104
    self.fd
    return self

  def _exit(self, exc_type, exc, traceback):
    try:
      self.unlock()
    finally:
      self.close()


class FileLock(_Lock):

  def __init__(self, path, description=None, verbose=True):
    """
    Args:
      path: On disk pathway to lock.  Can be a directory or a file.
      description: A description for this lock- what is it protecting?
    """
    if description is None:
      description = "lock %s" % (path,)
    _Lock.__init__(self, description=description, verbose=verbose)
    self.path = os.path.abspath(path)

  def _GetFd(self):
    # If we're on py3.4 and this attribute is exposed, use it to close
    # the threading race between open and fcntl setting; this is
    # extremely paranoid code, but might as well.
    cloexec = getattr(os, 'O_CLOEXEC', 0)
    return os.open(self.path, os.W_OK|os.O_CREAT|cloexec, 0664)


class ProcessLock(_Lock):

  """Process level locking visible to parent/child only.

  This lock is basically a more robust version of what
  multiprocessing.Lock does.  That implementation uses semaphores
  internally which require cleanup/deallocation code to run to release
  the lock; a SIGKILL hitting the process holding the lock violates those
  assumptions leading to a stuck lock.

  Thus this implementation is based around locking of a deleted tempfile;
  lockf locks are guranteed to be released once the process/fd is closed.
  """

  def _GetFd(self):
    with tempfile.TemporaryFile() as f:
      # We don't want to hold onto the object indefinitely; we just want
      # the fd to a temporary inode, preferably one that isn't vfs accessible.
      # Since TemporaryFile closes the fd once the object is GC'd, we just
      # dupe the fd so we retain a copy, while the original TemporaryFile
      # goes away.
      return os.dup(f.fileno())
