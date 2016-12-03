# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

class PixelTestPage(object):
  """A wrapper class mimicking the functionality of the PixelTestsStorySet
  from the old-style GPU tests.
  """
  def __init__(self, url, name, test_rect, revision,
               expected_colors=None, tolerance=2, browser_args=None):
    super(PixelTestPage, self).__init__()
    self.url = url
    self.name = name
    self.test_rect = test_rect
    self.revision = revision
    # The expected colors can be specified as a list of dictionaries,
    # in which case these specific pixels will be sampled instead of
    # comparing the entire image snapshot. The format is only defined
    # by contract with _CompareScreenshotSamples in
    # cloud_storage_integration_test_base.py.
    self.expected_colors = expected_colors
    # The tolerance when comparing against the reference image.
    self.tolerance = tolerance
    self.browser_args = browser_args

  def CopyWithNewBrowserArgsAndSuffix(self, browser_args, suffix):
    return PixelTestPage(
      self.url, self.name + suffix, self.test_rect, self.revision,
      self.expected_colors, self.tolerance, browser_args)

  def CopyWithNewBrowserArgsAndPrefix(self, browser_args, prefix):
    # Assuming the test name is 'Pixel'.
    split = self.name.split('_', 1)
    return PixelTestPage(
      self.url, split[0] + '_' + prefix + split[1], self.test_rect,
      self.revision, self.expected_colors, self.tolerance, browser_args)


def CopyPagesWithNewBrowserArgsAndSuffix(pages, browser_args, suffix):
  return [
    p.CopyWithNewBrowserArgsAndSuffix(browser_args, suffix) for p in pages]


def CopyPagesWithNewBrowserArgsAndPrefix(pages, browser_args, prefix):
  return [
    p.CopyWithNewBrowserArgsAndPrefix(browser_args, prefix) for p in pages]


def DefaultPages(base_name):
  return [
    PixelTestPage(
      'pixel_canvas2d.html',
      base_name + '_Canvas2DRedBox',
      test_rect=[0, 0, 300, 300],
      revision=7),

    PixelTestPage(
      'pixel_css3d.html',
      base_name + '_CSS3DBlueBox',
      test_rect=[0, 0, 300, 300],
      revision=15),

    PixelTestPage(
      'pixel_webgl_aa_alpha.html',
      base_name + '_WebGLGreenTriangle_AA_Alpha',
      test_rect=[0, 0, 300, 300],
      revision=1),

    PixelTestPage(
      'pixel_webgl_noaa_alpha.html',
      base_name + '_WebGLGreenTriangle_NoAA_Alpha',
      test_rect=[0, 0, 300, 300],
      revision=1),

    PixelTestPage(
      'pixel_webgl_aa_noalpha.html',
      base_name + '_WebGLGreenTriangle_AA_NoAlpha',
      test_rect=[0, 0, 300, 300],
      revision=1),

    PixelTestPage(
      'pixel_webgl_noaa_noalpha.html',
      base_name + '_WebGLGreenTriangle_NoAA_NoAlpha',
      test_rect=[0, 0, 300, 300],
      revision=1),

    PixelTestPage(
      'pixel_webgl_noalpha_implicit_clear.html',
      base_name + '_WebGLTransparentGreenTriangle_NoAlpha_ImplicitClear',
      test_rect=[0, 0, 300, 300],
      revision=1),

    PixelTestPage(
      'pixel_scissor.html',
      base_name + '_ScissorTestWithPreserveDrawingBuffer',
      test_rect=[0, 0, 300, 300],
      revision=0, # This is not used.
      expected_colors=[
        {
          'comment': 'red top',
          'location': [1, 1],
          'size': [198, 188],
          'color': [255, 0, 0],
          'tolerance': 3
        },
        {
          'comment': 'green bottom left',
          'location': [1, 191],
          'size': [8, 8],
          'color': [0, 255, 0],
          'tolerance': 3
        },
        {
          'comment': 'red bottom right',
          'location': [11, 191],
          'size': [188, 8],
          'color': [255, 0, 0],
          'tolerance': 3
        }
      ]),

    PixelTestPage(
      'pixel_canvas2d_webgl.html',
      base_name + '_2DCanvasWebGL',
      test_rect=[0, 0, 300, 300],
      revision=3),

    PixelTestPage(
      'pixel_background.html',
      base_name + '_SolidColorBackground',
      test_rect=[500, 500, 100, 100],
      revision=1),
  ]


# Pages that should be run with experimental canvas features.
def ExperimentalCanvasFeaturesPages(base_name):
  browser_args = ['--enable-experimental-canvas-features']
  unaccelerated_args = [
    '--disable-accelerated-2d-canvas',
    '--disable-gpu-compositing']

  return [
    PixelTestPage(
      'pixel_offscreenCanvas_transferToImageBitmap_main.html',
      base_name + '_OffscreenCanvasTransferToImageBitmap',
      test_rect=[0, 0, 300, 300],
      revision=1,
      browser_args=browser_args),

    PixelTestPage(
      'pixel_offscreenCanvas_transferToImageBitmap_worker.html',
      base_name + '_OffscreenCanvasTransferToImageBitmapWorker',
      test_rect=[0, 0, 300, 300],
      revision=1,
      browser_args=browser_args),

    PixelTestPage(
      'pixel_offscreenCanvas_webgl_commit_main.html',
      base_name + '_OffscreenCanvasWebGLDefault',
      test_rect=[0, 0, 350, 350],
      revision=1,
      browser_args=browser_args),

    PixelTestPage(
      'pixel_offscreenCanvas_webgl_commit_worker.html',
      base_name + '_OffscreenCanvasWebGLDefaultWorker',
      test_rect=[0, 0, 350, 350],
      revision=1,
      browser_args=browser_args),

    PixelTestPage(
      'pixel_offscreenCanvas_webgl_commit_main.html',
      base_name + '_OffscreenCanvasWebGLSoftwareCompositing',
      test_rect=[0, 0, 350, 350],
      revision=2,
      browser_args=browser_args + ['--disable-gpu-compositing']),

    PixelTestPage(
      'pixel_offscreenCanvas_webgl_commit_worker.html',
      base_name + '_OffscreenCanvasWebGLSoftwareCompositingWorker',
      test_rect=[0, 0, 350, 350],
      revision=2,
      browser_args=browser_args + ['--disable-gpu-compositing']),

    PixelTestPage(
      'pixel_offscreenCanvas_2d_commit_main.html',
      base_name + '_OffscreenCanvasAccelerated2D',
      test_rect=[0, 0, 300, 300],
      revision=2,
      browser_args=browser_args),

    PixelTestPage(
      'pixel_offscreenCanvas_2d_commit_worker.html',
      base_name + '_OffscreenCanvasAccelerated2DWorker',
      test_rect=[0, 0, 300, 300],
      revision=2,
      browser_args=browser_args),

    PixelTestPage(
      'pixel_offscreenCanvas_2d_commit_main.html',
      base_name + '_OffscreenCanvasUnaccelerated2D',
      test_rect=[0, 0, 300, 300],
      revision=2,
      browser_args=browser_args + unaccelerated_args),

    PixelTestPage(
      'pixel_offscreenCanvas_2d_commit_worker.html',
      base_name + '_OffscreenCanvasUnaccelerated2DWorker',
      test_rect=[0, 0, 300, 300],
      revision=2,
      browser_args=browser_args + unaccelerated_args),

    PixelTestPage(
      'pixel_offscreenCanvas_2d_commit_main.html',
      base_name + '_OffscreenCanvasUnaccelerated2DGPUCompositing',
      test_rect=[0, 0, 300, 300],
      revision=4,
      browser_args=browser_args + ['--disable-accelerated-2d-canvas']),

    PixelTestPage(
      'pixel_offscreenCanvas_2d_commit_worker.html',
      base_name + '_OffscreenCanvasUnaccelerated2DGPUCompositingWorker',
      test_rect=[0, 0, 300, 300],
      revision=4,
      browser_args=browser_args + ['--disable-accelerated-2d-canvas']),

    PixelTestPage(
      'pixel_canvas_display_linear-rgb.html',
      base_name + '_CanvasDisplayLinearRGBAccelerated2D',
      test_rect=[0, 0, 140, 140],
      revision=1,
      browser_args=browser_args),

    PixelTestPage(
      'pixel_canvas_display_linear-rgb.html',
      base_name + '_CanvasDisplayLinearRGBUnaccelerated2D',
      test_rect=[0, 0, 140, 140],
      revision=1,
      browser_args=browser_args + unaccelerated_args),

    PixelTestPage(
      'pixel_canvas_display_linear-rgb.html',
      base_name + '_CanvasDisplayLinearRGBUnaccelerated2DGPUCompositing',
      test_rect=[0, 0, 140, 140],
      revision=1,
      browser_args=browser_args + ['--disable-accelerated-2d-canvas']),
]


# Pages that should be run with various macOS specific command line
# arguments.
def MacSpecificPages(base_name):
  iosurface_2d_canvas_args = [
    '--enable-accelerated-2d-canvas',
    '--disable-display-list-2d-canvas']

  non_chromium_image_args = ['--disable-webgl-image-chromium']

  return [
    # On macOS, test the IOSurface 2D Canvas compositing path.
    PixelTestPage(
      'pixel_canvas2d_accelerated.html',
      base_name + '_IOSurface2DCanvas',
      test_rect=[0, 0, 400, 400],
      revision=1,
      browser_args=iosurface_2d_canvas_args),
    PixelTestPage(
      'pixel_canvas2d_webgl.html',
      base_name + '_IOSurface2DCanvasWebGL',
      test_rect=[0, 0, 300, 300],
      revision=2,
      browser_args=iosurface_2d_canvas_args),

    # On macOS, test WebGL non-Chromium Image compositing path.
    PixelTestPage(
      'pixel_webgl_aa_alpha.html',
      base_name + '_WebGLGreenTriangle_NonChromiumImage_AA_Alpha',
      test_rect=[0, 0, 300, 300],
      revision=1,
      browser_args=non_chromium_image_args),
    PixelTestPage(
      'pixel_webgl_noaa_alpha.html',
      base_name + '_WebGLGreenTriangle_NonChromiumImage_NoAA_Alpha',
      test_rect=[0, 0, 300, 300],
      revision=1,
      browser_args=non_chromium_image_args),
    PixelTestPage(
      'pixel_webgl_aa_noalpha.html',
      base_name + '_WebGLGreenTriangle_NonChromiumImage_AA_NoAlpha',
      test_rect=[0, 0, 300, 300],
      revision=1,
      browser_args=non_chromium_image_args),
    PixelTestPage(
      'pixel_webgl_noaa_noalpha.html',
      base_name + '_WebGLGreenTriangle_NonChromiumImage_NoAA_NoAlpha',
      test_rect=[0, 0, 300, 300],
      revision=1,
      browser_args=non_chromium_image_args),

    # On macOS, test CSS filter effects with and without the CA compositor.
    PixelTestPage(
      'filter_effects.html',
      base_name + '_CSSFilterEffects',
      test_rect=[0, 0, 300, 300],
      revision=3),
    PixelTestPage(
      'filter_effects.html',
      base_name + '_CSSFilterEffects_NoOverlays',
      test_rect=[0, 0, 300, 300],
      revision=3,
      tolerance=10,
      browser_args=['--disable-mac-overlays']),
  ]
