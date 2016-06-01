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

    # TODO(erikchen): Remove suppression after generating reference images.
    self.Fail('Pixel.WebGLGreenTriangle.NonChromiumImage.AA.Alpha',
              ['mac'], bug=607130)
    self.Fail('Pixel.WebGLGreenTriangle.NonChromiumImage.AA.NoAlpha',
              ['mac'], bug=607130)
    self.Fail('Pixel.WebGLGreenTriangle.NonChromiumImage.NoAA.Alpha',
              ['mac'], bug=607130)
    self.Fail('Pixel.WebGLGreenTriangle.NonChromiumImage.NoAA.NoAlpha',
              ['mac'], bug=607130)
    self.Fail('Pixel.WebGLGreenTriangle.AA.Alpha', bug=607130)
    self.Fail('Pixel.WebGLGreenTriangle.AA.NoAlpha', bug=607130)
    self.Fail('Pixel.WebGLGreenTriangle.NoAA.Alpha', bug=607130)
    self.Fail('Pixel.WebGLGreenTriangle.NoAA.NoAlpha', bug=607130)
    self.Fail('Pixel.WebGLGreenTriangle.AA.AlphaES3', bug=607130)
    self.Fail('Pixel.WebGLGreenTriangle.AA.NoAlphaES3', bug=607130)
    self.Fail('Pixel.WebGLGreenTriangle.NoAA.AlphaES3', bug=607130)
    self.Fail('Pixel.WebGLGreenTriangle.NoAA.NoAlphaES3', bug=607130)

    self.Fail('Pixel.ScissorTestWithPreserveDrawingBufferES3',
              ['mac'], bug=540039)
    self.Fail('Pixel.WebGLGreenTriangleES3',
              ['mac', ('intel', 0x116)], bug=540531)

    # TODO(ccameron): Remove suppression after rebaseline.
    self.Fail('Pixel.CSS3DBlueBox', ['mac'], bug=533690)
    self.Fail('Pixel.CSS3DBlueBoxES3', ['mac'], bug=533690)
