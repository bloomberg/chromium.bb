# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import threading
import time

class TimerQueue(threading.Thread):
  """Executes timers at a given interval.

  This class provides the ability to run methods at a given interval.  All
  methods are fired synchronously.  Only one method is running at a time.

  Example of using TimerQueue:
    def _fooPrinter(word):
      print('foo : %s' % word)

    timers = TimerQueue()
    timers.addTimer(self._fooPrinter, 15, args=('hello',))
    timers.start()

  >> hello will be printed after 15 seconds

  Note: TimerQueue is a subclass of threading.Thread, call start() to activate;
  do not call run() directly.
  """

  def __init__(self):
    """Initializes a TimerQueue object."""
    threading.Thread.__init__(self, name='timer_thread')
    self.timer_queue_lock = threading.Lock()
    self.terminate = False
    self.wait_time = 1
    self.timers = []

  def AddTimer(self, method, interval, args=()):
    """Adds a timer to the queue.

    Args:
      method: the method to be called at the given interval
      interval: delay between method runs, in seconds
      args: arguments to be passed to the method
    """
    self.timer_queue_lock.acquire()
    next_time = time.time() + interval
    self.timers.append({'method': method, 'interval': interval,
                        'next time': next_time, 'args': copy.copy(args)})
    self.timer_queue_lock.release()

  def SetResolution(self, resolution):
    """Sets the timer check frequency, in seconds."""
    self.wait_time = resolution

  def RemoveTimer(self, method):
    """Removes a timer from the queue.

    Args:
      method: the timer containing the given method to be removed
    """
    self.timer_queue_lock.acquire()
    for timer in self.timers:
      if timer['method'] == method:
        self.timers.remove(timer)
        break
    self.timer_queue_lock.release()

  def Stop(self):
    """Stops the timer."""
    self.terminate = True

  def run(self):
    """Primary run loop for the timer."""
    while True:
      now = time.time()
      self.timer_queue_lock.acquire()
      for timer in self.timers:
        if timer['next time'] <= now:
          # Use * to break the list into separate arguments
          timer['method'](*timer['args'])
          timer['next time'] += timer['interval']
      self.timer_queue_lock.release()
      if self.terminate:
        return
      time.sleep(self.wait_time)
