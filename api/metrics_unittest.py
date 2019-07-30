# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for the api/metrics library."""

from __future__ import print_function

import mock

from chromite.api import metrics
from chromite.api.gen.chromite.api import build_api_test_pb2
from chromite.lib import cros_test_lib
from chromite.utils.metrics import (MetricEvent, OP_NAMED_EVENT, OP_START_TIMER,
                                    OP_STOP_TIMER)


class MetricsTest(cros_test_lib.TestCase):
  """Test Metrics deserialization functionality at the API layer."""

  def testDeserializeTimer(self):
    """Test timer math and deserialization into proto objects."""
    response = build_api_test_pb2.TestResultMessage()
    mock_events = [
        MetricEvent(600, 'a.b', OP_START_TIMER, key='100'),
        MetricEvent(1000, 'a.b', OP_STOP_TIMER, key='100'),
    ]
    with mock.patch('chromite.api.metrics.metrics.read_metrics_events',
                    return_value=mock_events):
      metrics.deserialize_metrics_log(response.events)
      self.assertEqual(len(response.events), 1)
      self.assertEqual(response.events[0].name, 'a.b')
      self.assertEqual(response.events[0].timestamp_milliseconds, 1000)
      self.assertEqual(response.events[0].duration_milliseconds, 1000-600)

  def testDeserializeNamedEvent(self):
    """Test deserialization of a named event.

    This test also includes a prefix to test for proper prepending.
    """
    response = build_api_test_pb2.TestResultMessage()
    mock_events = [
        MetricEvent(1000, 'a.named_event', OP_NAMED_EVENT, key=None),
    ]
    with mock.patch('chromite.api.metrics.metrics.read_metrics_events',
                    return_value=mock_events):
      metrics.deserialize_metrics_log(response.events, prefix='prefix')
      self.assertEqual(len(response.events), 1)
      self.assertEqual(response.events[0].name, 'prefix.a.named_event')
      self.assertEqual(response.events[0].timestamp_milliseconds, 1000)
      self.assertFalse(response.events[0].duration_milliseconds)
