# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from gpu_tests.gpu_test_expectations import GpuTestExpectations

# See the GpuTestExpectations class for documentation.

class PixelExpectations(GpuTestExpectations):
  def SetExpectations(self):
    # Sample Usage:
    # self.Fail('Pixel_Canvas2DRedBox',
    #     ['mac', 'amd', ('nvidia', 0x1234)], bug=123)

    # Software compositing is not supported on Android; so we skip these tests
    # that disables gpu compositing on Android platforms.
    self.Skip('Pixel_OffscreenCanvasUnaccelerated2D', ['android'])
    self.Skip('Pixel_OffscreenCanvasUnaccelerated2DWorker', ['android'])
    self.Skip('Pixel_OffscreenCanvasWebGLSoftwareCompositing', ['android'])
    self.Skip('Pixel_OffscreenCanvasWebGLSoftwareCompositingWorker',
              ['android'])
    self.Skip('Pixel_CanvasDisplayLinearRGBUnaccelerated2D', ['android'])

    self.Fail('Pixel_ScissorTestWithPreserveDrawingBuffer',
        ['android'], bug=521588)

    # TODO(ccameron) fix these on Mac Retina
    self.Fail('Pixel_CSS3DBlueBox', ['mac'], bug=533690)

    # TODO(vmiura) check / generate reference images for Android devices
    self.Fail('Pixel_SolidColorBackground', ['mac', 'android'], bug=624256)

    self.Fail('Pixel_OffscreenCanvasUnaccelerated2DGPUCompositingWorker',
              ['mac', ('nvidia', 0xfe9)], bug=652931)

    # TODO(kbr): flakily timing out on this configuration.
    self.Flaky('*', ['linux', 'intel', 'debug'], bug=648369)

    # Flaky for unknown reasons only on macOS. Not planning to investigate
    # further.
    self.Flaky('Pixel_ScissorTestWithPreserveDrawingBuffer', ['mac'],
               bug=660461)
