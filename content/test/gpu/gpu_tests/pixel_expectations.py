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

    # TODO(vmiura) check / generate reference images for Android devices
    self.Fail('Pixel_SolidColorBackground', ['mac', 'android'], bug=624256)

    self.Fail('Pixel_OffscreenCanvasUnaccelerated2DGPUCompositingWorker',
        ['mac', ('nvidia', 0xfe9)], bug=706016)
    self.Fail('Pixel_CSSFilterEffects',
        ['mac', ('nvidia', 0xfe9)], bug=690277)

    # TODO(kbr): flakily timing out on this configuration.
    self.Flaky('*', ['linux', 'intel', 'debug'], bug=648369)

    # Flaky for unknown reasons only on macOS. Not planning to investigate
    # further.
    self.Flaky('Pixel_ScissorTestWithPreserveDrawingBuffer', ['mac'],
               bug=660461)

    self.Flaky('Pixel_OffscreenCanvas2DResizeOnWorker',
        ['win10', ('intel', 0x1912)], bug=690663)

    # TODO(jbauman): Re-enable when references images created.
    self.Fail('Pixel_DirectComposition_Video_*', ['win'], bug=704389)

    # TODO(xlai): Remove this after verifying reference images.
    self.Fail('Pixel_OffscreenCanvasAccelerated2D', bug=706647)
    self.Fail('Pixel_OffscreenCanvasAccelerated2DWorker', bug=706647)
    self.Fail('Pixel_OffscreenCanvasUnaccelerated2D', bug=706647)
    self.Fail('Pixel_OffscreenCanvasUnaccelerated2DWorker', bug=706647)
    self.Fail('Pixel_OffscreenCanvasUnaccelerated2DGPUCompositing',
        bug=706647)
    self.Fail('Pixel_OffscreenCanvasUnaccelerated2DGPUCompositingWorker',
        bug=706647)
    self.Fail('Pixel_OffscreenCanvasWebGLDefault', bug=706647)
    self.Fail('Pixel_OffscreenCanvasWebGLDefaultWorker', bug=706647)
    self.Fail('Pixel_OffscreenCanvasWebGLSoftwareCompositing', bug=706647)
    self.Fail('Pixel_OffscreenCanvasWebGLSoftwareCompositingWorker',
        bug=706647)
