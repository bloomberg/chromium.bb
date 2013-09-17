# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from pylib.perf import surface_stats_collector

# TODO(bulach): remove once all references to SurfaceStatsCollector are fixed.
class SurfaceStatsCollector(surface_stats_collector.SurfaceStatsCollector):
  def __init__(self, adb):
    super(SurfaceStatsCollector, self).__init__(adb)
