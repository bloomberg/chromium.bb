# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from perf import thermal_throttle

# TODO(bulach): remove once all references to ThermalThrottle are fixed.
class ThermalThrottle(thermal_throttle.ThermalThrottle):
  def __init__(self, adb):
    super(ThermalThrottle, self).__init__(adb)
