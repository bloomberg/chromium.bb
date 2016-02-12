# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from benchmarks import media_router_metric
from telemetry.page import page_test


class MediaRouterPageTest(page_test.PageTest):
  """Performs a measurement of Media Route dialog latency."""

  def __init__(self):
    super(MediaRouterPageTest, self).__init__()

  def ValidateAndMeasurePage(self, page, tab, results):
    media_router_metric.MediaRouterMetric().AddResults(tab, results)
