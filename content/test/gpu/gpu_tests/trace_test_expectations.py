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

    self.Flaky('trace_test.Canvas2DRedBox', bug=512622, max_num_retries=5)
    self.Flaky('trace_test.CSS3DBlueBox', bug=512622, max_num_retries=5)

    # Test has become increasingly flaky around 7/31/15: crbug.com/517232
    self.Flaky('trace_test.WebGLGreenTriangle', bug=517232, max_num_retries=5)

class DeviceTraceTestExpectations(GpuTestExpectations):
  def SetExpectations(self):
    # Device traces are not supported on all machines.
    self.Skip('*')
