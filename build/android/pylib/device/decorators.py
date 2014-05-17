# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Function/method decorators that provide timeout and retry logic.
"""

import functools
import os
import sys

from pylib import constants
from pylib.device import device_errors
from pylib.utils import reraiser_thread
from pylib.utils import timeout_retry

# TODO(jbudorick) Remove once the DeviceUtils implementations are no longer
#                 backed by AndroidCommands / android_testrunner.
sys.path.append(os.path.join(constants.DIR_SOURCE_ROOT, 'third_party',
                             'android_testrunner'))
import errors as old_errors


def _TimeoutRetryWrapper(f, timeout_func, retries_func):
  @functools.wraps(f)
  def TimeoutRetryWrapper(*args, **kwargs):
    timeout = timeout_func(*args, **kwargs)
    retries = retries_func(*args, **kwargs)
    def impl():
      return f(*args, **kwargs)
    try:
      return timeout_retry.Run(impl, timeout, retries)
    except old_errors.WaitForResponseTimedOutError as e:
      raise device_errors.CommandTimeoutError(str(e))
    except old_errors.DeviceUnresponsiveError as e:
      raise device_errors.DeviceUnreachableError(str(e))
    except reraiser_thread.TimeoutError as e:
      raise device_errors.CommandTimeoutError(str(e))
  return TimeoutRetryWrapper


def WithTimeoutAndRetries(f):
  """A decorator that handles timeouts and retries."""
  get_timeout = lambda *a, **kw: kw['timeout']
  get_retries = lambda *a, **kw: kw['retries']
  return _TimeoutRetryWrapper(f, get_timeout, get_retries)


def WithExplicitTimeoutAndRetries(timeout, retries):
  """
  A decorator that handles timeouts and retries using the provided values.
  """
  def decorator(f):
    get_timeout = lambda *a, **kw: timeout
    get_retries = lambda *a, **kw: retries
    return _TimeoutRetryWrapper(f, get_timeout, get_retries)
  return decorator


def WithTimeoutAndRetriesDefaults(default_timeout, default_retries):
  """
  A decorator that handles timeouts and retries using the provided defaults.
  """
  def decorator(f):
    get_timeout = lambda *a, **kw: kw.get('timeout', default_timeout)
    get_retries = lambda *a, **kw: kw.get('retries', default_retries)
    return _TimeoutRetryWrapper(f, get_timeout, get_retries)
  return decorator


def WithTimeoutAndRetriesFromInstance(
    default_timeout_name, default_retries_name):
  """
  A decorator that handles timeouts and retries using instance defaults.
  """
  def decorator(f):
    def get_timeout(inst, *_args, **kwargs):
      return kwargs.get('timeout', getattr(inst, default_timeout_name))
    def get_retries(inst, *_args, **kwargs):
      return kwargs.get('retries', getattr(inst, default_retries_name))
    return _TimeoutRetryWrapper(f, get_timeout, get_retries)
  return decorator

