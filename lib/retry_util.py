# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Basic infrastructure for implementing retries."""

from __future__ import print_function

import sys
import time

from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging


def GenericRetry(handler, max_retry, functor, *args, **kwargs):
  """Generic retry loop w/ optional break out depending on exceptions.

  To retry based on the return value of |functor| see the timeout_util module.

  Keep in mind that the total sleep time will be the triangular value of
  max_retry multiplied by the sleep value.  e.g. max_retry=5 and sleep=10
  will be T5 (i.e. 5+4+3+2+1) times 10, or 150 seconds total.  Rather than
  use a large sleep value, you should lean more towards large retries and
  lower sleep intervals, or by utilizing backoff_factor.

  Args:
    handler: A functor invoked w/ the exception instance that
      functor(*args, **kwargs) threw.  If it returns True, then a
      retry is attempted.  If False, the exception is re-raised.
    max_retry: A positive integer representing how many times to retry
      the command before giving up.  Worst case, the command is invoked
      (max_retry + 1) times before failing.
    functor: A callable to pass args and kwargs to.
    args: Positional args passed to functor.
    kwargs: Optional args passed to functor.
    sleep: Optional keyword.  Multiplier for how long to sleep between
      retries; will delay (1*sleep) the first time, then (2*sleep),
      continuing via attempt * sleep.
    backoff_factor: Optional keyword. If supplied and > 1, subsequent sleeps
                    will be of length (backoff_factor ^ (attempt - 1)) * sleep,
                    rather than the default behavior of attempt * sleep.
    success_functor: Optional functor that accepts 1 argument. Will be called
                     after successful call to |functor|, with the argument
                     being the number of attempts (1 = |functor| succeeded on
                     first try).
    raise_first_exception_on_failure: Optional boolean which determines which
                                      exception is raised upon failure after
                                      retries. If True, the first exception
                                      that was encountered. If False, the
                                      final one. Default: True.

  Returns:
    Whatever functor(*args, **kwargs) returns.

  Raises:
    Exception: Whatever exceptions functor(*args, **kwargs) throws and
      isn't suppressed is raised.  Note that the first exception encountered
      is what's thrown.
  """

  sleep = kwargs.pop('sleep', 0)
  if max_retry < 0:
    raise ValueError('max_retry needs to be zero or more: %s' % max_retry)

  backoff_factor = kwargs.pop('backoff_factor', 1)
  if backoff_factor < 1:
    raise ValueError('backoff_factor must be 1 or greater: %s'
                     % backoff_factor)

  success_functor = kwargs.pop('success_functor', lambda x: None)
  ret, success = (None, False)

  raise_first_exception_on_failure = kwargs.pop(
      'raise_first_exception_on_failure', True)

  attempt = 0

  exc_info = None
  for attempt in xrange(max_retry + 1):
    if attempt and sleep:
      if backoff_factor > 1:
        sleep_time = sleep * backoff_factor ** (attempt - 1)
      else:
        sleep_time = sleep * attempt
      time.sleep(sleep_time)
    try:
      ret = functor(*args, **kwargs)
      success = True
      break
    except Exception as e:
      # Note we're not snagging BaseException, so MemoryError/KeyboardInterrupt
      # and friends don't enter this except block.
      if not handler(e):
        raise
      # If raise_first_exception_on_failure, we intentionally ignore
      # any failures in later attempts since we'll throw the original
      # failure if all retries fail.
      if exc_info is None or not raise_first_exception_on_failure:
        exc_info = sys.exc_info()

  if success:
    success_functor(attempt + 1)
    return ret

  raise exc_info[0], exc_info[1], exc_info[2]


def RetryException(exc_retry, max_retry, functor, *args, **kwargs):
  """Convenience wrapper for GenericRetry based on exceptions.

  Args:
    exc_retry: A class (or tuple of classes).  If the raised exception
      is the given class(es), a retry will be attempted.  Otherwise,
      the exception is raised.
    max_retry: See GenericRetry.
    functor: See GenericRetry.
    *args: See GenericRetry.
    **kwargs: See GenericRetry.
  """
  if not isinstance(exc_retry, (tuple, type)):
    raise TypeError('exc_retry should be an exception (or tuple), not %r' %
                    exc_retry)
  #pylint: disable=E0102
  def exc_retry(exc, values=exc_retry):
    return isinstance(exc, values)
  return GenericRetry(exc_retry, max_retry, functor, *args, **kwargs)


def RetryCommand(functor, max_retry, *args, **kwargs):
  """Wrapper for RunCommand that will retry a command

  Args:
    functor: RunCommand function to run; retries will only occur on
      RunCommandError exceptions being thrown.
    max_retry: A positive integer representing how many times to retry
      the command before giving up.  Worst case, the command is invoked
      (max_retry + 1) times before failing.
    sleep: Optional keyword.  Multiplier for how long to sleep between
      retries; will delay (1*sleep) the first time, then (2*sleep),
      continuing via attempt * sleep.
    retry_on: If provided, we will retry on any exit codes in the given list.
      Note: A process will exit with a negative exit code if it is killed by a
      signal. By default, we retry on all non-negative exit codes.
    error_check: Optional callback to check the error output.  Return None to
      fall back to |retry_on|, or True/False to set the retry directly.
    log_retries: Whether to log a warning when retriable errors occur.
    args: Positional args passed to RunCommand; see RunCommand for specifics.
    kwargs: Optional args passed to RunCommand; see RunCommand for specifics.

  Returns:
    A CommandResult object.

  Raises:
    Exception:  Raises RunCommandError on error with optional error_message.
  """
  values = kwargs.pop('retry_on', None)
  error_check = kwargs.pop('error_check', lambda x: None)
  log_retries = kwargs.pop('log_retries', True)

  def ShouldRetry(exc):
    """Return whether we should retry on a given exception."""
    if not ShouldRetryCommandCommon(exc):
      return False
    if values is None and exc.result.returncode < 0:
      logging.info('Child process received signal %d; not retrying.',
                   -exc.result.returncode)
      return False

    ret = error_check(exc)
    if ret is not None:
      return ret

    if values is None or exc.result.returncode in values:
      if log_retries:
        logging.warning('Command failed with retriable error.\n%s', exc)
      return True
    return False

  return GenericRetry(ShouldRetry, max_retry, functor, *args, **kwargs)


def ShouldRetryCommandCommon(exc):
  """Returns whether any RunCommand should retry on a given exception."""
  if not isinstance(exc, cros_build_lib.RunCommandError):
    return False
  if exc.result.returncode is None:
    logging.error('Child process failed to launch; not retrying:\n'
                  'command: %s', exc.result.cmdstr)
    return False
  return True


def RunCommandWithRetries(max_retry, *args, **kwargs):
  """Wrapper for RunCommand that will retry a command

  Args:
    max_retry: See RetryCommand and RunCommand.
    *args: See RetryCommand and RunCommand.
    **kwargs: See RetryCommand and RunCommand.

  Returns:
    A CommandResult object.

  Raises:
    Exception:  Raises RunCommandError on error with optional error_message.
  """
  return RetryCommand(cros_build_lib.RunCommand, max_retry, *args, **kwargs)


class DownloadError(Exception):
  """Fetching file via curl failed"""


def RunCurl(args, fail=True, **kwargs):
  """Runs curl and wraps around all necessary hacks.

  Args:
    args: Command line to pass to curl.
    fail: Whether to use --fail w/curl.
    **kwargs: See RunCommandWithRetries and RunCommand.

  Returns:
    A CommandResult object.

  Raises:
    DownloadError: Whenever curl fails for any reason.
  """
  cmd = ['curl']
  if fail:
    cmd.append('--fail')
  cmd.extend(args)

  # These values were discerned via scraping the curl manpage; they're all
  # retry related (dns failed, timeout occurred, etc, see  the manpage for
  # exact specifics of each).
  # Note we allow 22 to deal w/ 500's- they're thrown by google storage
  # occasionally.  This is also thrown when getting 4xx, but curl doesn't
  # make it easy to differentiate between them.
  # Note we allow 35 to deal w/ Unknown SSL Protocol error, thrown by
  # google storage occasionally.
  # Finally, we do not use curl's --retry option since it generally doesn't
  # actually retry anything; code 18 for example, it will not retry on.
  retriable_exits = frozenset([5, 6, 7, 15, 18, 22, 26, 28, 35, 52, 56])

  def _CheckExit(exc):
    """Filter out specific error codes when getting exit 22

    Curl will exit(22) for a wide range of HTTP codes -- both the 4xx and 5xx
    set.  For the 4xx, we don't want to retry.  We have to look at the output.
    """
    if exc.result.returncode == 22:
      if '404 Not Found' in exc.result.error:
        return False
      else:
        return True
    else:
      # We'll let the common exit code filter do the right thing.
      return None

  args = {
      'retry_on': retriable_exits,
      'error_check': _CheckExit,
      'capture_output': True,
  }
  args.update(kwargs)
  try:
    return RunCommandWithRetries(5, cmd, sleep=3, **args)
  except cros_build_lib.RunCommandError as e:
    code = e.result.returncode
    if code in (51, 58, 60):
      # These are the return codes of failing certs as per 'man curl'.
      raise DownloadError(
          'Download failed with certificate error? Try "sudo c_rehash".')
    else:
      try:
        return RunCommandWithRetries(5, cmd, sleep=60, **kwargs)
      except cros_build_lib.RunCommandError as e:
        raise DownloadError('Curl failed w/ exit code %i: %s' %
                            (e.result.returncode, e.result.error))
