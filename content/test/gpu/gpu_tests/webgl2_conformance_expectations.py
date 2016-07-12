# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from gpu_tests.webgl_conformance_expectations import WebGLConformanceExpectations

# See the GpuTestExpectations class for documentation.

class WebGL2ConformanceExpectations(WebGLConformanceExpectations):
  def __init__(self, conformance_path):
    super(WebGL2ConformanceExpectations, self).__init__(conformance_path)

  def SetExpectations(self):
    # ===================================
    # Extension availability expectations
    # ===================================
    # It's expected that not all extensions will be available on all platforms.
    # Having a test listed here is not necessarily a problem.

    self.Fail('WebglExtension.WEBGL_compressed_texture_astc',
        ['win', 'mac', 'linux'])
    self.Fail('WebglExtension.WEBGL_compressed_texture_atc',
        ['win', 'mac', 'linux'])
    self.Fail('WebglExtension.WEBGL_compressed_texture_etc1',
        ['mac', 'linux'])
    self.Fail('WebglExtension.WEBGL_compressed_texture_pvrtc',
        ['win', 'mac', 'linux'])

    # ========================
    # Conformance expectations
    # ========================

    # All platforms.

    # Too slow (take about one hour to run)
    self.Skip('deqp/functional/gles3/builtinprecision/*.html', bug=619403)

    self.Fail('deqp/functional/gles3/framebufferblit/depth_stencil.html',
        bug=483282)
    self.Fail('deqp/data/gles3/shaders/linkage.html', bug=601821)

    self.Flaky('deqp/functional/gles3/negativefragmentapi.html', bug=604794)

    self.Fail('deqp/data/gles3/shaders/preprocessor.html', bug=483282)

    self.Fail('conformance2/glsl3/forbidden-operators.html', bug=483282)

    self.Flaky('conformance2/query/occlusion-query.html', bug=603168)

    self.Fail('conformance2/rendering/attrib-type-match.html', bug=627193)

    self.Fail('conformance2/transform_feedback/' +
        'unwritten-output-defaults-to-zero.html', bug=1441) # ANGLE bug

    # Avoid a conflict with a Mac expectation by setting
    self.Fail('conformance2/textures/misc/tex-input-validation.html',
        ['d3d9', 'd3d11', 'opengl'], bug=483282)

    # All platforms with AMD GPU.
    self.Fail('deqp/functional/gles3/multisample.html',
        ['amd'], bug=617290)

    # Windows only.
    self.Fail('deqp/functional/gles3/shaderloop_for.html',
        ['win'], bug=617817)
    self.Fail('deqp/functional/gles3/shaderloop_while.html',
        ['win'], bug=617817)
    self.Fail('deqp/functional/gles3/shaderloop_do_while.html',
        ['win'], bug=617817)

    self.Fail('deqp/functional/gles3/shadertexturefunction/texturelod.html',
        ['win'], bug=483282)
    self.Fail('deqp/functional/gles3/shadertexturefunction/' +
        'texturelodoffset.html',
        ['win'], bug=483282)
    self.Fail('deqp/functional/gles3/shadertexturefunction/' +
        'textureprojlod.html',
        ['win'], bug=483282)
    self.Fail('deqp/functional/gles3/shadertexturefunction/' +
        'textureprojlodoffset.html',
        ['win'], bug=483282)
    self.Fail('deqp/functional/gles3/shadertexturefunction/texturegrad.html',
        ['win'], bug=483282)
    self.Fail('deqp/functional/gles3/shadertexturefunction/' +
        'texturegradoffset.html',
        ['win'], bug=483282)
    self.Fail('deqp/functional/gles3/shadertexturefunction/' +
        'textureprojgrad.html',
        ['win'], bug=483282)
    self.Fail('deqp/functional/gles3/shadertexturefunction/' +
        'textureprojgradoffset.html',
        ['win'], bug=483282)

    self.Fail('conformance2/glsl3/array-in-complex-expression.html',
        ['win'], bug=483282)
    self.Skip('conformance2/reading/read-pixels-pack-parameters.html',
        ['win'], bug=483282)

    self.Fail('deqp/functional/gles3/fbomultisample*',
        ['win'], bug=483282)

    self.Fail('deqp/functional/gles3/fboinvalidate/sub.html',
        ['win'], bug=483282)
    self.Fail('deqp/functional/gles3/fboinvalidate/whole.html',
        ['win'], bug=624506)

    # Windows 8 only.

    self.Flaky('deqp/functional/gles3/buffercopy.html', ['win8'], bug=587601)

    # Win / NVidia
    self.Fail('deqp/functional/gles3/textureformat/compressed_cube.html',
        ['win', 'nvidia'], bug=614573)
    self.Fail('deqp/functional/gles3/framebufferblit/rect_02.html',
        ['win', 'nvidia'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/rect_05.html',
        ['win', 'nvidia'], bug=483282)
    self.Fail('deqp/functional/gles3/transformfeedback/*.html',
        ['win', 'nvidia'], bug=483282)

    # Win / AMD
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'texstorage2d_format_depth_stencil.html',
        ['win', ('amd', 0x6779)], bug=614178)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'texstorage3d_format_depth_stencil.html',
        ['win', ('amd', 0x6779)], bug=614178)
    self.Fail('deqp/functional/gles3/textureformat/compressed_cube.html',
        ['win', ('amd', 0x6779)], bug=614573)
    self.Fail('deqp/functional/gles3/shadertexturefunction/texture.html',
        ['win', ('amd', 0x6779)], bug=483282)
    self.Fail('deqp/functional/gles3/shadertexturefunction/' +
        'texelfetchoffset.html',
        ['win', ('amd', 0x6779)], bug=483282)
    self.Fail('deqp/functional/gles3/shadertexturefunction/texturesize.html',
        ['win', ('amd', 0x6779)], bug=483282)

    self.Fail('deqp/functional/gles3/framebufferblit/rect_02.html',
        ['win', ('amd', 0x6779)], bug=483282)
    self.Fail('deqp/functional/gles3/textureshadow/2d*',
        ['win', ('amd', 0x6779)], bug=483282)
    self.Fail('deqp/functional/gles3/textureshadow/cube*',
        ['win', ('amd', 0x6779)], bug=483282)

    self.Fail('deqp/functional/gles3/transformfeedback/*.html',
        ['win', ('amd', 0x6779)], bug=483282)

    # It's unfortunate that this suppression needs to be so broad, but
    # basically any test that uses readPixels is potentially flaky, and
    # it's infeasible to suppress individual failures one by one.
    self.Flaky('conformance2/*', ['win', ('amd', 0x6779)], bug=491419)
    self.Flaky('deqp/*', ['win', ('amd', 0x6779)], bug=491419)

    # Win / Intel
    self.Fail('conformance2/buffers/uniform-buffers.html',
        ['win', 'intel'], bug=483282)
    self.Skip('conformance2/textures/misc/copy-texture-image.html',
        ['win', 'intel'], bug=617449)
    self.Fail('deqp/functional/gles3/shaderderivate_*',
        ['win', 'intel'], bug=483282)
    self.Fail('deqp/functional/gles3/shaderstruct.html',
        ['win', 'intel'], bug=483282)
    self.Flaky('deqp/functional/gles3/lifetime.html',
        ['win', 'intel'], bug=620379)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'teximage3d_depth.html',
        ['win', 'intel'], bug=614418)
    self.Skip('deqp/functional/gles3/texturespecification/' +
        'teximage3d_depth_pbo.html',
        ['win', 'intel'], bug=617449)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'texsubimage3d_depth.html',
        ['win', 'intel'], bug=614418)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'texstorage3d_format_depth_stencil.html',
        ['win', 'intel'], bug=614418)
    self.Fail('deqp/functional/gles3/textureformat/sized_color_3d_pot_00.html',
        ['win', 'intel'], bug=614418)
    self.Fail('deqp/functional/gles3/textureformat/sized_color_3d_pot_01.html',
        ['win', 'intel'], bug=614418)
    self.Fail('deqp/functional/gles3/textureformat/sized_color_3d_pot_02.html',
        ['win', 'intel'], bug=614418)
    self.Fail('deqp/functional/gles3/textureformat/sized_color_3d_pot_03.html',
        ['win', 'intel'], bug=614418)
    self.Fail('deqp/functional/gles3/textureformat/sized_depth_stencil.html',
        ['win', 'intel'], bug=614418)
    self.Fail('deqp/functional/gles3/textureformat/compressed_cube.html',
        ['win', 'intel'], bug=614418)
    self.Flaky('deqp/functional/gles3/textureformat/unsized_3d.html',
        ['win', 'intel'], bug=614418)
    self.Fail('deqp/functional/gles3/shadertexturefunction/texture.html',
        ['win', 'intel'], bug=483282)
    self.Fail('deqp/functional/gles3/shadertexturefunction/' +
        'texelfetchoffset.html',
        ['win', 'intel'], bug=483282)
    self.Fail('deqp/functional/gles3/shadertexturefunction/texturesize.html',
        ['win', 'intel'], bug=483282)
    self.Fail('deqp/functional/gles3/uniformbuffers/*.html',
        ['win', 'intel'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/rect_02.html',
        ['win', 'intel'], bug=483282)
    self.Fail('deqp/functional/gles3/textureshadow/2d_array_*.html',
        ['win', 'intel'], bug=483282)
    self.Flaky('deqp/functional/gles3/transformfeedback/*.html',
        ['win', 'intel'], bug=626068)

    # Mac only.
    self.Flaky('deqp/functional/gles3/shaderindexing/varying.html',
        ['mac'], bug=619264)
    self.Fail('deqp/functional/gles3/shaderloop_do_while.html',
        ['mac'], bug=617820)
    # This one's flaky on AMD, NVIDIA and Intel GPUs, but the
    # GPU-specific expectations aren't working properly.
    self.Fail('deqp/functional/gles3/shaderpackingfunction.html',
        ['mac'], bug=619264)
    self.Fail('deqp/functional/gles3/uniformbuffers/random.html',
        ['mac'], bug=618464)

    self.Fail('deqp/functional/gles3/textureformat/compressed_2d.html',
        ['mac'], bug=612205)
    self.Fail('deqp/functional/gles3/textureformat/compressed_cube.html',
        ['mac'], bug=612205)
    self.Fail('deqp/functional/gles3/texturewrap/e*',
        ['mac'], bug=612205)

    self.Fail('deqp/data/gles3/shaders/qualification_order.html',
        ['mac'], bug=483282)
    self.Fail('deqp/data/gles3/shaders/scoping.html',
        ['mac'], bug=483282)
    self.Fail('deqp/functional/gles3/pixelbufferobject.html',
        ['mac'], bug=483282)

    self.Fail('conformance2/textures/misc/compressed-tex-image.html',
        ['mac'], bug=565438)
    self.Fail('conformance2/textures/misc/tex-storage-compressed-formats.html',
        ['mac'], bug=295792)
    self.Fail('conformance2/renderbuffers/framebuffer-test.html',
        ['mac'], bug=483282)
    self.Fail('conformance2/rendering/framebuffer-completeness-unaffected.html',
        ['mac'], bug=604053)

    self.Fail('deqp/functional/gles3/instancedrendering.html',
        ['mac'], bug=483282)

    self.Fail('deqp/functional/gles3/negativetextureapi.html',
        ['mac'], bug=483282)

    self.Fail('deqp/functional/gles3/fbomultisample*',
        ['mac'], bug=483282)

    self.Fail('deqp/functional/gles3/framebufferblit/conversion_04.html',
        ['mac'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_07.html',
        ['mac'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_08.html',
        ['mac'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_10.html',
        ['mac'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_11.html',
        ['mac'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_12.html',
        ['mac'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_13.html',
        ['mac'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_18.html',
        ['mac'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_25.html',
        ['mac'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_28.html',
        ['mac'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_29.html',
        ['mac'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_30.html',
        ['mac'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_31.html',
        ['mac'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_32.html',
        ['mac'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_33.html',
        ['mac'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_34.html',
        ['mac'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/' +
        'default_framebuffer_*.html',
        ['mac'], bug=483282)

    # Mac Retina NVIDIA
    self.Fail('conformance2/textures/misc/tex-input-validation.html',
        ['mac', ('nvidia', 0xfe9), 'no_angle'], bug=483282)
    self.Fail('conformance2/textures/misc/tex-mipmap-levels.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/shaderstruct.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/shaderswitch.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/negativevertexarrayapi.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)
    self.Fail('deqp/functional/gles3/fbocompleteness.html',
        ['mac', ('nvidia', 0xfe9)], bug=616562)

    self.Fail('deqp/functional/gles3/texturespecification/' +
        'basic_copyteximage2d.html',
        ['mac', ('nvidia', 0xfe9)], bug=620067)
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

    self.Fail('conformance2/rendering/draw-buffers.html',
        ['mac', ('nvidia', 0xfe9)], bug=617410)

    self.Fail('deqp/functional/gles3/fboinvalidate/format_02.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)

    self.Fail('deqp/functional/gles3/negativeshaderapi.html',
        ['mac', ('nvidia', 0xfe9)], bug=483282)

    # Mac AMD
    self.Fail('deqp/functional/gles3/clipping.html',
        ['mac', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/primitiverestart/00.html',
        ['mac', 'amd'], bug=598930)
    self.Fail('deqp/functional/gles3/primitiverestart/01.html',
        ['mac', 'amd'], bug=598930)
    self.Fail('deqp/functional/gles3/shadercommonfunction.html',
        ['mac', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/transformfeedback/*.html',
        ['mac', 'amd'], bug=483282)

    self.Fail('deqp/functional/gles3/shadertexturefunction/' +
        'texturesize.html',
        ['mac', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/shadertexturefunction/' +
        'textureprojlodoffset.html',
        ['mac', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/shadertexturefunction/' +
        'textureprojlod.html',
        ['mac', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/vertexarrays/' +
        'single_attribute.normalize.html',
        ['mac', 'amd'], bug=483282)

    self.Fail('deqp/functional/gles3/framebufferblit/rect_02.html',
        ['mac', 'amd'], bug=483282)

    self.Fail('deqp/functional/gles3/shaderoperator/' +
        'angle_and_trigonometry_02.html',
        ['mac', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/shaderoperator/' +
        'angle_and_trigonometry_03.html',
        ['mac', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/shaderoperator/common_fucntions.html',
        ['mac', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/shaderoperator/geometric.html',
        ['mac', 'amd'], bug=483282)

    # Mac Intel
    self.Fail('conformance2/textures/misc/tex-unpack-params.html',
        ['mac', 'intel', 'no_angle'], bug=483282)

    self.Fail('deqp/functional/gles3/shadercommonfunction.html',
        ['mac', 'intel'], bug=483282)
    self.Fail('deqp/functional/gles3/shaderderivate_*',
        ['mac', 'intel'], bug=483282)
    self.Fail('deqp/functional/gles3/transformfeedback/*.html',
        ['mac', 'intel'], bug=483282)

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
        'basic_copyteximage2d.html',
        ['mac', 'intel'], bug=620067)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'random_teximage2d_2d.html',
        ['mac', 'intel'], bug=483282)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'teximage3d_pbo_params.html',
        ['mac', 'intel'], bug=483282)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'texsubimage3d_pbo_params.html',
        ['mac', 'intel'], bug=483282)

    self.Fail('deqp/functional/gles3/shadertexturefunction/' +
        'texture.html',
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
    self.Fail('deqp/functional/gles3/shadertexturefunction/' +
        'texelfetchoffset.html',
        ['mac', 'intel'], bug=483282)
    self.Fail('deqp/functional/gles3/shadertexturefunction/' +
        'texturesize.html',
        ['mac', 'intel'], bug=483282)

    self.Fail('deqp/functional/gles3/framebufferblit/rect_02.html',
        ['mac', 'intel'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/rect_03.html',
        ['mac', 'intel'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/rect_04.html',
        ['mac', 'intel'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/rect_05.html',
        ['mac', 'intel'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/rect_06.html',
        ['mac', 'intel'], bug=483282)

    # Linux only.
    self.Fail('conformance2/glsl3/vector-dynamic-indexing.html',
        ['linux'], bug=483282)

    self.Fail('deqp/functional/gles3/fbodepthbuffer.html',
        ['linux'], bug=483282)

    # Behavior difference between GL compatibility profile and ES3.
    self.Fail('conformance2/rendering/draw-buffers.html',
        ['linux'], bug=617410)
    self.Fail('deqp/functional/gles3/texturespecification/' +
        'random_teximage2d_cube.html',
        ['linux'], bug=483282)
    self.Fail('deqp/functional/gles3/fboinvalidate/whole.html',
        ['linux'], bug=624506)

    self.Fail('deqp/data/gles3/shaders/functions.html',
        ['linux', 'amd', 'intel'], bug=483282)
    self.Skip('deqp/data/gles3/shaders/qualification_order.html',
        ['linux', 'amd', 'intel'], bug=483282)
    self.Fail('deqp/functional/gles3/clipping.html',
        ['linux', 'amd', 'intel'], bug=483282)
    self.Flaky('deqp/functional/gles3/texturespecification/' +
        'random_teximage2d_2d.html',
        ['linux', 'amd', 'intel'], bug=618447)

    # Linux NVIDIA only.
    self.Fail('deqp/functional/gles3/uniformapi/random.html',
        ['linux', 'nvidia'], bug=621178)

    # Linux NVIDIA with ANGLE only
    self.Fail('deqp/functional/gles3/buffercopy.html',
        ['linux', 'nvidia', 'opengl'], bug=483282)
    self.Fail('deqp/functional/gles3/bufferobjectquery.html',
        ['linux', 'nvidia', 'opengl'], bug=483282)
    self.Fail('conformance2/reading/read-pixels-pack-parameters.html',
        ['linux', 'nvidia', 'opengl'], bug=483282)
    self.Fail('conformance2/transform_feedback/transform_feedback.html',
        ['linux', 'nvidia', 'opengl'], bug=483282)
    self.Fail('deqp/functional/gles3/transformfeedback/*.html',
        ['linux', 'nvidia', 'opengl'], bug=618408)
    self.Fail('deqp/functional/gles3/shadercommonfunction.html',
        ['linux', 'nvidia', 'opengl'], bug=618408)
    self.Fail('deqp/functional/gles3/shaderbuiltinvar.html',
        ['linux', 'nvidia', 'opengl'], bug=483282)
    self.Fail('deqp/functional/gles3/shaderpackingfunction.html',
        ['linux', 'nvidia', 'opengl'], bug=483282)
    self.Fail('conformance2/buffers/bound-buffer-size-change-test.html',
        ['linux', 'nvidia', 'opengl'], bug=483282)
    self.Fail('conformance2/textures/misc/tex-unpack-params.html',
        ['linux', 'nvidia', 'opengl'], bug=483282)

    # Linux Intel with ANGLE only
    self.Fail('deqp/functional/gles3/pixelbufferobject.html',
        ['linux', 'intel', 'opengl'], bug=483282)
    self.Fail('deqp/functional/gles3/shaderderivate_*',
        ['linux', 'intel', 'opengl'], bug=618408)

    self.Fail('deqp/functional/gles3/fragmentoutput/*.html',
        ['linux', 'intel', 'opengl'], bug=483282)

    # The Mesa Intel driver has a scoping bug, see
    # https://bugs.freedesktop.org/show_bug.cgi?id=95184
    self.Fail('deqp/data/gles3/shaders/scoping.html',
        ['linux', 'intel'], bug=610800)

    self.Fail('deqp/functional/gles3/framebufferblit/conversion_04.html',
        ['linux', 'intel', 'opengl'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_07.html',
        ['linux', 'intel', 'opengl'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_08.html',
        ['linux', 'intel', 'opengl'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_10.html',
        ['linux', 'intel', 'opengl'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_11.html',
        ['linux', 'intel', 'opengl'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_12.html',
        ['linux', 'intel', 'opengl'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_13.html',
        ['linux', 'intel', 'opengl'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_18.html',
        ['linux', 'intel', 'opengl'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_25.html',
        ['linux', 'intel', 'opengl'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_28.html',
        ['linux', 'intel', 'opengl'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_29.html',
        ['linux', 'intel', 'opengl'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_30.html',
        ['linux', 'intel', 'opengl'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_31.html',
        ['linux', 'intel', 'opengl'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_32.html',
        ['linux', 'intel', 'opengl'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_33.html',
        ['linux', 'intel', 'opengl'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/conversion_34.html',
        ['linux', 'intel', 'opengl'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/' +
        'default_framebuffer_00.html',
        ['linux', 'intel', 'opengl'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/rect_02.html',
        ['linux', 'intel', 'opengl'], bug=483282)

    # Linux AMD only.
    # It looks like AMD shader compiler rejects many valid ES3 semantics.
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
    self.Fail('deqp/functional/gles3/shaderprecision*.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/texturefiltering/3d*',
        ['linux', 'amd'], bug=606114)
    self.Fail('deqp/functional/gles3/fbocompleteness.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/lifetime.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/shadertexturefunction/texture.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/shadertexturefunction/' +
        'textureprojoffset.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/shadertexturefunction/' +
        'textureprojlodoffset.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/shadertexturefunction/texturegrad.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/shadertexturefunction/' +
        'texelfetchoffset.html',
        ['linux', 'amd'], bug=483282)

    self.Fail('deqp/functional/gles3/instancedrendering.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/negativetextureapi.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/transformfeedback/*.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/uniformbuffers/random.html',
        ['linux', 'amd'], bug=483282)

    self.Fail('conformance2/misc/uninitialized-test-2.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('conformance2/reading/read-pixels-pack-parameters.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('conformance2/reading/read-pixels-into-pixel-pack-buffer.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('conformance2/renderbuffers/framebuffer-texture-layer.html',
        ['linux', 'amd'], bug=295792)
    self.Fail('conformance2/textures/misc/tex-mipmap-levels.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('conformance2/textures/misc/tex-unpack-params.html',
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
    self.Fail('conformance2/reading/read-pixels-from-fbo-test.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/vertexarrays/' +
        'single_attribute.output_type.unsigned_int.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/draw/*.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/fbomultisample*',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/fragmentoutput/*.html',
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
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/framebufferblit/rect_02.html',
        ['linux', 'amd'], bug=483282)

    self.Fail('deqp/functional/gles3/shaderoperator/unary_operator_01.html',
        ['linux', 'amd'], bug=483282)
    self.Fail('deqp/functional/gles3/shaderoperator/unary_operator_02.html',
        ['linux', 'amd'], bug=483282)

    # Conflicting expectations to test that the
    # "Expectations Have No collisions" unittest works.
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
