# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from gpu_tests.gpu_test_expectations import GpuTestExpectations

# See the GpuTestExpectations class for documentation.

class TraceTestExpectations(GpuTestExpectations):
  def SetExpectations(self):
    # Sample Usage:
    # self.Fail('trace_test.Canvas2DRedBox',
    #     ['mac', 'amd', ('nvidia', 0x1234)], bug=123)
    self.Skip('trace_test.2DCanvasWebGL', ['win'], bug=638744)
    self.Skip('trace_test.SolidColorBackground', ['win'], bug=638744)
    self.Skip('trace_test.WebGLGreenTriangle.AA.NoAlpha', ['win'], bug=638744)
    self.Skip('trace_test.WebGLGreenTriangle.NoAA.Alpha', ['win'], bug=638744)


class DeviceTraceTestExpectations(GpuTestExpectations):
  def SetExpectations(self):
    # Device traces are not supported on all machines.
    self.Skip('*')
