# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Basic locking functionality.
"""

import os
import errno
import fcntl
from chromite.lib import cros_build_lib


class FileLock(cros_build_lib.MasterPidContextManager):

  def __init__(self, path, description=None, verbose=True):
    """
    Args:
      path: On disk pathway to lock.  Can be a directory or a file.
      description: A description for this lock- what is it protecting?
    """
    cros_build_lib.MasterPidContextManager.__init__(self)
    self.path = os.path.abspath(path)
    self._fd = None
    self._verbose = verbose
    if description is None:
      description = "lock %s" % (self.path,)
    self.description = description

  @property
  def fd(self):
    fd = self._fd
    if fd is None:
      # Keep the child from holding the lock/fd via closing the fd on exec.
      # note os.O_CLOEXEC is >=py3.4 only.  Yes, we're ahead of the curve here.
      cloexec = getattr(os, 'O_CLOEXEC', 0)
      fd = self._fd = os.open(self.path, os.W_OK|os.O_CREAT|cloexec)
      if cloexec == 0:
        fcntl.fcntl(fd, fcntl.F_SETFD,
                    fcntl.fcntl(fd, fcntl.F_GETFD)|fcntl.FD_CLOEXEC)
    return fd

  def _enforce_lock(self, flags, message):
    # Try nonblocking first, if it fails, display the context/message,
    # and then wait on the lock.
    try:
      fcntl.lockf(self.fd, flags|fcntl.LOCK_NB)
      return
    except EnvironmentError, e:
      if e.errno != errno.EAGAIN:
        raise
    if self.description:
      message = '%s: blocking while %s' % (self.description, message)
    if self._verbose:
      cros_build_lib.Info(message)
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
