# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from gpu_tests.webgl_conformance_expectations import WebGLConformanceExpectations

# See the GpuTestExpectations class for documentation.

class WebGL2ConformanceExpectations(WebGLConformanceExpectations):
  def __init__(self, conformance_path, url_prefixes=None, is_asan=False):
    super(WebGL2ConformanceExpectations, self).__init__(
      conformance_path, url_prefixes=url_prefixes, is_asan=is_asan)

  def SetExpectations(self):
    # ===================================
    # Extension availability expectations
    # ===================================
    # It's expected that not all extensions will be available on all platforms.
    # Having a test listed here is not necessarily a problem.

    # Skip these, rather than expect them to fail, to speed up test
    # execution. The browser is restarted even after expected test
    # failures.
    self.Skip('WebglExtension_WEBGL_compressed_texture_astc',
        ['win', 'mac', 'linux'])
    self.Skip('WebglExtension_WEBGL_compressed_texture_atc',
        ['win', 'mac', 'linux'])
    self.Skip('WebglExtension_WEBGL_compressed_texture_etc',
        ['win', 'mac', 'linux'])
    self.Skip('WebglExtension_WEBGL_compressed_texture_etc1',
        ['win', 'mac', 'linux'])
    self.Skip('WebglExtension_WEBGL_compressed_texture_pvrtc',
        ['win', 'mac', 'linux'])
    self.Skip('WebglExtension_WEBGL_compressed_texture_s3tc_srgb',
        ['win', 'mac', 'linux'])

    # ========================
    # Conformance expectations
    # ========================

    # Too slow (take about one hour to run)
    self.Skip('deqp/functional/gles3/builtinprecision/*.html', bug=619403)

    # All platforms.
    self.Flaky('conformance2/query/occlusion-query.html', bug=603168)
    self.Fail('conformance2/glsl3/tricky-loop-conditions.html', bug=483282)

    self.Fail('conformance2/rendering/depth-stencil-feedback-loop.html',
        bug=660844) # WebGL 2.0.1
    self.Fail('conformance2/rendering/rendering-sampling-feedback-loop.html',
        bug=660844) # WebGL 2.0.1
    self.Fail('conformance2/textures/misc/' +
        'integer-cubemap-specification-order-bug.html',
        bug=483282) # owner:cwallez, test might be buggy
    self.Fail('conformance/textures/misc/tex-sub-image-2d-bad-args.html',
        bug=625738)

    self.Fail('conformance/glsl/misc/uninitialized-local-global-variables.html',
        bug=1966) # angle bug ID
    self.Fail('conformance2/glsl3/uninitialized-local-global-variables.html',
        bug=1966) # angle bug ID

    # Windows only.
    self.Fail('conformance2/rendering/blitframebuffer-outside-readbuffer.html',
        ['win', 'd3d11'], bug=644740)
    self.Fail('conformance2/textures/misc/tex-base-level-bug.html',
        ['win', 'd3d11'], bug=705865)

    # Win / NVidia
    self.Flaky('deqp/functional/gles3/fbomultisample*',
        ['win', 'nvidia', 'd3d11'], bug=631317)
    self.Fail('conformance2/rendering/' +
        'draw-with-integer-texture-base-level.html',
        ['win', 'nvidia', 'd3d11'], bug=679639)
    self.Flaky('deqp/functional/gles3/textureshadow/*.html',
        ['win', 'nvidia', 'd3d11'], bug=735464)

    # Win10 / NVIDIA Quadro P400 / D3D11 flaky failures
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'basic_types_interleaved_lines.html',
        ['win10', ('nvidia', 0x1cb3), 'd3d11'], bug=680754)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'basic_types_interleaved_triangles.html',
        ['win10', ('nvidia', 0x1cb3), 'd3d11'], bug=680754)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'basic_types_separate_lines.html',
        ['win10', ('nvidia', 0x1cb3), 'd3d11'], bug=680754)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'basic_types_separate_triangles.html',
        ['win10', ('nvidia', 0x1cb3), 'd3d11'], bug=680754)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'random_interleaved_lines.html',
        ['win10', ('nvidia', 0x1cb3), 'd3d11'], bug=680754)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'random_interleaved_triangles.html',
        ['win10', ('nvidia', 0x1cb3), 'd3d11'], bug=680754)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'random_separate_lines.html',
        ['win10', ('nvidia', 0x1cb3), 'd3d11'], bug=680754)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'random_separate_triangles.html',
        ['win10', ('nvidia', 0x1cb3), 'd3d11'], bug=680754)
    self.Fail('deqp/functional/gles3/transformfeedback/interpolation_flat.html',
        ['win10', ('nvidia', 0x1cb3), 'd3d11'], bug=680754)
    self.Flaky('conformance/textures/image_bitmap_from_video/' +
        'tex-2d-rgba-rgba-unsigned_short_5_5_5_1.html',
        ['win10', ('nvidia', 0x1cb3), 'd3d11'], bug=728670)
    self.Flaky('conformance/textures/image_bitmap_from_video/' +
        'tex-2d-rgba-rgba-unsigned_short_4_4_4_4.html',
        ['win10', ('nvidia', 0x1cb3), 'd3d11'], bug=728670)
    self.Flaky('conformance2/textures/video/*',
        ['win10', ('nvidia', 0x1cb3), 'd3d11'], bug=728670)
    self.Flaky('conformance2/textures/image_bitmap_from_video/*',
        ['win10', ('nvidia', 0x1cb3), 'd3d11'], bug=728670)
    self.Flaky('conformance/extensions/oes-texture-half-float-with-video.html',
        ['win10', ('nvidia', 0x1cb3), 'd3d11'], bug=728670)

    # Win / NVIDIA / OpenGL
    self.Fail('conformance2/glsl3/vector-dynamic-indexing-nv-driver-bug.html',
        ['win', 'nvidia', 'opengl'], bug=693090)
    self.Fail('conformance2/glsl3/' +
        'vector-dynamic-indexing-swizzled-lvalue.html',
        ['win', 'nvidia', 'opengl'], bug=709874)

    # Win / AMD
    self.Fail('conformance2/rendering/blitframebuffer-stencil-only.html',
        ['win', 'amd', 'd3d11'], bug=483282) # owner:jmadill

    # Keep a separate set of failures for the R7 240, since it can use a new
    # and updated driver. The older drivers won't ever get fixes from AMD.
    # Use ['win', ('amd', 0x6613)] for the R7 240 devices.

    # Have seen this time out. Think it may be because it's currently
    # the first test that runs in the shard, and the browser might not
    # be coming up correctly.
    self.Flaky('deqp/functional/gles3/multisample.html',
        ['win', ('amd', 0x6613)], bug=687374)

    # Win / Intel
    self.Fail('conformance2/glsl3/' +
        'texture-offset-uniform-texture-coordinate.html',
        ['win', 'intel', 'd3d11'], bug=662644) # WebGL 2.0.1
    self.Skip('conformance2/textures/misc/copy-texture-image.html',
        ['win', 'intel', 'd3d11'], bug=617449)
    # Seems to cause the harness to fail immediately afterward
    self.Skip('conformance2/textures/video/tex-2d-rgba16f-rgba-half_float.html',
        ['win', 'intel', 'd3d11'], bug=648337)
    self.Flaky('deqp/functional/gles3/lifetime.html',
        ['win', 'intel', 'd3d11'], bug=620379)
    self.Skip('deqp/functional/gles3/texturespecification/' +
        'teximage3d_depth_pbo.html',
        ['win', 'intel', 'd3d11'], bug=617449)
    self.Flaky('deqp/functional/gles3/textureformat/unsized_3d.html',
        ['win', 'intel', 'd3d11'], bug=614418)

    # These tests seem to crash flakily. It's best to leave them as skip
    # until we can run them without GPU hangs and crashes.
    self.Skip('deqp/functional/gles3/textureshadow/2d_array_*.html',
        ['win', 'intel', 'd3d11'], bug=666392)

    # Win 10 / Intel
    self.Fail('deqp/functional/gles3/fbocolorbuffer/clear.html',
        ['win10', 'intel', 'd3d11', 'no_passthrough'], bug=483282)

    # Intel HD 530
    self.Fail('conformance2/textures/misc/angle-stuck-depth-textures.html',
        ['win', 'intel', 'd3d11'], bug=680797)
    self.Fail('deqp/functional/gles3/fboinvalidate/format_00.html',
        ['win', 'intel', 'd3d11'], bug=680797)
    self.Fail('deqp/functional/gles3/fboinvalidate/format_01.html',
        ['win', 'intel', 'd3d11'], bug=680797)
    self.Fail('deqp/functional/gles3/fboinvalidate/format_02.html',
        ['win', 'intel', 'd3d11'], bug=680797)
    self.Fail('deqp/functional/gles3/framebufferblit/' +
        'default_framebuffer_03.html',
        ['win', 'intel', 'd3d11'], bug=680797)
    self.Fail('deqp/functional/gles3/framebufferblit/' +
        'default_framebuffer_04.html',
        ['win', 'intel', 'd3d11'], bug=680797)
    self.Fail('deqp/functional/gles3/framebufferblit/' +
        'default_framebuffer_06.html',
        ['win', 'intel', 'd3d11'], bug=680797)

    # It's unfortunate that these suppressions need to be so broad, but it
    # looks like the D3D11 device can be lost spontaneously on this
    # configuration while running basically any test.
    self.Flaky('conformance/*', ['win', 'intel', 'd3d11'], bug=628395)
    self.Flaky('conformance2/*', ['win', 'intel', 'd3d11'], bug=628395)
    self.Flaky('deqp/*', ['win', 'intel', 'd3d11'], bug=628395)

    # Passthrough command decoder / D3D11
    self.Fail('conformance/textures/image_bitmap_from_video/*',
        ['win', 'passthrough', 'd3d11'], bug=602688)
    self.Fail('conformance/textures/video/*',
        ['win', 'passthrough', 'd3d11'], bug=602688)
    self.Fail('conformance/textures/misc/texture-corner-case-videos.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)
    self.Fail('conformance2/textures/image_bitmap_from_video/*',
        ['win', 'passthrough', 'd3d11'], bug=602688)
    self.Fail('conformance2/textures/video/*',
        ['win', 'passthrough', 'd3d11'], bug=602688)

    self.Fail('conformance2/glsl3/no-attribute-vertex-shader.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)
    self.Fail('conformance2/rendering/clearbuffer-sub-source.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)

    self.Fail('conformance2/textures/canvas/tex-2d-rg8-rg-unsigned_byte.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)
    self.Fail('conformance2/textures/canvas/tex-2d-rg16f-rg-half_float.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)
    self.Fail('conformance2/textures/canvas/tex-2d-rg16f-rg-float.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)
    self.Fail('conformance2/textures/canvas/tex-2d-rg32f-rg-float.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)
    self.Fail('conformance2/textures/canvas/' +
        'tex-2d-rg8ui-rg_integer-unsigned_byte.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)
    self.Fail('conformance2/textures/canvas/' +
        'tex-2d-rgb8ui-rgb_integer-unsigned_byte.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)
    self.Fail('conformance2/textures/canvas/' +
        'tex-2d-rgba8ui-rgba_integer-unsigned_byte.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)

    self.Fail('conformance2/textures/canvas_sub_rectangle/' +
        'tex-2d-rg8-rg-unsigned_byte.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)
    self.Fail('conformance2/textures/canvas_sub_rectangle/' +
        'tex-2d-rg16f-rg-half_float.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)
    self.Fail('conformance2/textures/canvas_sub_rectangle/' +
        'tex-2d-rg16f-rg-float.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)
    self.Fail('conformance2/textures/canvas_sub_rectangle/' +
        'tex-2d-rg32f-rg-float.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)
    self.Fail('conformance2/textures/canvas_sub_rectangle/' +
        'tex-2d-rg8ui-rg_integer-unsigned_byte.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)
    self.Fail('conformance2/textures/canvas_sub_rectangle/' +
        'tex-2d-rgb8ui-rgb_integer-unsigned_byte.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)
    self.Fail('conformance2/textures/canvas_sub_rectangle/' +
        'tex-2d-rgba8ui-rgba_integer-unsigned_byte.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)

    self.Fail('conformance2/textures/webgl_canvas/' +
        'tex-2d-rg8-rg-unsigned_byte.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)
    self.Fail('conformance2/textures/webgl_canvas/' +
        'tex-2d-rg16f-rg-half_float.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)
    self.Fail('conformance2/textures/webgl_canvas/' +
        'tex-2d-rg16f-rg-float.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)
    self.Fail('conformance2/textures/webgl_canvas/' +
        'tex-2d-rg32f-rg-float.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)
    self.Fail('conformance2/textures/webgl_canvas/' +
        'tex-2d-rg8ui-rg_integer-unsigned_byte.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)
    self.Fail('conformance2/textures/webgl_canvas/' +
        'tex-2d-rgb8ui-rgb_integer-unsigned_byte.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)
    self.Fail('conformance2/textures/webgl_canvas/' +
        'tex-2d-rgba8ui-rgba_integer-unsigned_byte.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)

    self.Fail('conformance2/textures/image_bitmap_from_canvas/' +
        'tex-2d-rg8-rg-unsigned_byte.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)
    self.Fail('conformance2/textures/image_bitmap_from_canvas/' +
        'tex-2d-rg16f-rg-half_float.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)
    self.Fail('conformance2/textures/image_bitmap_from_canvas/' +
        'tex-2d-rg16f-rg-float.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)
    self.Fail('conformance2/textures/image_bitmap_from_canvas/' +
        'tex-2d-rg32f-rg-float.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)
    self.Fail('conformance2/textures/image_bitmap_from_canvas/' +
        'tex-2d-rg8ui-rg_integer-unsigned_byte.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)
    self.Fail('conformance2/textures/image_bitmap_from_canvas/' +
        'tex-2d-srgb8-rgb-unsigned_byte.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)
    self.Fail('conformance2/textures/image_bitmap_from_canvas/' +
        'tex-2d-rgb8ui-rgb_integer-unsigned_byte.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)
    self.Fail('conformance2/textures/image_bitmap_from_canvas/' +
        'tex-2d-srgb8_alpha8-rgba-unsigned_byte.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)
    self.Fail('conformance2/textures/image_bitmap_from_canvas/' +
        'tex-2d-rgba8ui-rgba_integer-unsigned_byte.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)

    self.Fail('conformance/glsl/misc/shaders-with-name-conflicts.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)
    self.Fail('deqp/data/gles3/shaders/preprocessor.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)
    self.Fail('conformance2/reading/read-pixels-from-fbo-test.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)
    self.Skip('conformance2/textures/misc/' +
        'copy-texture-image-webgl-specific.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)
    self.Skip('conformance2/reading/read-pixels-pack-parameters.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)
    self.Skip('conformance2/reading/read-pixels-into-pixel-pack-buffer.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)

    self.Fail('conformance2/reading/format-r11f-g11f-b10f.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)
    self.Fail('conformance2/textures/misc/' +
        'tex-image-with-bad-args-from-dom-elements.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)

    self.Fail('conformance/misc/uninitialized-test.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)

    self.Fail('conformance/reading/read-pixels-test.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)
    self.Fail('conformance/textures/misc/copy-tex-image-and-sub-image-2d.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)

    self.Fail('deqp/functional/gles3/fbocolorbuffer/clear.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)
    self.Fail('deqp/functional/gles3/fboinvalidate/sub.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)
    self.Fail('deqp/functional/gles3/shaderbuiltinvar.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)
    self.Fail('deqp/functional/gles3/framebufferblit/' +
        'default_framebuffer_*.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)
    self.Fail('deqp/functional/gles3/instancedrendering.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)
    self.Fail('deqp/functional/gles3/integerstatequery.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)
    self.Fail('deqp/functional/gles3/readpixel.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)
    self.Fail('deqp/functional/gles3/shaderderivate_dfdx.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)
    self.Fail('deqp/functional/gles3/shaderderivate_dfdy.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)
    self.Fail('deqp/functional/gles3/shaderderivate_fwidth.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)
    self.Fail('deqp/functional/gles3/shaderstruct.html',
        ['win', 'passthrough', 'd3d11'], bug=602688)

    # Mac only.

    # Regressions in 10.12.4.
    self.Fail('conformance2/textures/misc/tex-base-level-bug.html',
        ['sierra'], bug=705865)
    self.Fail('conformance2/textures/misc/tex-mipmap-levels.html',
        ['sierra'], bug=705865)

    # Fails on multiple GPU types.
    self.Fail('conformance2/glsl3/vector-dynamic-indexing-swizzled-lvalue.html',
        ['mac'], bug=709351)
    self.Fail('conformance2/rendering/' +
        'framebuffer-completeness-unaffected.html',
        ['mac', 'nvidia', 'intel'], bug=630800)
    self.Fail('deqp/functional/gles3/fbocompleteness.html',
        ['mac', 'nvidia', 'intel'], bug=630800)

    # Mac Retina NVIDIA
    self.Fail('deqp/functional/gles3/fbomultisample*',
        ['mac', ('nvidia', 0xfe9)], bug=641209)
    self.Fail('deqp/functional/gles3/framebufferblit/' +
        'default_framebuffer_04.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('conformance/attribs/gl-disabled-vertex-attrib.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Flaky(
        'conformance/extensions/webgl-compressed-texture-size-limit.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('conformance/programs/' +
        'gl-bind-attrib-location-long-names-test.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('conformance/programs/gl-bind-attrib-location-test.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('conformance2/glsl3/loops-with-side-effects.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('conformance2/textures/misc/tex-input-validation.html',
        ['mac', ('nvidia', 0xfe9), 'no_angle'], bug=483282)
    self.Flaky('conformance2/textures/image_bitmap_from_video/' +
        'tex-2d-rgba16f-rgba-half_float.html',
        ['mac', ('nvidia', 0xfe9)], bug=682834)

    self.Fail('deqp/functional/gles3/draw/random.html',
        ['sierra', ('nvidia', 0xfe9)], bug=716652)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_04.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_07.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_08.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_10.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_11.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_12.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_13.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_18.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_25.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_29.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_32.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_34.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)

    self.Fail('deqp/functional/gles3/pixelbufferobject.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/negativevertexarrayapi.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/shaderindexing/varying.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'teximage2d_pbo_2d_00.html',
        ['mac', ('nvidia', 0xfe9)], bug=614174)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'teximage2d_pbo_2d_01.html',
        ['mac', ('nvidia', 0xfe9)], bug=614174)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'texsubimage2d_pbo_2d_00.html',
        ['mac', ('nvidia', 0xfe9)], bug=614174)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'texsubimage2d_pbo_2d_01.html',
        ['mac', ('nvidia', 0xfe9)], bug=614174)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'texsubimage2d_pbo_cube_00.html',
        ['mac', ('nvidia', 0xfe9)], bug=614174)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'texsubimage2d_pbo_cube_01.html',
        ['mac', ('nvidia', 0xfe9)], bug=614174)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'texsubimage2d_pbo_cube_02.html',
        ['mac', ('nvidia', 0xfe9)], bug=614174)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'texsubimage2d_pbo_cube_03.html',
        ['mac', ('nvidia', 0xfe9)], bug=614174)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'texsubimage2d_pbo_cube_04.html',
        ['mac', ('nvidia', 0xfe9)], bug=614174)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'teximage3d_pbo_2d_array_00.html',
        ['mac', ('nvidia', 0xfe9)], bug=614174)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'teximage3d_pbo_2d_array_01.html',
        ['mac', ('nvidia', 0xfe9)], bug=614174)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'teximage3d_pbo_3d_00.html',
        ['mac', ('nvidia', 0xfe9)], bug=614174)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'teximage3d_pbo_3d_01.html',
        ['mac', ('nvidia', 0xfe9)], bug=614174)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'texsubimage3d_pbo_3d_00.html',
        ['mac', ('nvidia', 0xfe9)], bug=614174)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'texsubimage3d_pbo_3d_01.html',
        ['mac', ('nvidia', 0xfe9)], bug=614174)

    self.Fail('deqp/functional/gles3/fragmentoutput/array.fixed.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/fragmentoutput/basic.fixed.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/fragmentoutput/random_00.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/fragmentoutput/random_01.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/fragmentoutput/random_02.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)

    self.Fail('deqp/functional/gles3/fbocolorbuffer/clear.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/fbocolorbuffer/tex2d_05.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/fbocolorbuffer/tex2darray_05.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/fbocolorbuffer/tex3d_05.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/fbocolorbuffer/texcube_05.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/fbocolorbuffer/blend.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)

    self.Fail('deqp/functional/gles3/draw/draw_arrays.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/draw/draw_arrays_instanced.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/draw/draw_elements.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/draw/draw_elements_instanced.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/draw/draw_range_elements.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)

    self.Fail('deqp/functional/gles3/fboinvalidate/format_02.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)

    self.Fail('deqp/functional/gles3/negativeshaderapi.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)

    self.Flaky('deqp/functional/gles3/vertexarrays/' +
        'multiple_attributes.output.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)

    self.Fail('deqp/functional/gles3/framebufferblit/conversion_28.html',
        ['mac', ('nvidia', 0xfe9)], bug=654187)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_30.html',
        ['mac', ('nvidia', 0xfe9)], bug=654187)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_31.html',
        ['mac', ('nvidia', 0xfe9)], bug=654187)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_33.html',
        ['mac', ('nvidia', 0xfe9)], bug=654187)

    # Mac AMD
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'array_interleaved_lines.html',
        ['mac', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'array_interleaved_points.html',
        ['mac', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'array_interleaved_triangles.html',
        ['mac', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'array_separate_lines.html',
        ['mac', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'array_separate_points.html',
        ['mac', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'array_separate_triangles.html',
        ['mac', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'basic_types_interleaved_lines.html',
        ['mac', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'basic_types_interleaved_points.html',
        ['mac', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'basic_types_interleaved_triangles.html',
        ['mac', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'basic_types_separate_lines.html',
        ['mac', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'basic_types_separate_points.html',
        ['mac', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'basic_types_separate_triangles.html',
        ['mac', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'interpolation_centroid.html',
        ['mac', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'interpolation_flat.html',
        ['mac', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'interpolation_smooth.html',
        ['mac', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'point_size.html',
        ['mac', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'position.html',
        ['mac', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'random_interleaved_lines.html',
        ['mac', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'random_interleaved_points.html',
        ['mac', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'random_interleaved_triangles.html',
        ['mac', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'random_separate_lines.html',
        ['mac', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'random_separate_points.html',
        ['mac', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/transformfeedback/' +
        'random_separate_triangles.html',
        ['mac', 'amd'], bug=483282)

    self.Flaky('deqp/functional/gles3/shaderoperator/common_functions.html',
        ['mac', 'amd'], bug=702336)

    self.Flaky('deqp/functional/gles3/shaderindexing/mat_01.html',
        ['mac', 'amd'], bug=636648)
    self.Flaky('deqp/functional/gles3/shaderindexing/mat_02.html',
        ['mac', 'amd'], bug=644360)

    # These seem to be provoking intermittent GPU process crashes on
    # the MacBook Pros with AMD GPUs.
    self.Flaky('deqp/functional/gles3/texturefiltering/*',
        ['mac', 'amd'], bug=663601)
    self.Flaky('deqp/functional/gles3/textureshadow/*',
        ['mac', 'amd'], bug=663601)
    self.Flaky('deqp/functional/gles3/texturespecification/' +
        'teximage2d_unpack_params.html',
        ['mac', 'amd'], bug=679058)

    self.Fail('conformance2/rendering/clipping-wide-points.html',
        ['mac', 'amd'], bug=642822)

    # Mac Pro with AMD GPU
    self.Flaky('deqp/functional/gles3/shaderindexing/tmp.html',
        ['mac', ('amd', 0x679e)], bug=659871)

    # Mac Intel

    # Regressions in 10.12.4 on Haswell GPUs.
    self.Fail('deqp/functional/gles3/fbocolorbuffer/tex2d_00.html',
        ['mac', ('intel', 0x0a2e)], bug=718194)
    self.Fail('deqp/functional/gles3/fboinvalidate/format_00.html',
        ['mac', ('intel', 0x0a2e)], bug=718194)
    self.Fail('deqp/functional/gles3/framebufferblit/' +
        'default_framebuffer_05.html',
        ['mac', ('intel', 0x0a2e)], bug=718194)

    self.Fail('conformance2/rendering/framebuffer-texture-level1.html',
        ['mac', 'intel'], bug=680278)
    self.Fail('conformance2/textures/misc/angle-stuck-depth-textures.html',
        ['mac', 'intel'], bug=679692)
    self.Fail('deqp/functional/gles3/fbomultisample*',
        ['mac', 'intel'], bug=641209)
    self.Fail('deqp/functional/gles3/texturefiltering/2d_combinations_01.html',
        ['mac', 'intel'], bug=606074)
    self.Fail('deqp/functional/gles3/texturefiltering/' +
        'cube_combinations_01.html',
        ['mac', 'intel'], bug=606074)
    self.Fail('deqp/functional/gles3/texturefiltering/' +
        '2d_array_combinations_01.html',
        ['mac', 'intel'], bug=606074)
    self.Fail('deqp/functional/gles3/texturefiltering/3d_combinations_06.html',
        ['mac', 'intel'], bug=606074)
    self.Fail('deqp/functional/gles3/texturefiltering/3d_combinations_07.html',
        ['mac', 'intel'], bug=606074)
    self.Fail('deqp/functional/gles3/texturefiltering/3d_combinations_08.html',
        ['mac', 'intel'], bug=606074)

    self.Fail('deqp/functional/gles3/texturespecification/' +
        'random_teximage2d_2d.html',
        ['mac', 'intel'], bug=483282)

    self.Fail('deqp/functional/gles3/shadertexturefunction/' +
        'texturelod.html',
        ['mac', 'intel'], bug=483282)
    self.Fail('deqp/functional/gles3/shadertexturefunction/' +
        'texturegrad.html',
        ['mac', 'intel'], bug=483282)
    self.Fail('deqp/functional/gles3/shadertexturefunction/' +
        'textureprojgrad.html',
        ['mac', 'intel'], bug=483282)

    self.Fail('conformance2/textures/canvas_sub_rectangle/' +
              'tex-2d-r8ui-red_integer-unsigned_byte.html',
              ['yosemite', 'intel'], bug=665656)
    self.Fail('conformance2/textures/canvas_sub_rectangle/' +
              'tex-2d-rg8ui-rg_integer-unsigned_byte.html',
              ['yosemite', 'intel'], bug=665656)
    self.Fail('conformance2/textures/canvas_sub_rectangle/' +
              'tex-2d-rgb8ui-rgb_integer-unsigned_byte.html',
              ['yosemite', 'intel'], bug=665656)
    self.Fail('conformance2/textures/canvas_sub_rectangle/' +
              'tex-2d-rgba8ui-rgba_integer-unsigned_byte.html',
              ['yosemite', 'intel'], bug=665656)

    self.Fail('conformance2/textures/image_data/' +
        'tex-2d-rgba8ui-rgba_integer-unsigned_byte.html',
        ['mac', 'intel'], bug=665197)
    self.Fail('conformance2/textures/image_data/' +
        'tex-2d-rgb8ui-rgb_integer-unsigned_byte.html',
        ['mac', 'intel'], bug=665197)
    self.Fail('conformance2/textures/image_data/' +
        'tex-2d-rg8ui-rg_integer-unsigned_byte.html',
        ['mac', 'intel'], bug=665197)

    self.Fail('conformance2/textures/misc/' +
        'integer-cubemap-texture-sampling.html',
        ['mac', 'intel'], bug=658930)

    # Linux only.
    self.Flaky('conformance/textures/video/' +
               'tex-2d-rgba-rgba-unsigned_byte.html',
               ['linux'], bug=627525)
    self.Flaky('conformance/textures/video/' +
               'tex-2d-rgba-rgba-unsigned_short_4_4_4_4.html',
               ['linux'], bug=627525)
    self.Flaky('conformance/textures/video/' +
               'tex-2d-rgba-rgba-unsigned_short_5_5_5_1.html',
               ['linux'], bug=627525)
    self.Flaky('conformance/textures/video/' +
               'tex-2d-rgb-rgb-unsigned_byte.html',
               ['linux'], bug=627525)
    self.Flaky('conformance/textures/video/' +
               'tex-2d-rgb-rgb-unsigned_short_5_6_5.html',
               ['linux'], bug=627525)
    self.Fail('conformance2/glsl3/vector-dynamic-indexing-nv-driver-bug.html',
        ['linux'], bug=483282)
    self.Fail('conformance2/textures/image_bitmap_from_image/' +
              'tex-3d-r16f-red-float.html', ['linux'], bug=679695)

    # Linux Multi-vendor failures.
    self.Skip('deqp/data/gles3/shaders/qualification_order.html',
        ['linux', 'amd', 'intel'], bug=483282)
    self.Flaky('deqp/functional/gles3/texturespecification/' +
        'random_teximage2d_2d.html',
        ['linux', 'amd', 'intel'], bug=618447)
    self.Fail('conformance2/rendering/clipping-wide-points.html',
        ['linux', 'amd', 'intel'], bug=662644) # WebGL 2.0.1

    # Linux NVIDIA
    # This test is flaky both with and without ANGLE.
    self.Flaky('deqp/functional/gles3/texturespecification/' +
        'random_teximage2d_2d.html',
        ['linux', 'nvidia'], bug=618447)
    self.Fail('conformance/glsl/bugs/unary-minus-operator-float-bug.html',
        ['linux', 'nvidia'], bug=672380)
    self.Fail('conformance2/glsl3/vector-dynamic-indexing-swizzled-lvalue.html',
        ['linux', 'nvidia'], bug=709351)
    self.Fail('conformance2/textures/image_bitmap_from_canvas/' +
        'tex-3d-srgb8_alpha8-rgba-unsigned_byte.html',
        ['linux', 'nvidia'], bug=679677)
    self.Fail('conformance2/rendering/framebuffer-texture-level1.html',
        ['linux', 'nvidia', 'opengl'], bug=680278)
    self.Fail('conformance2/textures/image/' +
        'tex-3d-rg8ui-rg_integer-unsigned_byte.html',
        ['linux', ('nvidia', 0xf02)], bug=680282)
    self.Flaky('conformance2/textures/image_bitmap_from_image_data/' +
        'tex-2d-srgb8-rgb-unsigned_byte.html',
        ['linux', 'nvidia'], bug=694354)

    # Linux NVIDIA Quadro P400
    # This test causes a lost device and then the next test fails.
    self.Skip('conformance2/rendering/blitframebuffer-size-overflow.html',
        ['linux', ('nvidia', 0x1cb3)], bug=709320)

    # Linux Intel
    self.Fail('conformance2/extensions/ext-color-buffer-float.html',
        ['linux', 'intel'], bug=640389)
    self.Fail('WebglExtension_EXT_disjoint_timer_query_webgl2',
        ['linux', 'intel'], bug=687210)

    # See https://bugs.freedesktop.org/show_bug.cgi?id=94477
    self.Skip('conformance/glsl/bugs/temp-expressions-should-not-crash.html',
        ['linux', 'intel'], bug=540543)  # GPU timeout

    self.Fail('deqp/functional/gles3/fbomultisample.8_samples.html',
        ['linux', 'intel'], bug=635528)


    self.Fail('conformance2/textures/misc/tex-subimage3d-pixel-buffer-bug.html',
       ['linux', 'intel'], bug=662644) # WebGL 2.0.1

    self.Fail('deqp/functional/gles3/shadertexturefunction/texturesize.html',
       ['linux', 'intel'], bug=666384)
    self.Fail('conformance2/textures/misc/tex-3d-mipmap-levels-intel-bug.html',
       ['linux', 'intel'], bug=666384)

    # Fails on Intel Mesa GL 3.3, passes on Intel Mesa GL 4.5.
    self.Fail('conformance2/misc/views-with-offsets.html',
        ['linux', 'intel', 'no_angle'], bug=664180)

    # Linux Intel with ANGLE only
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_07.html',
        ['linux', 'intel', 'opengl'], bug=598902)
    self.Fail('conformance2/rendering/blitframebuffer-filter-srgb.html',
        ['linux', 'intel', 'opengl'], bug=680276)
    self.Fail('conformance2/rendering/blitframebuffer-outside-readbuffer.html',
        ['linux', 'intel', 'opengl'], bug=680276)

    # Linux Intel HD 530
    self.Fail('conformance/extensions/webgl-compressed-texture-astc.html',
        ['linux', 'intel'], bug=680720)
    self.Fail('conformance2/rendering/blitframebuffer-filter-outofbounds.html',
        ['linux', 'intel'], bug=680720)
    self.Fail('conformance2/rendering/blitframebuffer-filter-srgb.html',
        ['linux', 'intel', 'no_angle'], bug=680720)
    self.Fail('conformance2/rendering/blitframebuffer-outside-readbuffer.html',
        ['linux', 'intel', 'no_angle'], bug=680720)

    self.Fail('deqp/functional/gles3/framebufferblit/conversion_04.html',
        ['linux', 'intel'], bug=680720)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_08.html',
        ['linux', 'intel'], bug=680720)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_10.html',
        ['linux', 'intel'], bug=680720)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_11.html',
        ['linux', 'intel'], bug=680720)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_12.html',
        ['linux', 'intel'], bug=680720)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_13.html',
        ['linux', 'intel'], bug=680720)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_18.html',
        ['linux', 'intel'], bug=680720)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_25.html',
        ['linux', 'intel'], bug=680720)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_28.html',
        ['linux', 'intel'], bug=680720)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_29.html',
        ['linux', 'intel'], bug=680720)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_30.html',
        ['linux', 'intel'], bug=680720)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_31.html',
        ['linux', 'intel'], bug=680720)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_32.html',
        ['linux', 'intel'], bug=680720)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_33.html',
        ['linux', 'intel'], bug=680720)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_34.html',
        ['linux', 'intel'], bug=680720)
    self.Fail('deqp/functional/gles3/framebufferblit/' +
        'default_framebuffer_00.html',
        ['linux', 'intel'], bug=680720)
    self.Fail('conformance2/glsl3/' +
        'vector-dynamic-indexing-swizzled-lvalue.html',
        ['linux', 'intel'], bug=709874)

    # Intermittently running out of memory.
    self.Flaky('deqp/functional/gles3/texturefiltering/*',
        ['linux', 'intel'], bug=725664)
    self.Flaky('deqp/functional/gles3/textureformat/*',
        ['linux', 'intel'], bug=725664)
    self.Flaky('deqp/functional/gles3/textureshadow/*',
        ['linux', 'intel'], bug=725664)
    self.Flaky('deqp/functional/gles3/texturespecification/*',
        ['linux', 'intel'], bug=725664)

    # Linux AMD only.
    # It looks like AMD shader compiler rejects many valid ES3 semantics.
    self.Fail('conformance/glsl/misc/shaders-with-invariance.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('conformance2/glsl3/vector-dynamic-indexing-swizzled-lvalue.html',
        ['linux', 'amd'], bug=709351)
    self.Fail('deqp/functional/gles3/multisample.html',
        ['linux', 'amd'], bug=617290)
    self.Fail('deqp/data/gles3/shaders/conversions.html',
        ['linux', 'amd'], bug=483282)
    self.Skip('deqp/data/gles3/shaders/arrays.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/internalformatquery.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/texturestatequery.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/buffercopy.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/samplerobject.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/shaderprecision_int.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/texturefiltering/3d*',
        ['linux', 'amd'], bug=606114)
    self.Fail('deqp/functional/gles3/shadertexturefunction/texture.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/shadertexturefunction/texturegrad.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/shadertexturefunction/' +
        'texelfetchoffset.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/vertexarrays/' +
        'single_attribute.first.html',
        ['linux', 'amd'], bug=694877)

    self.Fail('deqp/functional/gles3/negativetextureapi.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/transformfeedback/array_separate*.html',
        ['linux', 'amd'], bug=483282)

    self.Fail('conformance2/misc/uninitialized-test-2.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('conformance2/reading/read-pixels-from-fbo-test.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('conformance2/rendering/blitframebuffer-filter-srgb.html',
        ['linux', 'amd'], bug=634525)
    self.Fail('conformance2/rendering/blitframebuffer-outside-readbuffer.html',
        ['linux', 'amd'], bug=662644) # WebGL 2.0.1
    self.Fail('conformance2/renderbuffers/framebuffer-texture-layer.html',
        ['linux', 'amd'], bug=295792)
    self.Fail('conformance2/textures/misc/tex-mipmap-levels.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('conformance2/textures/misc/copy-texture-image-luma-format.html',
        ['linux', 'amd'], bug=483282)

    self.Fail('deqp/functional/gles3/texturespecification/' +
        'teximage2d_pbo_cube_00.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'teximage2d_pbo_cube_01.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'teximage2d_pbo_cube_02.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'teximage2d_pbo_cube_03.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'teximage2d_pbo_cube_04.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'teximage2d_pbo_params.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'teximage2d_depth_pbo.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'basic_copyteximage2d.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'basic_teximage3d_3d_00.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'basic_teximage3d_3d_01.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'basic_teximage3d_3d_02.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'basic_teximage3d_3d_03.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'basic_teximage3d_3d_04.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'texstorage2d_format_depth_stencil.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'texstorage3d_format_2d_array_00.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'texstorage3d_format_2d_array_01.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'texstorage3d_format_2d_array_02.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'texstorage3d_format_3d_00.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'texstorage3d_format_3d_01.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'texstorage3d_format_3d_02.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'texstorage3d_format_3d_03.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'texstorage3d_format_depth_stencil.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'texstorage3d_format_size.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/vertexarrays/' +
        'single_attribute.output_type.unsigned_int.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/draw/*.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/fbomultisample*',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/fbocompleteness.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/textureshadow/*.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/shadermatrix/mul_dynamic_highp.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/shadermatrix/mul_dynamic_lowp.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/shadermatrix/mul_dynamic_mediump.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/shadermatrix/pre_decrement.html',
        ['linux', 'amd'], bug=483282)

    self.Fail('deqp/functional/gles3/framebufferblit/conversion_04.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_07.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_08.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_10.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_11.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_12.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_13.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_18.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_25.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_28.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_29.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_30.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_31.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_32.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_33.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_34.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/' +
        'default_framebuffer_00.html',
        ['linux', 'amd'], bug=658832)

    self.Fail('deqp/functional/gles3/shaderoperator/unary_operator_01.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/shaderoperator/unary_operator_02.html',
        ['linux', 'amd'], bug=483282)

    self.Fail('conformance2/glsl3/vector-dynamic-indexing.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('conformance2/reading/read-pixels-pack-parameters.html',
        ['linux', 'amd', 'no_angle'], bug=483282)
    self.Fail('conformance2/textures/misc/tex-unpack-params.html',
        ['linux', 'amd', 'no_angle'], bug=483282)
    self.Fail('conformance2/extensions/ext-color-buffer-float.html',
        ['linux', 'amd'], bug=633022)
    self.Fail('conformance2/rendering/blitframebuffer-filter-outofbounds.html',
        ['linux', 'amd'], bug=655147)

    self.Fail('conformance2/textures/misc/tex-base-level-bug.html',
        ['linux', 'amd'], bug=705865)
    self.Fail('conformance2/textures/image/' +
        'tex-2d-r11f_g11f_b10f-rgb-float.html',
        ['linux', 'amd'], bug=705865)

    # Uniform buffer related failures
    self.Fail('deqp/functional/gles3/uniformbuffers/single_struct_array.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/uniformbuffers/single_nested_struct.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/uniformbuffers/' +
        'single_nested_struct_array.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/uniformbuffers/multi_basic_types.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/uniformbuffers/multi_nested_struct.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/uniformbuffers/random.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('conformance2/buffers/uniform-buffers.html',
        ['linux', 'amd'], bug=658842)
    self.Fail('conformance2/rendering/uniform-block-buffer-size.html',
        ['linux', 'amd'], bug=658844)

    # Linux AMD R7 240
    self.Fail('conformance2/textures/canvas/' +
        'tex-2d-rg8ui-rg_integer-unsigned_byte.html',
        ['linux', ('amd', 0x6613)], bug=710392)
    self.Fail('conformance2/textures/canvas/' +
        'tex-2d-rgb8ui-rgb_integer-unsigned_byte.html',
        ['linux', ('amd', 0x6613)], bug=710392)
    self.Fail('conformance2/textures/canvas/' +
        'tex-2d-rgba8ui-rgba_integer-unsigned_byte.html',
        ['linux', ('amd', 0x6613)], bug=710392)
    self.Fail('conformance2/textures/webgl_canvas/' +
        'tex-2d-rg8ui-rg_integer-unsigned_byte.html',
        ['linux', ('amd', 0x6613)], bug=710392)
    self.Fail('conformance2/textures/webgl_canvas/' +
        'tex-2d-rgb8ui-rgb_integer-unsigned_byte.html',
        ['linux', ('amd', 0x6613)], bug=710392)
    self.Fail('conformance2/textures/webgl_canvas/' +
        'tex-2d-rgba8ui-rgba_integer-unsigned_byte.html',
        ['linux', ('amd', 0x6613)], bug=710392)
    self.Fail('conformance2/textures/image_bitmap_from_video/' +
        'tex-2d-rgba16f-rgba-float.html',
        ['linux', ('amd', 0x6613)], bug=701138)
    self.Fail('conformance2/textures/image_bitmap_from_video/' +
        'tex-2d-rgba16f-rgba-half_float.html',
        ['linux', ('amd', 0x6613)], bug=701138)
    self.Fail('conformance2/textures/image_bitmap_from_video/' +
        'tex-2d-rgba32f-rgba-float.html',
        ['linux', ('amd', 0x6613)], bug=701138)
    self.Fail('conformance2/textures/image_bitmap_from_video/' +
        'tex-2d-rgba4-rgba-unsigned_byte.html',
        ['linux', ('amd', 0x6613)], bug=701138)
    self.Fail('conformance2/textures/image_bitmap_from_video/' +
        'tex-2d-rgba4-rgba-unsigned_short_4_4_4_4.html',
        ['linux', ('amd', 0x6613)], bug=701138)
    self.Fail('conformance2/textures/image_data/' +
        'tex-3d-rgb565-rgb-unsigned_byte.html',
        ['linux', ('amd', 0x6613)], bug=701138)
    self.Fail('conformance2/textures/image_data/' +
        'tex-3d-rgb565-rgb-unsigned_short_5_6_5.html',
        ['linux', ('amd', 0x6613)], bug=701138)
    self.Fail('conformance2/textures/image_data/' +
        'tex-3d-rgb5_a1-rgba-unsigned_byte.html',
        ['linux', ('amd', 0x6613)], bug=701138)

    # Conflicting expectations to test that the
    # "Expectations have no collisions" unittest works.
    # page_name = 'conformance/glsl/constructors/glsl-construct-ivec4.html'

    # Conflict when all conditions match
    # self.Fail(page_name,
    #     ['linux', ('nvidia', 0x1), 'debug', 'opengl'])
    # self.Fail(page_name,
    #     ['linux', ('nvidia', 0x1), 'debug', 'opengl'])

    # Conflict when all conditions match (and different sets)
    # self.Fail(page_name,
    #     ['linux', 'win', ('nvidia', 0x1), 'debug', 'opengl'])
    # self.Fail(page_name,
    #     ['linux', 'mac', ('nvidia', 0x1), 'amd', 'debug', 'opengl'])

    # Conflict with one aspect not specified
    # self.Fail(page_name,
    #     ['linux', ('nvidia', 0x1), 'debug'])
    # self.Fail(page_name,
    #     ['linux', ('nvidia', 0x1), 'debug', 'opengl'])

    # Conflict with one aspect not specified (in both conditions)
    # self.Fail(page_name,
    #     ['linux', ('nvidia', 0x1), 'debug'])
    # self.Fail(page_name,
    #     ['linux', ('nvidia', 0x1), 'debug'])

    # Conflict even if the GPU is specified in a device ID
    # self.Fail(page_name,
    #     ['linux', ('nvidia', 0x1), 'debug'])
    # self.Fail(page_name,
    #     ['linux', 'nvidia', 'debug'])

    # Test there are no conflicts between two different devices
    # self.Fail(page_name,
    #     ['linux', ('nvidia', 0x1), 'debug'])
    # self.Fail(page_name,
    #     ['linux', ('nvidia', 0x2), 'debug'])

    # Test there are no conflicts between two devices with different vendors
    # self.Fail(page_name,
    #     ['linux', ('nvidia', 0x1), 'debug'])
    # self.Fail(page_name,
    #     ['linux', ('amd', 0x1), 'debug'])

    # Conflicts if there is a device and nothing specified for the other's
    # GPU vendors
    # self.Fail(page_name,
    #     ['linux', ('nvidia', 0x1), 'debug'])
    # self.Fail(page_name,
    #     ['linux', 'debug'])

    # Test no conflicts happen when only one aspect differs
    # self.Fail(page_name,
    #     ['linux', ('nvidia', 0x1), 'debug', 'opengl'])
    # self.Fail(page_name,
    #     ['win', ('nvidia', 0x1), 'debug', 'opengl'])

    # Conflicts if between a generic os condition and a specific version
    # self.Fail(page_name,
    #     ['xp', ('nvidia', 0x1), 'debug', 'opengl'])
    # self.Fail(page_name,
    #     ['win', ('nvidia', 0x1), 'debug', 'opengl'])
