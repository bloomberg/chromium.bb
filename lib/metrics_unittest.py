# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for chromite.lib.metrics."""

from __future__ import print_function

import pickle
import Queue

from chromite.lib import metrics
from chromite.lib import cros_test_lib


class TestIndirectMetrics(cros_test_lib.MockTestCase):
  """Tests the behavior of _Indirect metrics."""

  def testEnqueue(self):
    """Test that _Indirect enqueues messages correctly."""
    metric = metrics.Boolean('foo')
    # The metric should be pickleable
    pickle.dumps(metric)

    q = Queue.Queue()
    self.PatchObject(metrics, 'MESSAGE_QUEUE', q)

    proxy_metric = metrics.Boolean('foo')
    proxy_metric.example_method('arg1', 'arg2')

    entry = q.get_nowait()
    self.assertEqual(entry, (metric, 'example_method', ('arg1', 'arg2'), {}))
