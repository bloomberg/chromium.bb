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

    # Seems to be flaky on the new AMD R7 240 drivers.
    self.Flaky('Pixel_GpuRasterization_BlueBox',
        ['win', ('amd', 0x6613)], bug=653538)

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

    # TODO(ccameron): Re-enable this after baseline. This is caused by changing
    # the way we interpret BT709 color spaces.
    self.Fail('Pixel_Video_VP9', bug=763260)
    self.Fail('Pixel_Video_MP4', bug=763260)

    # TODO(vmiura) check / generate reference images for Android devices
    self.Fail('Pixel_SolidColorBackground', ['mac', 'android'], bug=624256)

    self.Fail('Pixel_OffscreenCanvasUnaccelerated2DGPUCompositingWorker',
        ['mac', ('nvidia', 0xfe9)], bug=706016)
    self.Fail('Pixel_CSSFilterEffects',
        ['mac', ('nvidia', 0xfe9)], bug=690277)

    # TODO(kbr): flakily timing out on this configuration.
    self.Flaky('*', ['linux', 'intel', 'debug'], bug=648369)

    self.Flaky('Pixel_Video_MP4', ['android', 'nvidia'], bug=716564)

    # Flaky for unknown reasons only on macOS. Not planning to investigate
    # further.
    self.Flaky('Pixel_ScissorTestWithPreserveDrawingBuffer', ['mac'],
               bug=660461)

    self.Flaky('Pixel_OffscreenCanvas2DResizeOnWorker',
        ['win10', ('intel', 0x1912)], bug=690663)

    self.Flaky('Pixel_OffscreenCanvasTransferBeforeStyleResize',
              ['mac', 'linux', 'win', 'android'], bug=735228)
    self.Flaky('Pixel_OffscreenCanvasTransferAfterStyleResize',
              ['mac', 'linux', 'win', 'android'], bug=735171)

    self.Flaky('Pixel_OffscreenCanvasWebGLSoftwareCompositingWorker',
        ['mac', ('nvidia', 0xfe9), 'debug'], bug=751328)
