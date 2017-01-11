# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

class PixelTestPage(object):
  """A wrapper class mimicking the functionality of the PixelTestsStorySet
  from the old-style GPU tests.
  """
  def __init__(self, url, name, test_rect, revision,
               tolerance=2, browser_args=None, expected_colors=None):
    super(PixelTestPage, self).__init__()
    self.url = url
    self.name = name
    self.test_rect = test_rect
    self.revision = revision
    # The tolerance when comparing against the reference image.
    self.tolerance = tolerance
    self.browser_args = browser_args
    # The expected colors can be specified as a list of dictionaries,
    # in which case these specific pixels will be sampled instead of
    # comparing the entire image snapshot. The format is only defined
    # by contract with _CompareScreenshotSamples in
    # cloud_storage_integration_test_base.py.
    self.expected_colors = expected_colors

  def CopyWithNewBrowserArgsAndSuffix(self, browser_args, suffix):
    return PixelTestPage(
      self.url, self.name + suffix, self.test_rect, self.revision,
      self.tolerance, browser_args, self.expected_colors)

  def CopyWithNewBrowserArgsAndPrefix(self, browser_args, prefix):
    # Assuming the test name is 'Pixel'.
    split = self.name.split('_', 1)
    return PixelTestPage(
      self.url, split[0] + '_' + prefix + split[1], self.test_rect,
      self.revision, self.tolerance, browser_args, self.expected_colors)


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
      revision=16),

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


# Pages that should be run with GPU rasterization enabled.
def GpuRasterizationPages(base_name):
  browser_args = ['--force-gpu-rasterization']
  return [
    PixelTestPage(
      'pixel_background.html',
      base_name + '_GpuRasterization_BlueBox',
      test_rect=[0, 0, 220, 220],
      revision=0, # This is not used.
      browser_args=browser_args,
      expected_colors=[
        {
          'comment': 'body-t',
          'location': [5, 5],
          'size': [1, 1],
          'color': [0, 128, 0],
          'tolerance': 0
        },
        {
          'comment': 'body-r',
          'location': [215, 5],
          'size': [1, 1],
          'color': [0, 128, 0],
          'tolerance': 0
        },
        {
          'comment': 'body-b',
          'location': [215, 215],
          'size': [1, 1],
          'color': [0, 128, 0],
          'tolerance': 0
        },
        {
          'comment': 'body-l',
          'location': [5, 215],
          'size': [1, 1],
          'color': [0, 128, 0],
          'tolerance': 0
        },
        {
          'comment': 'background-t',
          'location': [30, 30],
          'size': [1, 1],
          'color': [0, 0, 0],
          'tolerance': 0
        },
        {
          'comment': 'background-r',
          'location': [170, 30],
          'size': [1, 1],
          'color': [0, 0, 0],
          'tolerance': 0
        },
        {
          'comment': 'background-b',
          'location': [170, 170],
          'size': [1, 1],
          'color': [0, 0, 0],
          'tolerance': 0
        },
        {
          'comment': 'background-l',
          'location': [30, 170],
          'size': [1, 1],
          'color': [0, 0, 0],
          'tolerance': 0
        },
        {
          'comment': 'box-t',
          'location': [70, 70],
          'size': [1, 1],
          'color': [0, 0, 255],
          'tolerance': 0
        },
        {
          'comment': 'box-r',
          'location': [140, 70],
          'size': [1, 1],
          'color': [0, 0, 255],
          'tolerance': 0
        },
        {
          'comment': 'box-b',
          'location': [140, 140],
          'size': [1, 1],
          'color': [0, 0, 255],
          'tolerance': 0
        },
        {
          'comment': 'box-l',
          'location': [70, 140],
          'size': [1, 1],
          'color': [0, 0, 255],
          'tolerance': 0
        }
      ]),
    PixelTestPage(
      'concave_paths.html',
      base_name + '_GpuRasterization_ConcavePaths',
      test_rect=[0, 0, 100, 100],
      revision=0, # This is not used.
      browser_args=browser_args,
      expected_colors=[
        {
          'comment': 'outside',
          'location': [80, 60],
          'size': [1, 1],
          'color': [255, 255, 255],
          'tolerance': 0
        },
        {
          'comment': 'outside',
          'location': [28, 20],
          'size': [1, 1],
          'color': [255, 255, 255],
          'tolerance': 0
        },
        {
          'comment': 'inside',
          'location': [32, 25],
          'size': [1, 1],
          'color': [255, 215, 0],
          'tolerance': 0
        },
        {
          'comment': 'inside',
          'location': [80, 80],
          'size': [1, 1],
          'color': [255, 215, 0],
          'tolerance': 0
        }
      ])
  ]


# Pages that should be run with experimental canvas features.
def ExperimentalCanvasFeaturesPages(base_name):
  browser_args = ['--enable-experimental-canvas-features']
  unaccelerated_args = [
    '--disable-accelerated-2d-canvas',
    '--disable-gpu-compositing']

  return [
    PixelTestPage(
      'pixel_offscreenCanvas_transfer_after_style_resize.html',
      base_name + '_OffscreenCanvasTransferAfterStyleResize',
      test_rect=[0, 0, 350, 350],
      revision=1,
      browser_args=browser_args),

    PixelTestPage(
      'pixel_offscreenCanvas_transfer_before_style_resize.html',
      base_name + '_OffscreenCanvasTransferBeforeStyleResize',
      test_rect=[0, 0, 350, 350],
      revision=1,
      browser_args=browser_args),

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
      revision=3,
      browser_args=browser_args),

    PixelTestPage(
      'pixel_offscreenCanvas_2d_commit_worker.html',
      base_name + '_OffscreenCanvasAccelerated2DWorker',
      test_rect=[0, 0, 300, 300],
      revision=3,
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
      'pixel_offscreenCanvas_2d_resize_on_worker.html',
      base_name + '_OffscreenCanvas2DResizeOnWorker',
      test_rect=[0, 0, 200, 200],
      revision=2,
      browser_args=browser_args),

    PixelTestPage(
      'pixel_offscreenCanvas_webgl_resize_on_worker.html',
      base_name + '_OffscreenCanvasWebglResizeOnWorker',
      test_rect=[0, 0, 200, 200],
      revision=1,
      browser_args=browser_args),

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
      revision=4),
    PixelTestPage(
      'filter_effects.html',
      base_name + '_CSSFilterEffects_NoOverlays',
      test_rect=[0, 0, 300, 300],
      revision=4,
      tolerance=10,
      browser_args=['--disable-mac-overlays']),
  ]
