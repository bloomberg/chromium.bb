# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test flock library.

DEPRECATED: Should be migrated to chromite.lib.locking_unittest.
"""

from __future__ import print_function

import multiprocessing
import os
import sys
import time

from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib.paygen import flock


LOCK_ACQUIRED = 5
LOCK_NOT_ACQUIRED = 6


class FLockTest(cros_test_lib.TempDirTestCase):
  """Test FLock lock class."""

  def _HelperSingleLockTest(self, blocking, shared):
    """Helper method that runs a basic test with/without blocking/sharing."""
    lock = flock.Lock('SingleLock',
                      lock_dir=self.tempdir,
                      blocking=blocking,
                      shared=shared)

    expected_lock_file = os.path.join(self.tempdir, 'SingleLock')

    self.assertFalse(os.path.exists(expected_lock_file))
    self.assertFalse(lock.IsLocked())
    lock.Acquire()
    self.assertTrue(os.path.exists(expected_lock_file))
    self.assertTrue(lock.IsLocked())

    # Acquiring the lock again should be safe.
    lock.Acquire()
    self.assertTrue(lock.IsLocked())

    # Ensure the lock file contains our pid, and nothing else.
    fd = open(expected_lock_file, 'r')
    self.assertEquals(['%d\n' % os.getpid()], fd.readlines())
    fd.close()

    lock.Release()
    self.assertFalse(lock.IsLocked())

    osutils.SafeUnlink(expected_lock_file)

  def _HelperDoubleLockTest(self, blocking1, shared1, blocking2, shared2):
    """Helper method that runs a two-lock test with/without blocking/sharing."""
    lock1 = flock.Lock('DoubleLock',
                       lock_dir=self.tempdir,
                       blocking=blocking1,
                       shared=shared1)
    lock2 = flock.Lock('DoubleLock',
                       lock_dir=self.tempdir,
                       blocking=blocking2,
                       shared=shared2)

    lock1.Acquire()
    self.assertTrue(lock1.IsLocked())
    self.assertFalse(lock2.IsLocked())

    # The second lock should fail to acquire.
    self.assertRaises(flock.LockNotAcquired, lock2.Acquire)
    self.assertTrue(lock1.IsLocked())
    self.assertFalse(lock2.IsLocked())

    lock1.Release()
    self.assertFalse(lock1.IsLocked())
    self.assertFalse(lock2.IsLocked())

    # Releasing second lock should be harmless.
    lock2.Release()
    self.assertFalse(lock1.IsLocked())
    self.assertFalse(lock2.IsLocked())

  def _HelperInsideProcess(self, name, lock_dir, blocking, shared):
    """Helper method that runs a basic test with/without blocking."""

    try:
      with flock.Lock(name,
                      lock_dir=lock_dir,
                      blocking=blocking,
                      shared=shared):
        pass
      sys.exit(LOCK_ACQUIRED)
    except flock.LockNotAcquired:
      sys.exit(LOCK_NOT_ACQUIRED)

  def _HelperStartProcess(self, name, blocking=False, shared=False):
    """Create a process and invoke _HelperInsideProcess in it."""
    p = multiprocessing.Process(target=self._HelperInsideProcess,
                                args=(name, self.tempdir, blocking, shared))
    p.start()

    # It's highly probably that p will have tried to grab the lock before the
    # timer expired, but not certain.
    time.sleep(0.1)

    return p

  def _HelperWithProcess(self, name, expected, blocking=False, shared=False):
    """Create a process and invoke _HelperInsideProcess in it."""
    p = multiprocessing.Process(target=self._HelperInsideProcess,
                                args=(name, self.tempdir, blocking, shared))
    p.start()
    p.join()
    self.assertEquals(p.exitcode, expected)

  def testLockName(self):
    """Make sure that we get the expected lock file name."""
    lock = flock.Lock(lock_name='/tmp/foo')
    self.assertEqual(lock.lock_file, '/tmp/foo')

    lock = flock.Lock(lock_name='foo')
    self.assertEqual(lock.lock_file, '/tmp/run_once/foo')

    lock = flock.Lock(lock_name='foo', lock_dir='/bar')
    self.assertEqual(lock.lock_file, '/bar/foo')

  def testSingleLock(self):
    """Just test getting releasing a lock with options."""
    self._HelperSingleLockTest(blocking=False, shared=False)
    self._HelperSingleLockTest(blocking=True, shared=False)
    self._HelperSingleLockTest(blocking=False, shared=True)
    self._HelperSingleLockTest(blocking=True, shared=True)

  def testDoubleLock(self):
    """Test two lock objects for the same lock file."""
    self._HelperDoubleLockTest(blocking1=False, shared1=False,
                               blocking2=False, shared2=False)

  def testContextMgr(self):
    """Make sure we behave properly with 'with'."""

    name = 'WithLock'

    # Create an instance, and use it in a with
    prelock = flock.Lock(name, lock_dir=self.tempdir)
    self._HelperWithProcess(name, expected=LOCK_ACQUIRED)

    with prelock as lock:
      # Assert the instance didn't change.
      self.assertIs(prelock, lock)
      self._HelperWithProcess(name, expected=LOCK_NOT_ACQUIRED)

    self._HelperWithProcess(name, expected=LOCK_ACQUIRED)

    # Construct the instance in the with expression.
    with flock.Lock(name, lock_dir=self.tempdir) as lock:
      self.assertIsInstance(lock, flock.Lock)
      self._HelperWithProcess(name, expected=LOCK_NOT_ACQUIRED)

    self._HelperWithProcess(name, expected=LOCK_ACQUIRED)

  def testAcquireBeforeWith(self):
    """Sometimes you want to Acquire a lock and then return it into 'with'."""

    name = 'WithLock'
    lock = flock.Lock(name, lock_dir=self.tempdir)
    lock.Acquire()

    self._HelperWithProcess(name, expected=LOCK_NOT_ACQUIRED)

    with lock:
      self._HelperWithProcess(name, expected=LOCK_NOT_ACQUIRED)

    self._HelperWithProcess(name, expected=LOCK_ACQUIRED)

  def testSingleProcessLock(self):
    """Test grabbing the same lock in processes with no conflicts."""
    self._HelperWithProcess('ProcessLock', expected=LOCK_ACQUIRED)
    self._HelperWithProcess('ProcessLock', expected=LOCK_ACQUIRED)
    self._HelperWithProcess('ProcessLock', expected=LOCK_ACQUIRED,
                            blocking=True)
    self._HelperWithProcess('ProcessLock', expected=LOCK_ACQUIRED,
                            shared=True)
    self._HelperWithProcess('ProcessLock', expected=LOCK_ACQUIRED,
                            blocking=True, shared=True)

  def testNonBlockingConflicts(self):
    """Test that we get a lock conflict for non-blocking locks."""
    name = 'ProcessLock'
    with flock.Lock(name, lock_dir=self.tempdir):
      self._HelperWithProcess(name,
                              expected=LOCK_NOT_ACQUIRED)

      self._HelperWithProcess(name,
                              expected=LOCK_NOT_ACQUIRED,
                              shared=True)

    # Can grab it after it's released
    self._HelperWithProcess(name, expected=LOCK_ACQUIRED)

  def testSharedLocks(self):
    """Test lock conflict for blocking locks."""
    name = 'ProcessLock'

    # Intial lock is NOT shared
    with flock.Lock(name, lock_dir=self.tempdir, shared=False):
      self._HelperWithProcess(name, expected=LOCK_NOT_ACQUIRED, shared=True)

    # Intial lock IS shared
    with flock.Lock(name, lock_dir=self.tempdir, shared=True):
      self._HelperWithProcess(name, expected=LOCK_ACQUIRED, shared=True)
      self._HelperWithProcess(name, expected=LOCK_NOT_ACQUIRED, shared=False)

  def testBlockingConflicts(self):
    """Test lock conflict for blocking locks."""
    name = 'ProcessLock'

    # Intial lock is blocking, exclusive
    with flock.Lock(name, lock_dir=self.tempdir, blocking=True):
      self._HelperWithProcess(name,
                              expected=LOCK_NOT_ACQUIRED,
                              blocking=False)

      p = self._HelperStartProcess(name, blocking=True, shared=False)

    # when the with clause exits, p should unblock and get the lock, setting
    # its exit code to sucess now.
    p.join()
    self.assertEquals(p.exitcode, LOCK_ACQUIRED)

    # Intial lock is NON blocking
    with flock.Lock(name, lock_dir=self.tempdir):
      self._HelperWithProcess(name, expected=LOCK_NOT_ACQUIRED)

      p = self._HelperStartProcess(name, blocking=True, shared=False)

    # when the with clause exits, p should unblock and get the lock, setting
    # it's exit code to sucess now.
    p.join()
    self.assertEquals(p.exitcode, LOCK_ACQUIRED)

    # Intial lock is shared, blocking lock is exclusive
    with flock.Lock(name, lock_dir=self.tempdir, shared=True):
      self._HelperWithProcess(name, expected=LOCK_NOT_ACQUIRED)
      self._HelperWithProcess(name, expected=LOCK_ACQUIRED, shared=True)

      p = self._HelperStartProcess(name, blocking=True, shared=False)
      q = self._HelperStartProcess(name, blocking=True, shared=False)

    # when the with clause exits, p should unblock and get the lock, setting
    # it's exit code to sucess now.
    p.join()
    self.assertEquals(p.exitcode, LOCK_ACQUIRED)
    q.join()
    self.assertEquals(p.exitcode, LOCK_ACQUIRED)
