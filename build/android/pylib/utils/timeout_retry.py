# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A utility to run functions with timeouts and retries."""
# pylint: disable=W0702

import threading

from pylib.utils import reraiser_thread
from pylib.utils import watchdog_timer


def Run(func, timeout, retries, args=None, kwargs=None):
  """Runs the passed function in a separate thread with timeouts and retries.

  Args:
    func: the function to be wrapped.
    timeout: the timeout in seconds for each try.
    retries: the number of retries.
    args: list of positional args to pass to |func|.
    kwargs: dictionary of keyword args to pass to |func|.

  Returns:
    The return value of func(*args, **kwargs).
  """
  if not args:
    args = []
  if not kwargs:
    kwargs = {}

  # The return value uses a list because Python variables are references, not
  # values. Closures make a copy of the reference, so updating the closure's
  # reference wouldn't update where the original reference pointed.
  ret = [None]
  def RunOnTimeoutThread():
    ret[0] = func(*args, **kwargs)

  while True:
    try:
      name = 'TimeoutThread-for-%s' % threading.current_thread().name
      thread_group = reraiser_thread.ReraiserThreadGroup(
          [reraiser_thread.ReraiserThread(RunOnTimeoutThread, name=name)])
      thread_group.StartAll()
      thread_group.JoinAll(watchdog_timer.WatchdogTimer(timeout))
      return ret[0]
    except:
      if retries <= 0:
        raise
      retries -= 1
