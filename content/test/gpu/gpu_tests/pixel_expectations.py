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

    # Software compositing is not supported on Android: we skip the tests that
    # disable GPU compositing (--disable-gpu-compositing).
    self.Skip('Pixel_OffscreenCanvasUnaccelerated2D', ['android'])
    self.Skip('Pixel_OffscreenCanvasUnaccelerated2DWorker', ['android'])
    self.Skip('Pixel_OffscreenCanvasUnaccelerated2DGPUCompositing',
              ['android'])
    self.Skip('Pixel_OffscreenCanvasUnaccelerated2DGPUCompositingWorker',
              ['android'])
    self.Skip('Pixel_OffscreenCanvasWebGLSoftwareCompositing', ['android'])
    self.Skip('Pixel_OffscreenCanvasWebGLSoftwareCompositingWorker',
              ['android'])
    self.Skip('Pixel_CanvasDisplayLinearRGBUnaccelerated2D', ['android'])
    self.Skip('Pixel_CanvasUnacceleratedLowLatency2D', ['android'])
    self.Skip('Pixel_RepeatedWebGLTo2D_SoftwareCompositing', ['android'])

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

    # TODO(vmiura) check / generate reference images for Android devices
    self.Fail('Pixel_SolidColorBackground', ['mac', 'android'], bug=624256)

    self.Fail('Pixel_CSSFilterEffects', ['mac', ('nvidia', 0xfe9)], bug=690277)

    # Became flaky on 10.13.6. When it flakes, it flakes 3 times, so
    # mark failing, unfortunately.
    self.Fail('Pixel_CSSFilterEffects', ['highsierra', 'amd'], bug=872423)

    # TODO(kbr): flakily timing out on this configuration.
    self.Flaky('*', ['linux', 'intel', 'debug'], bug=648369)

    # Flaky for unknown reasons only on macOS. Not planning to investigate
    # further.
    self.Flaky('Pixel_ScissorTestWithPreserveDrawingBuffer', ['mac'],
               bug=660461)

    self.Flaky('Pixel_OffscreenCanvasWebGLSoftwareCompositingWorker',
        ['mac', ('nvidia', 0xfe9), 'debug'], bug=751328)

    # Failing on Nexus 5; haven't investigated why yet.
    self.Skip('Pixel_WebGL2_BlitFramebuffer_Result_Displayed',
        ['android', ('qualcomm', 'Adreno (TM) 330')], bug=773293)
    self.Skip('Pixel_WebGL2_ClearBufferfv_Result_Displayed',
        ['android', ('qualcomm', 'Adreno (TM) 330')], bug=773293)

    self.Fail('Pixel_WebGLGreenTriangle_NonChromiumImage_NoAA_NoAlpha',
        ['highsierra', 'mojave', ('intel', 0xa2e)], bug=774809)
    self.Flaky('Pixel_OffscreenCanvasTransferBeforeStyleResize',
        ['highsierra', ('intel', 0xa2e)], bug=857578)

    # Failing on NVIDIA Shield TV; not sure why yet.
    self.Fail('Pixel_WebGL_PremultipliedAlpha_False',
              ['android', 'nvidia'], bug=791733)

    # Failing on retina Macs
    self.Fail('Pixel_Canvas2DRedBox_NoGpuProcess',
              ['mac', ('amd', 0x6821)], bug=744658)
    self.Fail('Pixel_Canvas2DRedBox_NoGpuProcess',
              ['mac', ('nvidia', 0xfe9)], bug=744658)
    self.Fail('Pixel_CSS3DBlueBox_NoGpuProcess',
              ['mac', ('amd', 0x6821)], bug=744658)
    self.Fail('Pixel_CSS3DBlueBox_NoGpuProcess',
              ['mac', ('nvidia', 0xfe9)], bug=744658)

    # TODO(fserb): temporarily suppress this test.
    self.Flaky('Pixel_OffscreenCanvas2DResizeOnWorker',
        ['linux', 'mac'], bug=840394)

    # TODO(kbr): temporary suppression for new test.
    self.Flaky('Pixel_WebGLSadCanvas', ['linux', 'win'], bug=575305)
    self.Fail('Pixel_WebGLSadCanvas', ['mac'], bug=872423)
    self.Fail('Pixel_WebGLSadCanvas', ['android'], bug=575305)

    self.Fail('Pixel_CanvasLowLatencyWebGL', ['android', 'nvidia'], bug=868596)
    self.Fail('Pixel_OffscreenCanvasWebGLPaintAfterResize',
              ['android', 'nvidia'], bug=868596)

    # Fails on Nexus 5, 6 and 6P
    self.Fail('Pixel_BackgroundImage',
        ['android', ('qualcomm', 'Adreno (TM) 330')], bug=883500)
    self.Fail('Pixel_BackgroundImage',
        ['android', ('qualcomm', 'Adreno (TM) 420')], bug=883500)
    self.Fail('Pixel_BackgroundImage',
        ['android', ('qualcomm', 'Adreno (TM) 430')], bug=883500)

    # Flakes on Nexus 5X.
    self.Flaky('Pixel_BackgroundImage',
        ['android', ('qualcomm', 'Adreno (TM) 418')], bug=883500)

    # We do not have software H.264 decoding on Android, so it can't survive a
    # context loss which results in hardware decoder loss.
    self.Skip('Pixel_Video_Context_Loss_MP4', ['android'], bug=580386)

    # Fails on Mac Pro FYI Release (AMD)
    self.Fail('Pixel_Video_MP4',
        ['mac', ('amd', 0x679e)], bug=925744)
    self.Fail('Pixel_Video_Context_Loss_MP4',
        ['mac', ('amd', 0x679e)], bug=925744)
    self.Fail('Pixel_Video_MP4_FourColors_Aspect_4x3',
        ['mac', ('amd', 0x679e)], bug=911413)
    self.Fail('Pixel_Video_MP4_FourColors_Rot_90',
        ['mac', ('amd', 0x679e)], bug=911413)
    self.Fail('Pixel_Video_MP4_FourColors_Rot_180',
        ['mac', ('amd', 0x679e)], bug=911413)
    self.Fail('Pixel_Video_MP4_FourColors_Rot_270',
        ['mac', ('amd', 0x679e)], bug=911413)

    # Fails on multiple Android devices.
    self.Fail('Pixel_CSS3DBlueBox', ['android'], bug=927107)

    # Fail on Nexus 5, 5X, 6, 6P, 9 and Shield TV.
    self.Fail('Pixel_Video_MP4', ['android'], bug=925744)
    self.Fail('Pixel_Video_MP4_FourColors_Aspect_4x3', ['android'], bug=925744)
    self.Fail('Pixel_Video_MP4_FourColors_Rot_180', ['android'], bug=925744)
    self.Fail('Pixel_Video_MP4_FourColors_Rot_270', ['android'], bug=925744)
    self.Fail('Pixel_Video_MP4_FourColors_Rot_90', ['android'], bug=925744)
    self.Fail('Pixel_Video_VP9', ['android'], bug=925744)
    self.Fail('Pixel_Video_Context_Loss_VP9', ['android'], bug=925744)

    # Skip on platforms where DXVA vs D3D11 decoder doesn't matter.
    self.Skip('Pixel_Video_MP4_DXVA', ['linux', 'android', 'mac', 'chromeos'],
              bug=927901)
    self.Skip('Pixel_Video_VP9_DXVA', ['linux', 'android', 'mac', 'chromeos'],
              bug=927901)

    # Complex overlays test is flaky on Nvidia probably due to its small size.
    self.Flaky('Pixel_DirectComposition_ComplexOverlays', ['win', 'nvidia'],
               bug=929425)
