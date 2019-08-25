# -*- coding: utf-8 -*-
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test Utils library."""

from __future__ import print_function

import os
import time
import threading

from chromite.lib import cros_test_lib
from chromite.lib import osutils

from chromite.lib.paygen import utils

# We access a lot of protected members during testing.
# pylint: disable=protected-access

# Tests involving the memory semaphore should block this long.
ACQUIRE_TIMEOUT = 120
ACQUIRE_SHOULD_BLOCK_TIMEOUT = 20


class TestUtils(cros_test_lib.TempDirTestCase):
  """Test utils methods."""

  @classmethod
  def setUpClass(cls):
    """Class setup to run system polling quickly in semaphore tests."""
    utils.MemoryConsumptionSemaphore.SYSTEM_POLLING_INTERVAL_SECONDS = 0

  class MockClock(object):
    """Mock clock that is manually incremented."""

    def __call__(self):
      """Return the current mock time."""
      return self._now

    def __init__(self):
      """Init the clock."""
      self._now = 0.0

    def add_time(self, n):
      """Add some amount of time."""
      self._now += n

  def mock_get_system_available(self, how_much):
    """Mock the system's available memory, used to override psutil."""
    return lambda: how_much

  def testListdirFullpath(self):
    file_a = os.path.join(self.tempdir, 'a')
    file_b = os.path.join(self.tempdir, 'b')

    osutils.Touch(file_a)
    osutils.Touch(file_b)

    self.assertEqual(sorted(utils.ListdirFullpath(self.tempdir)),
                     [file_a, file_b])

  def testReadLsbRelease(self):
    """Tests that we correctly read the lsb release file."""
    path = os.path.join(self.tempdir, 'etc', 'lsb-release')
    osutils.WriteFile(path, 'key=value\nfoo=bar\n', makedirs=True)

    self.assertEqual(utils.ReadLsbRelease(self.tempdir),
                     {'key': 'value', 'foo': 'bar'})

  def testMassiveMemoryConsumptionSemaphore(self):
    """Tests that we block on not having enough memory."""
    # You should never get 2**64 bytes.
    _semaphore = utils.MemoryConsumptionSemaphore(
        system_available_buffer_bytes=2 ** 64,
        single_proc_max_bytes=2 ** 64,
        quiescence_time_seconds=0)

    # You can't get that much.
    self.assertEqual(_semaphore.acquire(
        ACQUIRE_SHOULD_BLOCK_TIMEOUT), False)

  def testNoMemoryConsumptionSemaphore(self):
    """Tests that you can acquire a very little amount of memory."""
    # You should always get one byte.
    _semaphore = utils.MemoryConsumptionSemaphore(
        system_available_buffer_bytes=1,
        single_proc_max_bytes=1,
        quiescence_time_seconds=0)

    # Sure you can have two bytes.
    self.assertEqual(_semaphore.acquire(ACQUIRE_TIMEOUT), True)
    _semaphore.release()

  def testQuiesceMemoryConsumptionSemaphore(self):
    """Tests that you wait for memory utilization to settle (quiesce)."""
    # All you want is two bytes.
    _semaphore = utils.MemoryConsumptionSemaphore(
        system_available_buffer_bytes=1,
        single_proc_max_bytes=1,
        quiescence_time_seconds=2)

    # Should want two bytes, have a whole lot.
    _semaphore._get_system_available = self.mock_get_system_available(2**64)
    self.assertEqual(_semaphore.acquire(ACQUIRE_TIMEOUT), True)
    _semaphore.release()

    # Should want two bytes, have a whole lot (but you'll block for 2 seconds).
    _semaphore._get_system_available = self.mock_get_system_available(2**64 - 2)
    self.assertEqual(_semaphore.acquire(ACQUIRE_TIMEOUT), True)
    _semaphore.release()

  def testUncheckedMemoryConsumptionSemaphore(self):
    """Tests that some acquires work unchecked."""
    # You should never get 2**64 bytes (i wish...).
    _semaphore = utils.MemoryConsumptionSemaphore(
        system_available_buffer_bytes=2**64,
        single_proc_max_bytes=2**64,
        quiescence_time_seconds=2,
        unchecked_acquires=2)

    # Nothing available, but we expect unchecked_acquires to allow it.
    _semaphore._get_system_available = self.mock_get_system_available(0)
    self.assertEqual(_semaphore.acquire(ACQUIRE_TIMEOUT), True)
    _semaphore.release()
    self.assertEqual(_semaphore.acquire(ACQUIRE_TIMEOUT), True)
    _semaphore.release()

  def testQuiescenceUnblocksMemoryConsumptionSemaphore(self):
    """Test that after a period of time you unblock (due to quiescence)."""
    _semaphore = utils.MemoryConsumptionSemaphore(
        system_available_buffer_bytes=1,
        single_proc_max_bytes=1,
        quiescence_time_seconds=2,
        unchecked_acquires=0)

    # Make large amount of memory available, but we expect quiescence
    # to block the second task.
    _semaphore._get_system_available = self.mock_get_system_available(2**64)
    start_time = time.time()
    self.assertEqual(_semaphore.acquire(ACQUIRE_TIMEOUT), True)
    _semaphore.release()

    # Get the lock or die trying. We spin fast here instead of ACQUIRE_TIMEOUT.
    while not _semaphore.acquire(1):
      continue
    _semaphore.release()

    # Check that the lock was acquired after quiescence_time_seconds.
    end_time = time.time()
    # Why 1.8? Because the clock isn't monotonic and we don't want to flake.
    self.assertGreaterEqual(end_time - start_time, 1.8)

  def testThreadedMemoryConsumptionSemaphore(self):
    """Test many threads simultaneously using the Semaphore."""
    initial_memory = 6
    # These are lists so we can write nonlocal.
    mem_avail = [initial_memory]
    good_thread_exits = [0]
    mock_clock = TestUtils.MockClock()
    lock, exit_lock = threading.Lock(), threading.Lock()
    test_threads = 8

    # Currently executes in 1.6 seconds a 2 x Xeon Gold 6154 CPUs
    get_and_releases = 50

    def sub_mem():
      with lock:
        mem_avail[0] = mem_avail[0] - 1

    def add_mem():
      with lock:
        mem_avail[0] = mem_avail[0] + 1

    def get_mem():
      with lock:
        return mem_avail[0]

    # Ask for two bytes available each time.
    _semaphore = utils.MemoryConsumptionSemaphore(
        system_available_buffer_bytes=1,
        single_proc_max_bytes=1,
        quiescence_time_seconds=0.1,
        unchecked_acquires=0,
        clock=mock_clock)
    _semaphore._get_system_available = get_mem

    def hammer_semaphore():
      for _ in range(get_and_releases):
        while not _semaphore.acquire(0.1):
          continue
        # Simulate 'using the memory'.
        sub_mem()
        add_mem()
        _semaphore.release()
      with exit_lock:
        good_thread_exits[0] = good_thread_exits[0] + 1

    threads = [threading.Thread(target=hammer_semaphore)
               for _ in range(test_threads)]
    for x in threads:
      x.daemon = True
      x.start()

    # ~Maximum 600 seconds realtime, keeps clock ticking for overall timeout.
    for _ in range(60000):
      time.sleep(0.01)
      mock_clock.add_time(0.1)

      # Maybe we can break early? (and waste some time for other threads).
      threads_dead = [not x.isAlive() for x in threads]
      if all(threads_dead):
        break

    # If we didn't get here a thread did not exit. This is fatal and may
    # indicate a deadlock has been introduced.
    self.assertEqual(initial_memory, get_mem())
    self.assertEqual(good_thread_exits[0], test_threads)
