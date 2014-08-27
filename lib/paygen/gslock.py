#!/usr/bin/python

# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This library can use Google Storage files as basis for locking.

   This is mostly convenient because it works inter-server.
"""

import fixup_path
fixup_path.FixupPath()

import datetime
import logging
import os
import random
import re
import socket

from chromite.lib.paygen import gslib
from chromite.lib.paygen import utils


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

  def __init__(self, gs_path, lock_timeout_mins=120, dry_run=False):
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
    """
    self._gs_path = gs_path
    self._timeout = datetime.timedelta(minutes=lock_timeout_mins)
    self._contents = repr((socket.gethostname(), os.getpid(), id(self),
                           random.random()))
    self._generation = 0
    self._dry_run = dry_run

  def _LockExpired(self):
    """Check to see if an existing lock has timed out.

    Returns:
      True if the lock is expired. False otherwise.
    """
    try:
      modified = self.LastModified()
    except LockProbeError:
      # If we couldn't figure out when the file was last modified, it might
      # have already been released. In any case, it's probably not safe to try
      # to clear the lock, so we'll return False here.
      return False
    return modified and datetime.datetime.utcnow() > modified + self._timeout

  def _AcquireLock(self, filename, retries):
    """Attempt to acquire the lock.

    Args:
      filename: local file to copy into the lock's contents.
      retries: How many times to retry to GS operation to fetch the lock.

    Returns:
      Whether or not the lock was acquired.
    """
    try:
      res = gslib.RunGsutilCommand(['cp', '-v', filename, self._gs_path],
                                   generation=self._generation,
                                   redirect_stdout=True, redirect_stderr=True)
      m = re.search(r'%s#(\d+)' % self._gs_path, res.error)
      if m:
        error = None
        self._generation = int(m.group(1))
      else:
        error = 'No generation found.'
        self._generation = 0
    except gslib.GSLibError as ex:
      error = str(ex)

    try:
      result = gslib.Cat(self._gs_path, generation=self._generation)
    except gslib.CatFail as ex:
      self._generation = 0
      if error:
        raise LockNotAcquired(error)
      raise LockNotAcquired(ex)

    if result == self._contents:
      if error is not None:
        logging.warning('Lock at %s acquired despite copy error.',
                        self._gs_path)
    elif self._LockExpired() and retries >= 0:
      logging.warning('Timing out lock at %s.', self._gs_path)
      try:
        # Attempt to set our generation to whatever the current generation is.
        res = gslib.RunGsutilCommand(['stat', self._gs_path],
                                     redirect_stdout=True)
        m = re.search(r'Generation:\s*(\d+)', res.output)
        # Make sure the lock is still expired and hasn't been stolen by
        # someone else.
        if self._LockExpired():
          self._generation = int(m.group(1))
      except gslib.GSLibError as ex:
        logging.warning('Exception while stat-ing %s: %s', self._gs_path, ex)
        logging.warning('Lock may have been cleared by someone else')

      self._AcquireLock(filename, retries - 1)
    else:
      self._generation = 0
      raise LockNotAcquired(result)

  def LastModified(self):
    """Return the lock's last modification time.

    If the lock is already acquired, uses in-memory values. Otherwise, probes
    the lock file.

    Returns:
      The UTC time when the lock was last modified. None if corresponding
      attribute is missing.

    Raises:
      LockProbeError: if a (non-acquired) lock is not present.
    """
    try:
      res = gslib.RunGsutilCommand(['stat', self._gs_path],
                                   redirect_stdout=True)
      m = re.search(r'Creation time:\s*(.*)', res.output)
      if not m:
        raise LockProbeError('Failed to extract creation time.')
      return datetime.datetime.strptime(m.group(1), '%a, %d %b %Y %H:%M:%S %Z')
    except gslib.GSLibError as ex:
      raise LockProbeError(ex)

    return None

  def Acquire(self):
    """Attempt to acquire the lock.

    Will remove an existing lock if it has timed out.

    Raises:
      LockNotAcquired if it is unable to get the lock.
    """
    if self._dry_run:
      return

    with utils.CreateTempFileWithContents(self._contents) as tmp_file:
      self._AcquireLock(tmp_file.name, gslib.RETRY_ATTEMPTS)

  def Release(self):
    """Release the lock."""
    if self._dry_run:
      return

    try:
      gslib.Remove(self._gs_path, generation=self._generation,
                   ignore_no_match=True)
    except gslib.RemoveFail:
      if not self._LockExpired():
        raise
      logging.warning('Lock at %s expired and was stolen.', self._gs_path)
    self._generation = 0

  def Renew(self):
    """Resets the timeout on a lock you are holding.

    Raises:
      LockNotAcquired if it can't Renew the lock for any reason.
    """
    if self._dry_run:
      return

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
