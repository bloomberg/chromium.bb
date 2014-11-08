# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This library can use Google Storage files as basis for locking.

A lock is acquired by creating a GS file (file creation is atomic). This means
the URL defines the lock.

When we create a lock, we also find its 'generation', which is a version
number for the file that will be unique over time. We can prefix most GS
commands with our version number to ensure the file hasn't been changed
underneath us somehow. This allows us to ensure our operations are atomic.

Locks have a timeout value, and may be legally acquired by any one else after
the timeout has expired. Generation values are important to making sure this is
an atomic operation. This timeout is agreed upon in advance, and there can be
confusion if different clients use different timeout values. The current lock
owner is NOT notified if a lock is expired.

A lock owner can 'Renew' the lock at any time to ensure it doesn't expire.

Lock files will normally hold the hostname and pid of the process that created
them, which can be useful for debugging purposes.

Note that lock files will be left behind forever if not explicitly cleaned up by
the creating server.
"""

# pylint: disable=bad-whitespace

from __future__ import print_function

import fixup_path
fixup_path.FixupPath()

import datetime
import logging

from chromite.lib import cros_build_lib
from chromite.lib import gs


class LockProbeError(Exception):
  """Raised when there was an error probing a lock file."""


class LockNotAcquired(Exception):
  """Raised when the lock is already held by another process."""


class Lock(object):
  """This class manages a google storage file as a form of lock.

  This class can be used in conjuction with a "with" clause to ensure
  the lock is released, or directly.

    try:
      with gslock.Lock("gs://chromoes-releases/lock-file"):
        # Protected code
        ...
    except LockNotAcquired:
      # Error handling
      ...

    lock = gslock.Lock("gs://chromoes-releases/lock-file")
    try:
      lock.Acquire()
    except LockNotAcquired:
      # Error handling

    # Protected code
    ...

    lock.Release()

    Locking is strictly atomic, except when timeouts are involved.

    It assumes that local server time is in sync with Google Storage server
    time.
  """
  def __init__(self, gs_path, lock_timeout_mins=120, dry_run=False,
               ctx=None):
    """Initializer for the lock.

    Args:
      gs_path:
        Path to the potential GS file we use for lock management.
      lock_timeout_mins:
        How long should an existing lock be considered valid? This timeout
        should be long enough that it's never hit unless a server is
        unexpectedly rebooted, lost network connectivity or had
        some other catastrophic error.
      dry_run: do nothing, always succeed
      ctx: chromite.lib.gs.GSContext to use.
    """
    self._gs_path = gs_path
    self._timeout = datetime.timedelta(minutes=lock_timeout_mins)
    self._contents = cros_build_lib.MachineDetails()
    self._generation = 0
    self._ctx = ctx if ctx is not None else gs.GSContext(dry_run=dry_run)

  def _LockExpired(self):
    """Check to see if an existing lock has timed out.

    Returns:
      True if the lock is expired. False otherwise.
    """
    try:
      stat_results = self._ctx.Stat(self._gs_path)
    except gs.GSNoSuchKey:
      # If we couldn't figure out when the file was last modified, it might
      # have already been released. In any case, it's probably not safe to try
      # to clear the lock, so we'll return False here.
      return False, 0

    modified = stat_results.creation_time
    expired =  datetime.datetime.utcnow() > modified + self._timeout

    return expired, stat_results.generation

  def _AcquireLock(self):
    """Attempt to acquire the lock.

    Raises:
      LockNotAcquired: If the lock isn't acquired.
    """
    try:
      self._generation = self._ctx.Copy(
          '-', self._gs_path, input=self._contents, version=self._generation)
      if self._generation is None:
        self._generation = 0
        raise LockProbeError('Unable to detect generation')
    except gs.GSContextPreconditionFailed:
      # We could use Cat here to find who owns the lock, but it's another GS
      # round trip for data that mostly doesn't matter. Also this behavior
      # was triggering BigStore errors.

      # We didn't get the lock, raise the expected exception.
      self._generation = 0
      raise LockNotAcquired()

  def Acquire(self):
    """Attempt to acquire the lock.

    Will remove an existing lock if it has timed out.

    Raises:
      LockNotAcquired if it is unable to get the lock.
    """
    try:
      self._AcquireLock()
    except LockNotAcquired:
      # We failed to get the lock right away, try to expire then acquire.
      expired, generation = self._LockExpired()
      if not expired:
        raise

      # It is expired, grab it, but use it's existing generation to close
      # a race condition with someone else who is also expiring it.
      logging.warning('Attempting to time out lock at %s.', self._gs_path)
      self._generation = generation
      self._AcquireLock()

  def Release(self):
    """Release the lock."""
    try:
      self._ctx.Remove(self._gs_path, version=self._generation,
                       ignore_missing=True)
    except gs.GSContextPreconditionFailed:
      if not self._LockExpired():
        raise
      logging.warning('Lock at %s expired and was stolen.', self._gs_path)
    self._generation = 0

  def Renew(self):
    """Resets the timeout on a lock you are holding.

    Raises:
      LockNotAcquired if it can't Renew the lock for any reason.
    """
    if int(self._generation) == 0:
      raise LockNotAcquired('Lock not held')
    self.Acquire()

  def __enter__(self):
    """Support for entering a with clause."""
    self.Acquire()
    return self

  def __exit__(self, _type, _value, _traceback):
    """Support for exiting a with clause."""
    self.Release()
