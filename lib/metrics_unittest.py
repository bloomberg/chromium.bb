# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for chromite.lib.metrics."""

from __future__ import print_function

import mock
import tempfile

from chromite.lib import cros_test_lib
from chromite.lib import metrics
from chromite.lib import parallel
from chromite.lib import ts_mon_config


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

  def patchTime(self):
    """Simulate time passing to force a Flush() every time a metric is sent."""
    def TimeIterator():
      t = 0
      while True:
        t += ts_mon_config.FLUSH_INTERVAL + 1
        yield t

    self.PatchObject(ts_mon_config,
                     'time',
                     mock.Mock(time=mock.Mock(side_effect=TimeIterator())))

  def testResetAfter(self):
    """Tests that the reset_after flag works to send metrics only once."""
    # By mocking out its "time" module, the forked flushing process will think
    # it should call Flush() whenever we send a metric.
    self.patchTime()

    with tempfile.NamedTemporaryFile(dir='/var/tmp') as out:
      # * The indirect=True flag is required for reset_after to work.
      # * Using debug_file, we send metrics to the temporary file instead of
      # sending metrics to production via PubSub.
      with ts_mon_config.SetupTsMonGlobalState('metrics_unittest',
                                               indirect=True,
                                               debug_file=out.name):
        def MetricName(i, flushed):
          return 'test/metric/name/%d/%s' % (i, flushed)

        # Each of these .set() calls will result in a Flush() call.
        for i in range(7):
          # any extra streams with different fields and reset_after=False
          # will be cleared only if the below metric is cleared.
          metrics.Boolean(MetricName(i, True), reset_after=False).set(
              True, fields={'original': False})

          metrics.Boolean(MetricName(i, True), reset_after=True).set(
              True, fields={'original': True})

        for i in range(7):
          metrics.Boolean(MetricName(i, False), reset_after=False).set(True)


      # By leaving the context, we .join() the flushing process.
      with open(out.name, 'r') as fh:
        content = fh.read()

      # The flushed metrics should be sent only three times, because:
      # * original=False is sent twice
      # * original=True is sent once.
      for i in range(7):
        self.assertEqual(content.count(MetricName(i, True)), 3)

      # The nonflushed metrics are sent once-per-flush.
      # There are 7 of these metrics,
      # * The 0th is sent 7 times,
      # * The 1st is sent 6 times,
      # ...
      # * The 6th is sent 1 time.
      # So the "i"th metric is sent (7-i) times.
      for i in range(7):
        self.assertEqual(content.count(MetricName(i, False)), 7 - i)


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
