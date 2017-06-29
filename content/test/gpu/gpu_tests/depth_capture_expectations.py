# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from gpu_tests.gpu_test_expectations import GpuTestExpectations

# See the GpuTestExpectations class for documentation.

class DepthCaptureExpectations(GpuTestExpectations):
  def SetExpectations(self):
    # Sample Usage:
    # self.Fail('DepthCapture_depthStreamToRGBAFloatTexture',
    #     ['mac', 'amd', ('nvidia', 0x1234)], bug=123)
    self.Flaky('DepthCapture_depthStreamToR32FloatTexture',
               ['linux', 'nvidia'], bug=737410)
    self.Flaky('DepthCapture_depthStreamToRGBAFloatTexture',
               ['linux', 'nvidia'], bug=737410)
    self.Flaky('DepthCapture_depthStreamToRGBAUint8Texture',
               ['linux', 'nvidia'], bug=737410)
