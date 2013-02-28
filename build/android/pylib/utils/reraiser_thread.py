# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Thread and ThreadGroup that reraise exceptions on the main thread."""

import sys
import threading


class ReraiserThread(threading.Thread):
  """Thread class that can reraise exceptions."""
  def __init__(self, func, args=[], kwargs={}):
    super(ReraiserThread, self).__init__()
    self.daemon = True
    self._func = func
    self._args = args
    self._kwargs = kwargs
    self._exc_info = None

  def ReraiseIfException(self):
    """Reraise exception if an exception was raised in the thread."""
    if self._exc_info:
      raise self._exc_info[0], self._exc_info[1], self._exc_info[2]

  #override
  def run(self):
    """Overrides Thread.run() to add support for reraising exceptions."""
    try:
      self._func(*self._args, **self._kwargs)
    except:
      self._exc_info = sys.exc_info()
      raise


class ReraiserThreadGroup(object):
  """A group of ReraiserThread objects."""
  def __init__(self, threads=[]):
    """Initialize thread group.

    Args:
      threads: a list of ReraiserThread objects; defaults to empty.
    """
    self._threads = threads

  def Add(self, thread):
    """Add a thread to the group.

    Args:
      thread: a ReraiserThread object.
    """
    self._threads.append(thread)

  def StartAll(self):
    """Start all threads."""
    for thread in self._threads:
      thread.start()

  def JoinAll(self):
    """Join all threads.

    Reraises exceptions raised by the child threads and supports
    breaking immediately on exceptions raised on the main thread.
    """
    alive_threads = self._threads[:]
    while alive_threads:
      for thread in alive_threads[:]:
        # Allow the main thread to periodically check for interrupts.
        thread.join(0.1)
        if not thread.isAlive():
          alive_threads.remove(thread)
    # All threads are allowed to complete before reraising exceptions.
    for thread in self._threads:
      thread.ReraiseIfException()
