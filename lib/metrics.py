# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrapper library around ts_mon.

This library provides some wrapper functionality around ts_mon, to make it more
friendly to developers. It also provides import safety, in case ts_mon is not
deployed with your code.
"""

from __future__ import print_function

import contextlib
import datetime
import ssl

from functools import wraps
from collections import namedtuple

from chromite.lib import cros_logging as logging

try:
  from infra_libs import ts_mon
# TODO(akeshet): AttributeError only needs to be caught while landing
# https://chromium-review.googlesource.com/#/c/359447/ , due to some issues in
# cbuildbot bootstrapping and backwards compatibility of ts_mon. I believe that
# after it lands, we no longer need to catch AttributeError here.
except (ImportError, RuntimeError, AttributeError):
  ts_mon = None


# This number is chosen because 1.16^100 seconds is about
# 32 days. This is a good compromise between bucket size
# and dynamic range.
_SECONDS_BUCKET_FACTOR = 1.16

# If none, we create metrics in this process. Otherwise, we send metrics via
# this Queue to a dedicated flushing processes.
MESSAGE_QUEUE = None

MetricCall = namedtuple(
    'MetricCall',
    'metric_name metric_args metric_kwargs '
    'method method_args method_kwargs '
    'reset_after')


class ProxyMetric(object):
  """Redirects any method calls to the message queue."""
  def __init__(self, metric, metric_args, metric_kwargs):
    self.metric = metric
    self.metric_args = metric_args
    self.reset_after = metric_kwargs.pop('reset_after', False)
    self.metric_kwargs = metric_kwargs

  def __getattr__(self, method_name):
    """Redirects all method calls to the MESSAGE_QUEUE."""
    def enqueue(*args, **kwargs):
      MESSAGE_QUEUE.put(MetricCall(
          metric_name=self.metric,
          metric_args=self.metric_args,
          metric_kwargs=self.metric_kwargs,
          method=method_name,
          method_args=args,
          method_kwargs=kwargs,
          reset_after=self.reset_after))
    return enqueue


def _Indirect(fn):
  """Decorates a function to be indirect If MESSAGE_QUEUE is set.

  If MESSAGE_QUEUE is set, the indirect function will return a proxy metrics
  object; otherwise, it behaves normally.
  """
  @wraps(fn)
  def AddToQueueIfPresent(*args, **kwargs):
    if MESSAGE_QUEUE:
      return ProxyMetric(fn.__name__, args, kwargs)
    else:
      # Whether to reset the metric after the flush; this is only used by
      # |ProxyMetric|, so remove this from the kwargs.
      kwargs.pop('reset_after', None)
      return fn(*args, **kwargs)
  return AddToQueueIfPresent


class MockMetric(object):
  """Mock metric object, to be returned if ts_mon is not set up."""

  def _mock_method(self, *args, **kwargs):
    pass

  def __getattr__(self, _):
    return self._mock_method


def _ImportSafe(fn):
  """Decorator which causes |fn| to return MockMetric if ts_mon not imported."""
  @wraps(fn)
  def wrapper(*args, **kwargs):
    if ts_mon:
      return fn(*args, **kwargs)
    else:
      return MockMetric()

  return wrapper


def _Metric(fn):
  """A pipeline of decorators to apply to our metric constructors."""
  return _ImportSafe(_Indirect(fn))


@_Metric
def Counter(name):
  """Returns a metric handle for a counter named |name|."""
  return ts_mon.CounterMetric(name)


@_Metric
def Gauge(name):
  """Returns a metric handle for a gauge named |name|."""
  return ts_mon.GaugeMetric(name)


@_Metric
def String(name):
  """Returns a metric handle for a string named |name|."""
  return ts_mon.StringMetric(name)


@_Metric
def Boolean(name):
  """Returns a metric handle for a boolean named |name|."""
  return ts_mon.BooleanMetric(name)


@_Metric
def Float(name):
  """Returns a metric handle for a float named |name|."""
  return ts_mon.FloatMetric(name)


@_Metric
def CumulativeDistribution(name):
  """Returns a metric handle for a cumulative distribution named |name|."""
  return ts_mon.CumulativeDistributionMetric(name)


@_Metric
def CumulativeSmallIntegerDistribution(name):
  """Returns a metric handle for a cumulative distribution named |name|.

  This differs slightly from CumulativeDistribution, in that the underlying
  metric uses a uniform bucketer rather than a geometric one.

  This metric type is suitable for holding a distribution of numbers that are
  nonnegative integers in the range of 0 to 100.
  """
  return ts_mon.CumulativeDistributionMetric(
      name,
      bucketer=ts_mon.FixedWidthBucketer(1))

@_Metric
def SecondsDistribution(name):
  """Returns a metric handle for a cumulative distribution named |name|.

  The distribution handle returned by this method is better suited than the
  default one for recording handling times, in seconds.

  This metric handle has bucketing that is optimized for time intervals
  (in seconds) in the range of 1 second to 32 days.
  """
  b = ts_mon.GeometricBucketer(growth_factor=_SECONDS_BUCKET_FACTOR)
  return ts_mon.CumulativeDistributionMetric(
      name, bucketer=b, units=ts_mon.MetricsDataUnits.SECONDS)


@contextlib.contextmanager
def SecondsTimer(name, fields=None):
  """Record the time of an operation to a SecondsDistributionMetric.

  Usage:

  with SecondsTimer('timer/name', fields={'foo': 'bar'}):
    doSomething()

  Will record the time taken inside of the context block, to the
  SecondsDistribution named 'timer/name', with the given fields.

  Note that this helper can only be used if the field values are known
  at timer-start time and to not depend on the result of the operation
  being timed.
  """
  m = SecondsDistribution(name)
  t0 = datetime.datetime.now()
  yield
  dt = (datetime.datetime.now() - t0).total_seconds()
  m.add(dt, fields=fields)


def SecondsTimerDecorator(name, fields=None):
  """Decorator to time the duration of function calls.

  Usage:
    @SecondsTimerDecorator('timer/name', fields={'foo': 'bar'})
    def Foo(bar):
      return doStuff()

    is equivalent to

    def Foo(bar):
      with SecondsTimer('timer/name', fields={'foo': 'bar'})
        return doStuff()
  """
  def decorator(fn):
    @wraps(fn)
    def wrapper(*args, **kwargs):
      with SecondsTimer(name, fields):
        return fn(*args, **kwargs)

    return wrapper

  return decorator


def Flush(reset_after=()):
  """Flushes metrics, but warns on transient errors.

  Args:
    reset_after: A list of metrics to reset after flushing.
  """
  if not ts_mon:
    return

  try:
    ts_mon.flush()
    while reset_after:
      reset_after.pop().reset()
  except ssl.SSLError as e:
    logging.warning('Caught transient network error while flushing: %s', e)
  except Exception as e:
    logging.error('Caught exception while flushing: %s', e)
