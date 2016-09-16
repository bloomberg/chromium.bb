# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for chromite.lib.metrics."""

from __future__ import print_function

import mock

from chromite.lib import metrics
from chromite.lib import parallel
from chromite.lib import cros_test_lib


class TestIndirectMetrics(cros_test_lib.MockTestCase):
  """Tests the behavior of _Indirect metrics."""

  def testEnqueue(self):
    """Test that _Indirect enqueues messages correctly."""
    metric = metrics.Boolean

    with parallel.Manager() as manager:
      q = manager.Queue()
      self.PatchObject(metrics, 'MESSAGE_QUEUE', q)

      proxy_metric = metric('foo')
      proxy_metric.example('arg1', 'arg2')

      message = q.get(timeout=10)

    self.assertEqual(
        message,
        metrics.MetricCall(metric.__name__, ('foo',),
                           {}, 'example', ('arg1', 'arg2'), {},
                           False))

class TestSecondsTimer(cros_test_lib.MockTestCase):
  """Tests the behavior of SecondsTimer and SecondsTimerDecorator."""

  def setUp(self):
    self._mockMetric = mock.MagicMock()
    self.PatchObject(metrics, 'SecondsDistribution',
                     return_value=self._mockMetric)

  @metrics.SecondsTimerDecorator('fooname', fields={'foo': 'bar'})
  def _DecoratedFunction(self, *args, **kwargs):
    pass

  def testDecorator(self):
    """Test that calling a decorated function ends up emitting metric."""
    self._DecoratedFunction(1, 2, 3, foo='bar')
    self.assertEqual(metrics.SecondsDistribution.call_count, 1)
    self.assertEqual(self._mockMetric.add.call_count, 1)

  def testContextManager(self):
    """Test that timing context manager emits a metric."""
    with metrics.SecondsTimer('fooname'):
      pass
    self.assertEqual(metrics.SecondsDistribution.call_count, 1)
    self.assertEqual(self._mockMetric.add.call_count, 1)
