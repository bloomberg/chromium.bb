# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrapper library around ts_mon.

This library provides some wrapper functionality around ts_mon, to make it more
friendly to developers. It also provides import safety, in case ts_mon is not
deployed with your code.
"""

from __future__ import print_function

try:
  from infra_libs.ts_mon.common import distribution
  from infra_libs.ts_mon.common import metrics
  from infra_libs.ts_mon.common import interface
except (ImportError, RuntimeError):
  metrics = None
  interface = None


# This number is chosen because 1.16^100 seconds is about
# 32 days. This is a good compromise between bucket size
# and dynamic range.
_SECONDS_BUCKET_FACTOR = 1.16


class MockMetric(object):
  """Mock metric object, to be returned if ts_mon is not set up."""

  def _mock_method(self, *args, **kwargs):
    pass

  def __getattr__(self, _):
    return self._mock_method


def _ImportSafe(fn):
  """Decorator which causes |fn| to return MockMetric if ts_mon not imported."""
  def wrapper(*args, **kwargs):
    if metrics:
      return fn(*args, **kwargs)
    else:
      return MockMetric()

  return wrapper


def _GetOrConstructMetric(name, fn, *args, **kwargs):
  """Returns an existing metric handle or constructs a new one.

  Args:
    name: Name of the metric to construct.
    fn: Callable constructor object, if that metric doesn't exist.

  Returns:
    A metric handle, or a MockMetric if ts_mon was not imported.
  """
  return interface.state.metrics.get(name) or fn(name, *args, **kwargs)


@_ImportSafe
def Counter(name):
  """Returns a metric handle for a counter named |name|."""
  return _GetOrConstructMetric(name, metrics.CounterMetric)


@_ImportSafe
def Gauge(name):
  """Returns a metric handle for a gauge named |name|."""
  return _GetOrConstructMetric(name, metrics.GaugeMetric)


@_ImportSafe
def String(name):
  """Returns a metric handle for a string named |name|."""
  return _GetOrConstructMetric(name, metrics.StringMetric)


@_ImportSafe
def Boolean(name):
  """Returns a metric handle for a boolean named |name|."""
  return _GetOrConstructMetric(name, metrics.BooleanMetric)


@_ImportSafe
def Float(name):
  """Returns a metric handle for a float named |name|."""
  return _GetOrConstructMetric(name, metrics.FloatMetric)


@_ImportSafe
def CumulativeDistribution(name):
  """Returns a metric handle for a cumulative distribution named |name|."""
  return _GetOrConstructMetric(name, metrics.CumulativeDistributionMetric)


@_ImportSafe
def CumulativeSmallIntegerDistribution(name):
  """Returns a metric handle for a cumulative distribution named |name|.

  This differs slightly from CumulativeDistribution, in that the underlying
  metric uses a uniform bucketer rather than a geometric one.

  This metric type is suitable for holding a distribution of numbers that are
  nonnegative integers in the range of 0 to 100.
  """
  return _GetOrConstructMetric(name, metrics.CumulativeDistributionMetric,
                               bucketer=distribution.FixedWidthBucketer(1))

@_ImportSafe
def SecondsDistribution(name):
  """Returns a metric handle for a cumulative distribution named |name|.

  The distribution handle returned by this method is better suited than the
  default one for recording handling times, in seconds.

  This metric handle has bucketing that is optimized for time intervals
  (in seconds) in the range of 1 second to 32 days.
  """
  b = distribution.GeometricBucketer(growth_factor=_SECONDS_BUCKET_FACTOR)
  return _GetOrConstructMetric(name, metrics.CumulativeDistributionMetric,
                               bucketer=b)
