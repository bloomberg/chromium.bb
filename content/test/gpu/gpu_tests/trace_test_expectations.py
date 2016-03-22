# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from gpu_tests.gpu_test_expectations import GpuTestExpectations

# See the GpuTestExpectations class for documentation.

class TraceTestExpectations(GpuTestExpectations):
  def SetExpectations(self):
    # Sample Usage:
    # self.Fail('TraceTest.Canvas2DRedBox',
    #     ['mac', 'amd', ('nvidia', 0x1234)], bug=123)

    # Skip some stories as a temporary debugging measure.
    # https://crbug.com/595754.
    self.Skip('trace_test.IOSurface2DCanvasWebGL', ['mac'], bug=595754)
    self.Skip('trace_test.2DCanvasWebGL', bug=595754)


class DeviceTraceTestExpectations(GpuTestExpectations):
  def SetExpectations(self):
    # Device traces are not supported on all machines.
    self.Skip('*')
