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
    self._headers = None
    self._dry_run = dry_run

  @property
  def _generation(self):
    """Get the current x-goog-generation, if any."""
    return 0 if self._headers is None else self._headers.get('generation', 0)

  @staticmethod
  def _LastModifiedFromHeaders(headers):
    """Returns the latest modification time given a dictionary of lock headers.

    Returns:
      The UTC time when a lock was last modified (datetime.datetime). None if
      headers are empty or corresponding attribute is missing.
    """
    modified = headers.get('Last-Modified') if headers else None
    if modified is not None:
      return datetime.datetime.strptime(modified, '%a, %d %b %Y %H:%M:%S %Z')

  def _LockExpired(self):
    """Check to see if an existing lock has timed out.

    Returns:
      True if the lock is expired. False otherwise.
    """
    modified = self._LastModifiedFromHeaders(self._headers)
    return modified and datetime.datetime.utcnow() > modified + self._timeout

  def _AcquireLock(self, filename, retries):
    """Attempt to acquire the lock.

    Args:
      filename: local file to copy into the lock's contents.
      retries: How many times to retry to GS operation to fetch the lock.

    Returns:
      Whether or not the lock was acquired.
    """
    generation = self._generation
    try:
      gslib.Copy(filename, self._gs_path, generation=generation)
      error = None
    except gslib.CopyFail as ex:
      error = str(ex)

    headers = {}
    try:
      result = gslib.Cat(self._gs_path, headers=headers)
    except gslib.CatFail as ex:
      if error:
        raise LockNotAcquired(error)
      raise LockNotAcquired(ex)
    self._headers = headers

    if result == self._contents and self._generation != generation:
      if error is not None:
        logging.warning('Lock at %s acquired despite copy error.',
                        self._gs_path)
    elif self._LockExpired() and retries >= 0:
      logging.warning('Timing out lock at %s.', self._gs_path)
      self._AcquireLock(filename, retries - 1)
    else:
      self._headers = None
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
    headers = self._headers
    if headers is None:
      headers = {}
      try:
        gslib.Stat(self._gs_path, headers=headers)
      except gslib.StatFail as ex:
        raise LockProbeError(ex)

    return self._LastModifiedFromHeaders(headers)

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
    self._headers = None

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
