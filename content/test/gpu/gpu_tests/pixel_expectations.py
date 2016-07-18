# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from gpu_tests.gpu_test_expectations import GpuTestExpectations

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
    self.Fail('Pixel.WebGLGreenTriangleES3',
              ['mac', ('intel', 0x116)], bug=540531)

    # TODO(ccameron) fix these on Mac Retina
    self.Fail('Pixel.CSS3DBlueBox', ['mac'], bug=533690)
    self.Fail('Pixel.CSS3DBlueBoxES3', ['mac'], bug=533690)

    # TODO(vmiura) check / generate reference images for Android devices
    self.Fail('Pixel.SolidColorBackground', ['mac', 'android'], bug=624256)

    # TODO(erikchen) check / generate reference images.
    self.Fail('Pixel.CSSFilterEffects', ['mac'], bug=581526)
    self.Fail('Pixel.CSSFilterEffects.NoOverlays', ['mac'], bug=581526)
