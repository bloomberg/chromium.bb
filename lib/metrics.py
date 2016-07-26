# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrapper library around ts_mon.

This library provides some wrapper functionality around ts_mon, to make it more
friendly to developers. It also provides import safety, in case ts_mon is not
deployed with your code.
"""

from __future__ import print_function

from functools import wraps

try:
  from infra_libs import ts_mon
except (ImportError, RuntimeError):
  ts_mon = None


# This number is chosen because 1.16^100 seconds is about
# 32 days. This is a good compromise between bucket size
# and dynamic range.
_SECONDS_BUCKET_FACTOR = 1.16

# If none, we create metrics in this process. Otherwise, we send metrics via
# this Queue to a dedicated flushing processes.
MESSAGE_QUEUE = None


class ProxyMetric(object):
  """Redirects any method calls to the message queue."""
  def __init__(self, metric):
    self.metric = metric

  def __getattr__(self, name):
    def enqueue(*args, **kwargs):
      MESSAGE_QUEUE.put((self.metric, name, args, kwargs))
    return enqueue


def _Indirect(fn):
  """Decorates a function to be indirect If MESSAGE_QUEUE is set.

  If MESSAGE_QUEUE is set, the indirect function will return a proxy metrics
  object; otherwise, it behaves normally.
  """
  @wraps(fn)
  def AddToQueueIfPresent(*args, **kwargs):
    metric = fn(*args, **kwargs)
    if MESSAGE_QUEUE:
      return ProxyMetric(metric)
    else:
      return metric
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
  return ts_mon.CumulativeDistributionMetric(name, bucketer=b)


flush = ts_mon.flush if ts_mon else lambda: None
