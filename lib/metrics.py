# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrapper library around ts_mon.

This library provides some wrapper functionality around ts_mon, to make it more
friendly to developers. It also provides import safety, in case ts_mon is not
deployed with your code.
"""

from __future__ import print_function

try:
  from infra_libs.ts_mon.common import metrics
  from infra_libs.ts_mon.common import interface
except (ImportError, RuntimeError):
  metrics = None
  interface = None


class MockMetric(object):
  """Mock metric object, to be returned if ts_mon is not set up."""

  def _mock_method(self, *args, **kwargs):
    pass

  def __getattr__(self, _):
    return self._mock_method


def Counter(name):
  """Returns a counter handle for counter named |name|."""
  if metrics:
    return interface.state.metrics.get(name) or metrics.CounterMetric(name)
  else:
    return MockMetric()


def Gauge(name):
  """Returns a gauge handle for gauge named |name|."""
  if metrics:
    return interface.state.metrics.get(name) or metrics.GaugeMetric(name)
  else:
    return MockMetric()
