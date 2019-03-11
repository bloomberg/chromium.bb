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
    # TODO(kbr): flakily timing out on this configuration.
    self.Flaky('TraceTest_*', ['linux', 'intel', 'debug'], bug=648369)

    # Device traces are not supported on all machines.
    self.Skip('DeviceTraceTest_*')

    # We do not have software H.264 decoding on Android, so it can't survive a
    # context loss which results in hardware decoder loss.
    self.Skip('*_Video_Context_Loss_MP4', ['android'], bug=580386)

    # Skip on platforms where DirectComposition isn't supported
    self.Skip('VideoPathTraceTest_*',
        ['mac', 'linux', 'android', 'chromeos', 'win7'], bug=867136)
    self.Skip('OverlayModeTraceTest_*',
        ['mac', 'linux', 'android', 'chromeos', 'win7'], bug=867136)

    # VP9 videos fail to trigger zero copy video presentation path.
    self.Fail('VideoPathTraceTest_DirectComposition_Video_VP9_Fullsize',
        ['win', 'intel'], bug=930343)

    # Complex overlays test is flaky on Nvidia probably due to its small size.
    self.Flaky('VideoPathTraceTest_DirectComposition_ComplexOverlays',
        ['win10', 'nvidia'], bug=937545)

