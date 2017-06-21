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

    self.Flaky('Pixel_Video_MP4', ['android', 'nvidia'], bug=716564)

    # Flaky for unknown reasons only on macOS. Not planning to investigate
    # further.
    self.Flaky('Pixel_ScissorTestWithPreserveDrawingBuffer', ['mac'],
               bug=660461)

    self.Flaky('Pixel_OffscreenCanvas2DResizeOnWorker',
        ['win10', ('intel', 0x1912)], bug=690663)

    # TODO(zakerinasab): check / generate reference images.
    self.Fail('Pixel_Canvas2DUntagged', bug=713632)

    # TODO(fmalita): remove when new baselines are available.
    self.Fail('Pixel_CSSFilterEffects', bug=731693)
    self.Fail('Pixel_CSSFilterEffects_NoOverlays', bug=731693)

    # Failures on Haswell GPUs on macOS after upgrade to 10.12.4.
    self.Fail('Pixel_OffscreenCanvas2DResizeOnWorker',
              ['mac', ('intel', 0x0a2e)], bug=718183)
    self.Fail('Pixel_OffscreenCanvasAccelerated2D',
              ['mac', ('intel', 0x0a2e)], bug=718183)
    self.Fail('Pixel_OffscreenCanvasAccelerated2DWorker',
              ['mac', ('intel', 0x0a2e)], bug=718183)
    self.Fail('Pixel_WebGLGreenTriangle_NonChromiumImage_NoAA_NoAlpha',
              ['mac', ('intel', 0x0a2e)], bug=718183)

    self.Flaky('Pixel_OffscreenCanvasTransferBeforeStyleResize',
              ['mac', 'linux', 'win', 'android'], bug=735228)
    self.Flaky('Pixel_OffscreenCanvasTransferAfterStyleResize',
              ['mac', 'linux', 'win', 'android'], bug=735171)
