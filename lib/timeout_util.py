# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Functions for implementing timeouts."""

from __future__ import print_function

import contextlib
import datetime
import functools
import signal
import threading
import time

from chromite.lib import cros_logging as logging
from chromite.lib import signals


class TimeoutError(Exception):
  """Raises when code within Timeout has been run too long."""


def Timedelta(num, zero_ok=False):
  """Normalize |num| (in seconds) into a datetime.timedelta."""
  if not isinstance(num, datetime.timedelta):
    num = datetime.timedelta(seconds=num)
  if zero_ok:
    if num.total_seconds() < 0:
      raise ValueError('timing must be >= 0, not %s' % (num,))
  else:
    if num.total_seconds() <= 0:
      raise ValueError('timing must be greater than 0, not %s' % (num,))
  return num


@contextlib.contextmanager
def Timeout(max_run_time,
            error_message="Timeout occurred- waited %(time)s seconds.",
            reason_message=None):
  """ContextManager that alarms if code is ran for too long.

  Timeout can run nested and raises a TimeoutException if the timeout
  is reached. Timeout can also nest underneath FatalTimeout.

  Args:
    max_run_time: How long to wait before sending SIGALRM.  May be a number
      (in seconds) or a datetime.timedelta object.
    error_message: Optional string to wrap in the TimeoutError exception on
      timeout. If not provided, default template will be used.
    reason_message: Optional string to be appended to the TimeoutError
      error_message string. Provide a custom message here if you want to have
      a purpose-specific message without overriding the default template in
      |error_message|.
  """
  max_run_time = int(Timedelta(max_run_time).total_seconds())
  if reason_message:
    error_message += reason_message

  # pylint: disable=W0613
  def kill_us(sig_num, frame):
    raise TimeoutError(error_message % {'time': max_run_time})

  original_handler = signal.signal(signal.SIGALRM, kill_us)
  previous_time = int(time.time())

  # Signal the min in case the leftover time was smaller than this timeout.
  remaining_timeout = signal.alarm(0)
  if remaining_timeout:
    signal.alarm(min(remaining_timeout, max_run_time))
  else:
    signal.alarm(max_run_time)

  try:
    yield
  finally:
    # Cancel the alarm request and restore the original handler.
    signal.alarm(0)
    signal.signal(signal.SIGALRM, original_handler)

    # Ensure the previous handler will fire if it was meant to.
    if remaining_timeout > 0:
      # Signal the previous handler if it would have already passed.
      time_left = remaining_timeout - (int(time.time()) - previous_time)
      if time_left <= 0:
        signals.RelaySignal(original_handler, signal.SIGALRM, None)
      else:
        signal.alarm(time_left)


@contextlib.contextmanager
def FatalTimeout(max_run_time, display_message=None):
  """ContextManager that exits the program if code is run for too long.

  This implementation is fairly simple, thus multiple timeouts
  cannot be active at the same time.

  Additionally, if the timeout has elapsed, it'll trigger a SystemExit
  exception within the invoking code, ultimately propagating that past
  itself.  If the underlying code tries to suppress the SystemExit, once
  a minute it'll retrigger SystemExit until control is returned to this
  manager.

  Args:
    max_run_time: How long to wait.  May be a number (in seconds) or a
      datetime.timedelta object.
    display_message: Optional string message to be included in timeout
      error message, if the timeout occurs.
  """
  max_run_time = int(Timedelta(max_run_time).total_seconds())

  # pylint: disable=W0613
  def kill_us(sig_num, frame):
    # While this SystemExit *should* crash it's way back up the
    # stack to our exit handler, we do have live/production code
    # that uses blanket except statements which could suppress this.
    # As such, keep scheduling alarms until our exit handler runs.
    # Note that there is a potential conflict via this code, and
    # RunCommand's kill_timeout; thus we set the alarming interval
    # fairly high.
    signal.alarm(60)

    # The cbuildbot stage that gets aborted by this timeout should be treated as
    # failed by buildbot.
    error_message = ("Timeout occurred- waited %i seconds, failing." %
                     max_run_time)
    if display_message:
      error_message += ' Timeout reason: %s' % display_message
    logging.PrintBuildbotStepFailure()
    logging.error(error_message)
    raise SystemExit(error_message)

  original_handler = signal.signal(signal.SIGALRM, kill_us)
  remaining_timeout = signal.alarm(max_run_time)
  if remaining_timeout:
    # Restore things to the way they were.
    signal.signal(signal.SIGALRM, original_handler)
    signal.alarm(remaining_timeout)
    # ... and now complain.  Unfortunately we can't easily detect this
    # upfront, thus the reset dance above.
    raise Exception("_Timeout cannot be used in parallel to other alarm "
                    "handling code; failing")
  try:
    yield
  finally:
    # Cancel the alarm request and restore the original handler.
    signal.alarm(0)
    signal.signal(signal.SIGALRM, original_handler)


def TimeoutDecorator(max_time):
  """Decorator used to ensure a func is interrupted if it's running too long."""
  # Save off the built-in versions of time.time, signal.signal, and
  # signal.alarm, in case they get mocked out later. We want to ensure that
  # tests don't accidentally mock out the functions used by Timeout.
  def _Save():
    return time.time, signal.signal, signal.alarm
  def _Restore(values):
    (time.time, signal.signal, signal.alarm) = values
  builtins = _Save()

  def NestedTimeoutDecorator(func):
    @functools.wraps(func)
    def TimeoutWrapper(*args, **kwargs):
      new = _Save()
      try:
        _Restore(builtins)
        with Timeout(max_time):
          _Restore(new)
          try:
            func(*args, **kwargs)
          finally:
            _Restore(builtins)
      finally:
        _Restore(new)

    return TimeoutWrapper

  return NestedTimeoutDecorator


def WaitForReturnTrue(*args, **kwargs):
  """Periodically run a function, waiting in between runs.

  Continues to run until the function returns True.

  Args:
    See WaitForReturnValue([True], ...)

  Raises:
    TimeoutError when the timeout is exceeded.
  """
  WaitForReturnValue([True], *args, **kwargs)


def WaitForReturnValue(values, *args, **kwargs):
  """Periodically run a function, waiting in between runs.

  Continues to run until the function return value is in the list
  of accepted |values|.  See WaitForSuccess for more details.

  Args:
    values: A list or set of acceptable return values.
    *args, **kwargs: See WaitForSuccess for remaining arguments.

  Returns:
    The value most recently returned by |func|.

  Raises:
    TimeoutError when the timeout is exceeded.
  """
  def _Retry(return_value):
    return return_value not in values

  return WaitForSuccess(_Retry, *args, **kwargs)


def WaitForSuccess(retry_check, func, timeout, period=1, side_effect_func=None,
                   func_args=None, func_kwargs=None, fallback_timeout=10):
  """Periodically run a function, waiting in between runs.

  Continues to run given function until return value is accepted by retry check.

  To retry based on raised exceptions see GenericRetry in retry_util.

  Args:
    retry_check: A functor that will be passed the return value of |func| as
      the only argument.  If |func| should be retried |retry_check| should
      return True.
    func: The function to run to test for a value.
    timeout: The maximum amount of time to wait.  May be a number (in seconds)
      or a datetime.timedelta object.
    period: How long between calls to |func|.  May be a number (in seconds) or
      a datetime.timedelta object.
    side_effect_func: Optional function to be called between polls of func,
      typically to output logging messages. The remaining time will be passed
      as a datetime.timedelta object.
    func_args: Optional list of positional arguments to be passed to |func|.
    func_kwargs: Optional dictionary of keyword arguments to be passed to
                 |func|.
    fallback_timeout: We set a secondary timeout based on sigalarm this many
                      seconds after the initial timeout. This should NOT be
                      considered robust, but can allow timeouts inside blocking
                      methods.

  Returns:
    The value most recently returned by |func| that was not flagged for retry.

  Raises:
    TimeoutError when the timeout is exceeded.
  """
  timeout = Timedelta(timeout, zero_ok=True)
  period = Timedelta(period, zero_ok=True)
  fallback_timeout = Timedelta(fallback_timeout)
  func_args = func_args or []
  func_kwargs = func_kwargs or {}

  end = datetime.datetime.now() + timeout

  is_main_thread = isinstance(threading.current_thread(),
                              threading._MainThread)
  def retry():
    while True:
      # Guarantee we always run at least once.
      value = func(*func_args, **func_kwargs)
      if not retry_check(value):
        return value

      # Run the user's callback func if available.
      if side_effect_func:
        delta = end - datetime.datetime.now()
        if delta.total_seconds() < 0:
          delta = datetime.timedelta(seconds=0)
        side_effect_func(delta)

      # If we're just going to sleep past the timeout period, abort now.
      delta = end - datetime.datetime.now()
      if delta <= period:
        raise TimeoutError('Timed out after %s' % timeout)

      time.sleep(period.total_seconds())

  if not is_main_thread:
    # Warning: the function here is not working in the main thread. Since
    # signal only works in main thread, this function may run longer than
    # timeout or even hang.
    return retry()
  else:
    # Use a sigalarm after an extra delay, in case a function we call is
    # blocking for some reason. This should NOT be considered reliable.
    with Timeout(timeout + fallback_timeout):
      return retry()
