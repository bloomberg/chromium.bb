# -*- coding: utf-8 -*-
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common python commands used by various internal build scripts."""

from __future__ import print_function

import os
import time
import threading

# this import exists, linter apparently has an issue.
import psutil  # pylint: disable=import-error

def ListdirFullpath(directory):
  """Return all files in a directory with full pathnames.

  Args:
    directory: directory to find files for.

  Returns:
    Full paths to every file in that directory.
  """
  return [os.path.join(directory, f) for f in os.listdir(directory)]


class RestrictedAttrDict(dict):
  """Define a dictionary which is also a struct.

  The keys will belong to a restricted list of values.
  """

  _slots = ()

  def __init__(self, *args, **kwargs):
    """Ensure that only the expected keys are added during initialization."""
    dict.__init__(self, *args, **kwargs)

    # Ensure all slots are at least populated with None.
    for key in self._slots:
      self.setdefault(key)

    for key in self.keys():
      assert key in self._slots, 'Unexpected key %s in %s' % (key, self._slots)

  def __setattr__(self, name, val):
    """Setting an attribute, actually sets a dictionary value."""
    if name not in self._slots:
      raise AttributeError("'%s' may not have attribute '%s'" %
                           (self.__class__.__name__, name))
    self[name] = val

  def __getattr__(self, name):
    """Fetching an attribute, actually fetches a dictionary value."""
    if name not in self:
      raise AttributeError("'%s' has no attribute '%s'" %
                           (self.__class__.__name__, name))
    return self[name]

  def __setitem__(self, name, val):
    """Restrict which keys can be stored in this dictionary."""
    if name not in self._slots:
      raise KeyError(name)
    dict.__setitem__(self, name, val)

  def __str__(self):
    """Default stringification behavior."""
    name = self._name if hasattr(self, '_name') else self.__class__.__name__
    return '%s (%s)' % (name, self._GetAttrString())

  def _GetAttrString(self, delim=', ', equal='='):
    """Return string showing all non-None values of self._slots.

    The ordering of attributes in self._slots is honored in string.

    Args:
      delim: String for separating key/value elements in result.
      equal: String to put between key and associated value in result.

    Returns:
      A string like "a='foo', b=12".
    """
    slots = [s for s in self._slots if self[s] is not None]
    elems = ['%s%s%r' % (s, equal, self[s]) for s in slots]
    return delim.join(elems)

  def _clear_if_default(self, key, default):
    """Helper for constructors.

    If they key value is set to the default value, set it to None.

    Args:
      key: Key value to check and possibly clear.
      default: Default value to compare the key value against.
    """
    if self[key] == default:
      self[key] = None


def ReadLsbRelease(sysroot):
  """Reads the /etc/lsb-release file out of the given sysroot.

  Args:
    sysroot: The path to sysroot of an image to read sysroot/etc/lsb-release.

  Returns:
    The lsb-release file content in a dictionary of key/values.
  """
  lsb_release_file = os.path.join(sysroot, 'etc', 'lsb-release')
  lsb_release = {}
  with open(lsb_release_file, 'r') as f:
    for line in f:
      tokens = line.strip().split('=')
      lsb_release[tokens[0]] = tokens[1]

  return lsb_release


class MemoryConsumptionSemaphore(object):
  """A semaphore that tries to acquire only if there is enough memory available.

    Watch the free memory of the host in order to not oversubscribe. Also,
    rate limit so that memory consumption of previously launched
    fledgling process can swell to peak(ish) level. Also assumes this semaphore
    controls the vast majority of the memory utilization on the host when
    active.

    It will also measure the available total memory when there are no
    acquires (and when it was initialized) and use that to baseline a guess
    based on the configured max memory per acquire to limit the total of
    acquires.
  """
  SYSTEM_POLLING_INTERVAL_SECONDS = 0.5

  def __init__(self, system_available_buffer_bytes=None,
               single_proc_max_bytes=None,
               quiescence_time_seconds=None,
               unchecked_acquires=0,
               clock=time.time):
    """Create a new MemoryConsumptionSemaphore.

    Args:
      system_available_buffer_bytes (int): The number of bytes to reserve
        on the system as a buffer against moving into swap (or OOM).
      single_proc_max_bytes (int): The number of bytes we expect a process
        to consume on the system.
      quiescence_time_seconds (float): The number of seconds to wait at a
        minimum between acquires. The purpose is to ensure the subprocess
        begins to consume a stable amount of memory.
      unchecked_acquires (int): The number acquires to allow without checking
        available memory. This is to allow users to supply a mandatory minimum
        even if the semaphore would otherwise not allow it (because of the
        current available memory being to low).
      clock (fn): Function that gets float time.

    Returns:
      A new MemoryConsumptionSemaphore.
    """
    self.quiescence_time_seconds = quiescence_time_seconds
    self.unchecked_acquires = unchecked_acquires
    self._timer_future = 0  # epoch.
    self._lock = threading.RLock()  # single thread may acquire lock twice.
    self._n_within = 0  # current count holding memory\resources.
    self._clock = clock  # injected, primarily useful for testing.
    self._system_available_buffer_bytes = system_available_buffer_bytes
    self._single_proc_max_bytes = single_proc_max_bytes
    self._base_available = self._get_system_available()

  def _get_system_available(self):
    """Get the system's available memory (memory free before swapping)."""
    return psutil.virtual_memory().available

  def _timer_blocked(self):
    """Check the timer, if we're past it return true, otherwise false."""
    if self._clock() >= self._timer_future:
      return False
    else:
      return True

  def _inc_within(self):
    """Inc the lock."""
    with self._lock:
      self._n_within += 1

  def _dec_within(self):
    """Dec the lock."""
    with self._lock:
      self._n_within -= 1

  def _set_timer(self):
    """Set a time in the future to unblock after."""
    self._timer_future = max(self._clock() + self.quiescence_time_seconds,
                             self._timer_future)

  def _allow_consumption(self):
    """Calculate max utilization to determine if another should be allowed.

    Returns:
      Boolean if you're allowed to consume (acquire).
    """
    one_more_total = (self._n_within + 1) * self._single_proc_max_bytes
    total_avail = self._base_available - self._system_available_buffer_bytes
    # If the guessed max plus yourself is above what's available including
    # the buffer then refuse to admit.
    if total_avail < one_more_total:
      return False
    else:
      return True

  def acquire(self, timeout):
    """Block until enough available memory, or timeout.

    Polls the system every SYSTEM_POLLING_INTERVAL_SECONDS and determines
    if there is enough available memory to proceed, or potentially timeout.

    Args:
      timeout (float): Time to block for available memory before return.

    Returns:
      True if you should go, False if you timed out waiting.
    """
    # Remeasure the base.
    if self._n_within == 0:
      self._base_available = self._get_system_available()

    # If you're under the unchecked_acquires go for it, but lock
    # so that we can't race for it.
    with self._lock:
      if self._n_within < self.unchecked_acquires:
        self._set_timer()
        self._inc_within()
        return True

    init_time = self._clock()
    # If not enough memory or timer is running then block.
    while init_time + timeout > self._clock():
      with self._lock:
        if not self._timer_blocked():
          # Extrapolate system state and perhaps allow.
          if self._allow_consumption():
            self._set_timer()
            self._inc_within()
            return True

      time.sleep(MemoryConsumptionSemaphore.SYSTEM_POLLING_INTERVAL_SECONDS)

    # There was no moment before timeout where we could have ran the task.
    return False

  def release(self):
    """Releases a single acquire."""
    self._dec_within()
