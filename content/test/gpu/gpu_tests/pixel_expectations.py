# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from gpu_test_expectations import GpuTestExpectations

# See the GpuTestExpectations class for documentation.

class PixelExpectations(GpuTestExpectations):
  def SetExpectations(self):
    # Sample Usage:
    # self.Fail('Pixel.Canvas2DRedBox',
    #     ['mac', 'amd', ('nvidia', 0x1234)], bug=123)
    self.Fail('Pixel.Canvas2DRedBox', bug=485183)
    self.Fail('Pixel.CSS3DBlueBox', bug=485183)
    self.Fail('Pixel.WebGLGreenTriangle', bug=485183)
