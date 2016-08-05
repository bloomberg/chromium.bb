# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for chromite.lib.metrics."""

from __future__ import print_function

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
                           {}, 'example', ('arg1', 'arg2'), {}))
