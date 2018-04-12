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

    # Tests running with SwiftShader are skipped on platforms where SwiftShader
    # isn't supported.
    self.Skip('Pixel_Canvas2DRedBox_SwiftShader',
        ['mac', 'android', 'chromeos'])
    self.Skip('Pixel_CSS3DBlueBox_SwiftShader',
        ['mac', 'android', 'chromeos'])
    self.Skip('Pixel_WebGLGreenTriangle_AA_Alpha_SwiftShader',
        ['mac', 'android', 'chromeos'])
    # Tests running in no GPU process mode are skipped on platforms where GPU
    # process is required.
    self.Skip('Pixel_Canvas2DRedBox_NoGpuProcess', ['android', 'chromeos'])
    self.Skip('Pixel_CSS3DBlueBox_NoGpuProcess', ['android', 'chromeos'])

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
    self.Fail('Pixel_Video_MP4',
        ['android', ('qualcomm', 'Adreno (TM) 418')], bug=820240)
    self.Flaky('Pixel_Video_MP4', ['linux', 'nvidia'], bug=819635)

    # TODO(junov); validate new test results
    self.Fail('Pixel_CanvasLowLatency2D',
        ['mac', 'linux', 'win', 'android', 'chromeos'], bug=788439)
    self.Fail('Pixel_CanvasUnacceleratedLowLatency2D',
        ['mac', 'linux', 'win', 'android', 'chromeos'], bug=788439)

    # Rebaseline Pixel_CSS3DBlueBox
    self.Fail('Pixel_CSS3DBlueBox', bug=796558)

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
    self.Flaky('Pixel_OffscreenCanvasTransferToImageBitmap',
              ['linux', 'win', 'android'], bug=807742)

    self.Flaky('Pixel_OffscreenCanvasWebGLSoftwareCompositingWorker',
        ['mac', ('nvidia', 0xfe9), 'debug'], bug=751328)

    # Failing on Nexus 5; haven't investigated why yet.
    self.Skip('Pixel_WebGL2_BlitFramebuffer_Result_Displayed',
        ['android', ('qualcomm', 'Adreno (TM) 330')], bug=773293)
    self.Skip('Pixel_WebGL2_ClearBufferfv_Result_Displayed',
        ['android', ('qualcomm', 'Adreno (TM) 330')], bug=773293)

    # Failing on Mac Intel HighSierra
    self.Fail('Pixel_Video_MP4',
        ['highsierra', ('intel', 0xa2e)], bug=774809)
    self.Fail('Pixel_Video_VP9',
        ['highsierra', ('intel', 0xa2e)], bug=774809)
    self.Fail('Pixel_WebGLGreenTriangle_NonChromiumImage_NoAA_NoAlpha',
        ['highsierra', ('intel', 0xa2e)], bug=774809)

    # Failing on NVIDIA Shield TV; not sure why yet.
    self.Fail('Pixel_WebGL_PremultipliedAlpha_False',
              ['android', 'nvidia'], bug=791733)

    # TODO(zmo): temporarily suppress these two tests until new
    # reference images with new names are generated.
    self.Fail('Pixel_Canvas2DRedBox_NoGpuProcess',
              ['linux', 'mac', 'win'], bug=744658)
    self.Fail('Pixel_CSS3DBlueBox_NoGpuProcess',
              ['linux', 'mac', 'win'], bug=744658)

    # TODO(hubbe): Temporary supressions for rebaseline
    self.Fail('Pixel_Video_VP9', bug=754986)
    self.Fail('Pixel_DirectComposition_Video_VP9', bug=754986)

    # TODO(kbr): temporary suppression for new test.
    self.Fail('Pixel_WebGLSadCanvas', bug=575305)
