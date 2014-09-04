#!/usr/bin/python
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test Utils library."""

import datetime
import mox
import multiprocessing
import os
import socket

import fixup_path
fixup_path.FixupPath()

from chromite.lib import cros_test_lib

from chromite.lib.paygen import gslib
from chromite.lib.paygen import gslock
from chromite.lib.paygen import unittest_lib


class GSLockMockNotLocked(unittest_lib.ContextManagerObj):
  """This is a helper class for mocking out GSLocks which get the lock."""


class GSLockMockLocked(unittest_lib.ContextManagerObj):
  """This is a helper class for mocking out GSLocks which don't get the lock."""
  def __enter__(self):
    raise gslock.LockNotAcquired()


def _InProcessAcquire(lock_uri):
  """Acquire a lock in a sub-process, but don't release.

  This helper has to be pickleable, so can't be a member of the test class.

  Args:
    lock_uri: URI of the lock to acquire.

  Returns:
    boolean telling if this method got the lock.
  """
  lock = gslock.Lock(lock_uri)
  try:
    lock.Acquire()
    return True
  except gslock.LockNotAcquired:
    return False


def _InProcessDoubleAcquire(lock_uri):
  """Acquire a lock in a sub-process, and reacquire it a second time.

  Do not release the lock after acquiring.

  This helper has to be pickleable, so can't be a member of the test class.

  Args:
    lock_uri: URI of the lock to acquire.

  Returns:
    int describing how many times it acquired a lock.
  """
  count = 0

  lock = gslock.Lock(lock_uri)
  try:
    lock.Acquire()
    count += 1
    lock.Acquire()
    count += 1
  except gslock.LockNotAcquired:
    pass

  return count


def _InProcessDataUpdate(lock_uri_data_uri):
  """Increment a number in a GS file protected by a lock.

  Keeps looking until the lock is acquired, so effectively, blocking. Stores
  or increments an integer in the data_uri by one, once.

  This helper has to be pickleable, so can't be a member of the test class.

  Args:
    lock_uri_data_uri: Tuple containing (lock_uri, data_uri). Passed as
                       a tuple, since multiprocessing.Pool.map only allows
                       a single argument in.

    lock_uri: URI of the lock to acquire.
    data_uri: URI of the data file to create/increment.

  Returns:
    boolean describing if this method got the lock.
  """

  lock_uri, data_uri = lock_uri_data_uri

  # Keep trying until the lock is acquired.
  while True:
    try:
      with gslock.Lock(lock_uri):
        if gslib.Exists(data_uri):
          data = int(gslib.Cat(data_uri)) + 1
        else:
          data = 1

        gslib.CreateWithContents(data_uri, str(data))
        return True

    except gslock.LockNotAcquired:
      pass


class GSLockTest(mox.MoxTestBase):
  """This test suite covers the GSLock file."""

  @cros_test_lib.NetworkTest()
  def setUp(self):
    self.mox = mox.Mox()

    # Use the unique id to make sure the tests can be run multiple places.
    unique_id = "%s.%d" % (socket.gethostname(), os.getpid())

    self.lock_uri = 'gs://chromeos-releases-test/test-%s-gslock' % unique_id
    self.data_uri = 'gs://chromeos-releases-test/test-%s-data' % unique_id

    # Clear out any flags left from previous failure
    gslib.Remove(self.lock_uri, ignore_no_match=True)
    gslib.Remove(self.data_uri, ignore_no_match=True)

    # To make certain we don't self update while running tests.
    os.environ['CROSTOOLS_NO_SOURCE_UPDATE'] = '1'

  @cros_test_lib.NetworkTest()
  def tearDown(self):
    self.assertFalse(gslib.Exists(self.lock_uri))
    self.assertFalse(gslib.Exists(self.data_uri))
    self.mox.UnsetStubs()

  @cros_test_lib.NetworkTest()
  def testLock(self):
    """Test getting a lock."""
    # We aren't using the mox in the usual way, we just want to force a known
    # hostname.
    self.mox.StubOutWithMock(socket, 'gethostname')
    socket.gethostname().MultipleTimes().AndReturn('TestHost')
    self.mox.ReplayAll()

    # We want to make sure that probing the lock's last modified time does not
    # have detrimental effect on the internal state of the lock object, hence
    # testing w/ and w/o probing.
    for probe_last_modified in (False, True):
      lock = gslock.Lock(self.lock_uri)

      self.assertFalse(gslib.Exists(self.lock_uri))
      if probe_last_modified:
        self.assertRaises(gslock.LockProbeError, lock.LastModified)
      time_before = datetime.datetime.utcnow()
      lock.Acquire()
      time_after = datetime.datetime.utcnow()
      self.assertTrue(gslib.Exists(self.lock_uri))
      if probe_last_modified:
        modified = lock.LastModified()
        self.assertGreater(modified, time_before)
        self.assertLess(modified, time_after)

      contents = gslib.Cat(self.lock_uri)
      self.assertTrue(contents.startswith("('TestHost', "))

      lock.Release()
      self.assertFalse(gslib.Exists(self.lock_uri))
      if probe_last_modified:
        self.assertRaises(gslock.LockProbeError, lock.LastModified)

    self.mox.VerifyAll()

  @cros_test_lib.NetworkTest()
  def testLockRepetition(self):
    """Test aquiring same lock multiple times."""
    self.mox.StubOutWithMock(socket, 'gethostname')
    socket.gethostname().MultipleTimes().AndReturn('TestHost')
    self.mox.ReplayAll()

    lock = gslock.Lock(self.lock_uri)

    self.assertFalse(gslib.Exists(self.lock_uri))
    lock.Acquire()
    self.assertTrue(gslib.Exists(self.lock_uri))

    lock.Acquire()
    self.assertTrue(gslib.Exists(self.lock_uri))

    lock.Release()
    self.assertFalse(gslib.Exists(self.lock_uri))

    lock.Acquire()
    self.assertTrue(gslib.Exists(self.lock_uri))

    lock.Release()
    self.assertFalse(gslib.Exists(self.lock_uri))

  @cros_test_lib.NetworkTest()
  def testLockConflict(self):
    """Test lock conflict."""

    for probe_last_modified in (False, True):
      lock1 = gslock.Lock(self.lock_uri)
      lock2 = gslock.Lock(self.lock_uri)

      if probe_last_modified:
        self.assertRaises(gslock.LockProbeError, lock1.LastModified)
        self.assertRaises(gslock.LockProbeError, lock2.LastModified)

      # Manually lock 1, and ensure lock2 can't lock
      time_before = datetime.datetime.utcnow()
      lock1.Acquire()
      time_after = datetime.datetime.utcnow()
      if probe_last_modified:
        modified1 = lock1.LastModified()
        modified2 = lock2.LastModified()
        self.assertEqual(modified1, modified2)
        self.assertGreater(modified1, time_before)
        self.assertLess(modified1, time_after)
      self.assertRaises(gslock.LockNotAcquired, lock2.Acquire)
      lock1.Release()

      # Use a with clause on 2, and ensure 1 can't lock
      time_before = datetime.datetime.utcnow()
      with lock2:
        time_after = datetime.datetime.utcnow()
        if probe_last_modified:
          modified1 = lock1.LastModified()
          modified2 = lock2.LastModified()
          self.assertEqual(modified1, modified2)
          self.assertGreater(modified2, time_before)
          self.assertLess(modified2, time_after)
        self.assertRaises(gslock.LockNotAcquired, lock1.Acquire)

      # Ensure we can renew a given lock
      lock1.Acquire()
      time_before = datetime.datetime.utcnow()
      lock1.Renew()
      time_after = datetime.datetime.utcnow()
      if probe_last_modified:
        modified1 = lock1.LastModified()
        self.assertGreater(modified1, time_before)
        self.assertLess(modified1, time_after)
      lock1.Release()

      # Ensure we get an error renewing a lock we don't hold
      self.assertRaises(gslock.LockNotAcquired, lock1.Renew)

  @cros_test_lib.NetworkTest()
  def testLockTimeout(self):
    """Test getting a lock when an old timed out one is present."""

    # Both locks are always timed out.
    lock1 = gslock.Lock(self.lock_uri, lock_timeout_mins = -1)
    lock2 = gslock.Lock(self.lock_uri, lock_timeout_mins = -1)

    lock1.Acquire()
    lock2.Acquire()

    gslib.Remove(self.lock_uri)

  @cros_test_lib.NetworkTest()
  def testRaceToAcquire(self):
    """Have lots of processes race to acquire the same lock."""
    count = 20
    pool = multiprocessing.Pool(processes=count)
    results = pool.map(_InProcessAcquire, [self.lock_uri] * count)

    # Clean up the lock since the processes explicitly only acquire.
    gslib.Remove(self.lock_uri)

    # Ensure that only one of them got the lock.
    self.assertEqual(results.count(True), 1)

  @cros_test_lib.NetworkTest()
  def testRaceToDoubleAcquire(self):
    """Have lots of processes race to double acquire the same lock."""
    count = 20
    pool = multiprocessing.Pool(processes=count)
    results = pool.map(_InProcessDoubleAcquire, [self.lock_uri] * count)

    # Clean up the lock sinc the processes explicitly only acquire.
    gslib.Remove(self.lock_uri)

    # Ensure that only one of them got the lock (and got it twice).
    self.assertEqual(results.count(0), count-1)
    self.assertEqual(results.count(2), 1)

  @cros_test_lib.NetworkTest()
  def testMultiProcessDataUpdate(self):
    """Have lots of processes update a GS file proctected by a lock."""
    count = 200
    pool = multiprocessing.Pool(processes=count)
    results = pool.map(_InProcessDataUpdate,
                       [(self.lock_uri, self.data_uri)] * count)

    self.assertEqual(gslib.Cat(self.data_uri), str(count))

    # Ensure that all report success
    self.assertEqual(results.count(True), count)

    # Clean up the data file.
    gslib.Remove(self.data_uri)


if __name__ == '__main__':
  cros_test_lib.main()
