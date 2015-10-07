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

    self.Fail('Pixel.ScissorTestWithPreserveDrawingBuffer',
        ['android'], bug=521588)

    self.Fail('Pixel.ScissorTestWithPreserveDrawingBufferES3',
              ['mac'], bug=540039)

    # TODO(kbr): remove these failure expectations once reference
    # images are generated.
    self.Fail('Pixel.Canvas2DRedBoxES3',
              ['mac'], bug=534114)
    self.Fail('Pixel.CSS3DBlueBoxES3',
              ['mac'], bug=534114)
    self.Fail('Pixel.WebGLGreenTriangleES3',
              ['mac'], bug=534114)
