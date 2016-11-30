# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os

from gpu_tests.gpu_test_expectations import GpuTestExpectations

# See the GpuTestExpectations class for documentation.

class WebGLConformanceExpectations(GpuTestExpectations):
  def __init__(self, conformance_path, url_prefixes=None):
    self.conformance_path = conformance_path
    super(WebGLConformanceExpectations, self).__init__(
      url_prefixes=url_prefixes)

  def Fail(self, pattern, condition=None, bug=None):
    self.CheckPatternIsValid(pattern)
    GpuTestExpectations.Fail(self, pattern, condition, bug)

  def Flaky(self, pattern, condition=None, bug=None, max_num_retries=2):
    self.CheckPatternIsValid(pattern)
    GpuTestExpectations.Flaky(self, pattern, condition, bug=bug,
        max_num_retries=max_num_retries)

  def Skip(self, pattern, condition=None, bug=None):
    self.CheckPatternIsValid(pattern)
    GpuTestExpectations.Skip(self, pattern, condition, bug)

  def CheckPatternIsValid(self, pattern):
    # Look for basic wildcards.
    if not '*' in pattern and not 'WebglExtension_' in pattern:
      full_path = os.path.normpath(os.path.join(self.conformance_path, pattern))
      if not os.path.exists(full_path):
        raise Exception('The WebGL conformance test path specified in ' +
          'expectation does not exist: ' + full_path)

  def SetExpectations(self):
    # ===================================
    # Extension availability expectations
    # ===================================
    # It's expected that not all extensions will be available on all platforms.
    # Having a test listed here is not necessarily a problem.

    self.Fail('WebglExtension_EXT_color_buffer_float',
        ['win', 'mac'])
    # Skip these, rather than expect them to fail, to speed up test
    # execution. The browser is restarted even after expected test
    # failures.
    self.Skip('WebglExtension_WEBGL_compressed_texture_astc',
        ['win', 'mac', 'linux'])
    self.Skip('WebglExtension_WEBGL_compressed_texture_atc',
        ['win', 'mac', 'linux'])
    self.Skip('WebglExtension_WEBGL_compressed_texture_etc1',
        ['mac', 'linux'])
    self.Skip('WebglExtension_WEBGL_compressed_texture_pvrtc',
        ['win', 'mac', 'linux'])
    self.Skip('WebglExtension_WEBGL_compressed_texture_s3tc_srgb',
        ['win', 'mac', 'linux', 'android'])

    # Extensions not available under D3D9
    self.Fail('WebglExtension_EXT_disjoint_timer_query',
        ['win', 'd3d9'])
    self.Fail('WebglExtension_EXT_sRGB',
        ['win', 'd3d9'])
    self.Fail('WebglExtension_WEBGL_compressed_texture_etc1',
        ['win', 'd3d9'])

    self.Fail('WebglExtension_WEBGL_depth_texture',
        ['win', 'amd', 'd3d9'])

    self.Fail('WebglExtension_WEBGL_draw_buffers',
        ['win', 'd3d9'])

    # Android general
    self.Fail('WebglExtension_EXT_disjoint_timer_query',
        ['android'])
    self.Fail('WebglExtension_EXT_frag_depth',
        ['android'])
    self.Fail('WebglExtension_EXT_shader_texture_lod',
        ['android'])
    self.Fail('WebglExtension_WEBGL_compressed_texture_astc',
        ['android'])
    self.Fail('WebglExtension_WEBGL_compressed_texture_pvrtc',
        ['android'])
    self.Fail('WebglExtension_WEBGL_compressed_texture_s3tc',
        ['android'])
    self.Fail('WebglExtension_WEBGL_depth_texture',
        ['android'])
    self.Fail('WebglExtension_WEBGL_draw_buffers',
        ['android'])

    # Nexus 5
    self.Fail('WebglExtension_OES_texture_float_linear',
              ['android', ('qualcomm', 'Adreno (TM) 330')])

    # Nexus 5X
    self.Fail('WebglExtension_EXT_sRGB',
              ['android', ('qualcomm', 'Adreno (TM) 418')], bug=610951)

    # Nexus 6 (Adreno 420) and 6P (Adreno 430)
    self.Fail('WebglExtension_EXT_sRGB',
              ['android',
               ('qualcomm', 'Adreno (TM) 420'),
               ('qualcomm', 'Adreno (TM) 430')])

    # Nexus 9
    self.Fail('WebglExtension_WEBGL_compressed_texture_atc',
              ['android', ('nvidia', 'NVIDIA Tegra')])

    # ========================
    # Conformance expectations
    # ========================
    # Fails on all platforms

    # Need to add detection of feedback loops with multiple render targets.
    self.Fail('conformance/extensions/webgl-draw-buffers-feedback-loop.html',
        bug=1619) # angle bug ID

    # We need to add WebGL 1 check in command buffer that format/type from
    # TexSubImage2D have to match the current texture's.
    self.Fail('conformance/textures/misc/tex-sub-image-2d-bad-args.html',
        bug=570453)

    # Win failures
    # Note that the following test seems to pass, but it may still be flaky.
    self.Fail('conformance/glsl/constructors/' +
              'glsl-construct-vec-mat-index.html',
              ['win'], bug=525188)
    self.Fail('conformance/rendering/point-specific-shader-variables.html',
        ['win'], bug=616335)
    self.Fail('deqp/data/gles2/shaders/functions.html',
        ['win'], bug=478572)

    # Win NVIDIA failures
    self.Flaky('conformance/textures/misc/texture-npot-video.html',
        ['win', 'nvidia'], bug=626524)
    self.Flaky('conformance/textures/misc/texture-upload-size.html',
        ['win', 'nvidia'], bug=630860)

    # Win7 / Intel failures
    self.Fail('conformance/textures/misc/' +
              'copy-tex-image-and-sub-image-2d.html',
              ['win7', 'intel'])

    # Win / AMD flakiness seen on new tryservers.
    # It's unfortunate that this suppression needs to be so broad, but
    # basically any test that uses readPixels is potentially flaky, and
    # it's infeasible to suppress individual failures one by one.
    self.Flaky('conformance/*', ['win', ('amd', 0x6779)], bug=491419)

    # Win AMD failures
    # This test is probably flaky on all AMD, but only visible on the
    # new AMD (the whole test suite is flaky on the old config).
    # Mark as Fail since it often flakes in all 3 retries
    self.Fail('conformance/extensions/oes-texture-half-float.html',
              ['win', ('amd', 0x6613)], bug=653533)

    # Win / AMD D3D9 failures
    self.Fail('conformance/extensions/angle-instanced-arrays.html',
        ['win', 'amd', 'd3d9'], bug=475095)
    self.Fail('conformance/rendering/more-than-65536-indices.html',
        ['win', 'amd', 'd3d9'], bug=475095)

    # Win / D3D9 failures
    # Skipping these two tests because they're causing assertion failures.
    self.Skip('conformance/extensions/oes-texture-float-with-canvas.html',
        ['win', 'd3d9'], bug=896) # angle bug ID
    self.Skip('conformance/extensions/oes-texture-half-float-with-canvas.html',
        ['win', 'd3d9'], bug=896) # angle bug ID
    self.Fail('conformance/glsl/bugs/floor-div-cos-should-not-truncate.html',
        ['win', 'd3d9'], bug=1179) # angle bug ID
    # The functions test have been persistently flaky on D3D9
    self.Flaky('conformance/glsl/functions/*',
        ['win', 'd3d9'], bug=415609)
    self.Flaky('conformance/glsl/matrices/glsl-mat4-to-mat3.html',
        ['win', 'd3d9'], bug=617148)
    self.Flaky('conformance/glsl/matrices/glsl-mat3-construction.html',
        ['win', 'd3d9'], bug=617148)

    # WIN / D3D9 / Intel failures
    self.Fail('conformance/ogles/GL/cos/cos_001_to_006.html',
        ['win', 'intel', 'd3d9'], bug=540538)

    # WIN / OpenGL / NVIDIA failures
    # Mark ANGLE's OpenGL as flaky on Windows Nvidia
    self.Flaky('conformance/*', ['win', 'nvidia', 'opengl'], bug=582083)

    # Win / OpenGL / AMD failures
    self.Skip('conformance/attribs/gl-bindAttribLocation-aliasing.html',
        ['win', 'amd', 'opengl'], bug=649824)
    self.Flaky('conformance/attribs/gl-bindAttribLocation-matrix.html',
        ['win', ('amd', 0x6779), 'opengl'], bug=649824)
    self.Flaky('conformance/attribs/gl-bindAttribLocation-repeated.html',
        ['win', ('amd', 0x6779), 'opengl'], bug=649824)
    self.Fail('conformance/extensions/webgl-draw-buffers.html',
        ['win', ('amd', 0x6779), 'opengl'], bug=649824)
    self.Skip('conformance/glsl/misc/shader-struct-scope.html',
        ['win', 'amd', 'opengl'], bug=1007) # angle bug ID
    self.Skip('conformance/glsl/misc/shaders-with-invariance.html',
        ['win', 'amd', 'opengl'], bug=1007) # angle bug ID
    self.Fail('conformance/glsl/misc/struct-nesting-of-variable-names.html',
        ['win', 'amd', 'opengl'], bug=1007) # angle bug ID
    self.Fail('conformance/rendering/clipping-wide-points.html',
        ['win', 'amd', 'opengl'], bug=1506) # angle bug ID

    # Mark ANGLE's OpenGL as flaky on Windows Amd
    self.Flaky('conformance/*', ['win', 'amd', 'opengl'], bug=582083)

    # Win / OpenGL / Intel failures
    self.Fail('conformance/glsl/functions/glsl-function-normalize.html',
        ['win', 'intel', 'opengl'], bug=1007) # angle bug ID
    self.Fail('conformance/glsl/misc/shader-struct-scope.html',
        ['win', 'intel', 'opengl'], bug=1007) # angle bug ID
    self.Fail('conformance/uniforms/uniform-default-values.html',
        ['win', 'intel', 'opengl'], bug=1007) # angle bug ID

    # Mac failures
    self.Flaky('conformance/extensions/oes-texture-float-with-video.html',
        ['mac'], bug=599272)

    # Mac AMD failures
    self.Fail('conformance/glsl/bugs/bool-type-cast-bug-int-float.html',
        ['mac', 'amd'], bug=483282)
    self.Fail('conformance/extensions/webgl-draw-buffers.html',
        ['mac', 'amd'], bug=625365)
    self.Fail('conformance/rendering/clipping-wide-points.html',
        ['mac', 'amd'], bug=642822)

    # Mac Intel failures
    self.Flaky('conformance/textures/webgl_canvas/tex-' +
               '2d-rgb-rgb-unsigned_byte.html',
               ['mac', 'intel'], bug=648377)

    # Mac Retina NVidia failures
    self.Fail('conformance/attribs/gl-disabled-vertex-attrib.html',
        ['mac', ('nvidia', 0xfe9)], bug=635081)
    self.Fail('conformance/programs/' +
        'gl-bind-attrib-location-long-names-test.html',
        ['mac', ('nvidia', 0xfe9)], bug=635081)
    self.Fail('conformance/programs/gl-bind-attrib-location-test.html',
        ['mac', ('nvidia', 0xfe9)], bug=635081)
    self.Fail('conformance/renderbuffers/framebuffer-object-attachment.html',
        ['mac', ('nvidia', 0xfe9)], bug=635081)
    self.Fail('conformance/textures/misc/tex-input-validation.html',
        ['mac', ('nvidia', 0xfe9)], bug=635081)

    # Linux failures
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

    # NVIDIA
    self.Flaky('conformance/extensions/oes-element-index-uint.html',
               ['linux', 'nvidia'], bug=524144)
    self.Flaky('conformance/textures/image/' +
               'tex-2d-rgb-rgb-unsigned_byte.html',
               ['linux', 'nvidia'], bug=596622)
    # AMD
    self.Flaky('conformance/more/functions/uniformi.html',
               ['linux', 'amd'], bug=550989)
    self.Fail('conformance/rendering/clipping-wide-points.html',
        ['linux', 'amd'], bug=642822)

    # AMD Radeon 6450 and/or R7 240
    self.Fail('conformance/extensions/angle-instanced-arrays.html',
        ['linux', 'amd', 'no_angle'], bug=479260)
    self.Flaky('conformance/extensions/ext-texture-filter-anisotropic.html',
        ['linux', ('amd', 0x6779)], bug=436212)
    self.Flaky('conformance/glsl/misc/shader-struct-scope.html',
        ['linux', ('amd', 0x6779)], bug=436212)
    self.Flaky('conformance/glsl/misc/struct-nesting-of-variable-names.html',
        ['linux', ('amd', 0x6779)], bug=436212)
    self.Flaky('conformance/rendering/point-size.html',
        ['linux', ('amd', 0x6779)], bug=436212)
    self.Flaky('conformance/textures/misc/texture-sub-image-cube-maps.html',
        ['linux', ('amd', 0x6779)], bug=436212)
    self.Flaky('conformance/more/functions/uniformf.html',
        ['linux', ('amd', 0x6779)], bug=436212)
    self.Fail('conformance/glsl/misc/shaders-with-invariance.html',
        ['linux', 'amd'], bug=479952)
    self.Flaky('conformance/textures/misc/texture-mips.html',
        ['linux', ('amd', 0x6779)], bug=479981)
    self.Flaky('conformance/textures/misc/texture-size-cube-maps.html',
        ['linux', ('amd', 0x6779)], bug=479983)
    self.Flaky('conformance/uniforms/uniform-default-values.html',
        ['linux', ('amd', 0x6779)], bug=482013)
    self.Flaky('conformance/glsl/samplers/glsl-function-texture2dlod.html',
        ['linux', ('amd', 0x6779)], bug=436212)
    self.Flaky('conformance/glsl/samplers/glsl-function-texture2dprojlod.html',
        ['linux', ('amd', 0x6779)], bug=436212)
    # Intel
    # See https://bugs.freedesktop.org/show_bug.cgi?id=94477
    self.Skip('conformance/glsl/bugs/temp-expressions-should-not-crash.html',
        ['linux', 'intel'], bug=540543)  # GPU timeout
    self.Skip('conformance/uniforms/gl-uniform-arrays.html',
        ['linux', 'debug', ('intel', 0x412)], bug=604140)
    # Fixed on Mesa 12.0
    self.Fail('conformance/rendering/clipping-wide-points.html',
        ['linux', 'intel'], bug=642822)

    ####################
    # Android failures #
    ####################

    self.Fail('conformance/glsl/bugs/sequence-operator-evaluation-order.html',
        ['android'], bug=478572)
    # The following test is very slow and therefore times out on Android bot.
    self.Skip('conformance/rendering/multisample-corruption.html',
        ['android'])
    self.Fail('conformance/textures/image/tex-2d-rgb-rgb-unsigned_byte.html',
        ['android'], bug=586183)
    # The following tests timed out on android, so skip them for now.
    self.Skip('conformance/textures/image_bitmap_from_video/*',
        ['android'], bug=585108)
    # The following WebView crashes are causing problems with further
    # tests in the suite, so skip them for now.
    self.Skip('conformance/textures/video/' +
        'tex-2d-rgb-rgb-unsigned_byte.html',
        ['android', 'android-webview-shell'], bug=352645)
    self.Skip('conformance/textures/video/' +
        'tex-2d-rgb-rgb-unsigned_short_5_6_5.html',
        ['android', 'android-webview-shell'], bug=352645)
    self.Skip('conformance/textures/video/' +
        'tex-2d-rgba-rgba-unsigned_byte.html',
        ['android', 'android-webview-shell'], bug=352645)
    self.Skip('conformance/textures/video/' +
        'tex-2d-rgba-rgba-unsigned_short_4_4_4_4.html',
        ['android', 'android-webview-shell'], bug=352645)
    self.Skip('conformance/textures/video/' +
        'tex-2d-rgba-rgba-unsigned_short_5_5_5_1.html',
        ['android', 'android-webview-shell'], bug=352645)
    self.Skip('conformance/textures/misc/texture-npot-video.html',
        ['android', 'android-webview-shell'], bug=352645)
    # These are failing on both the Nexus 5 and 6
    self.Fail('conformance/extensions/oes-texture-float-with-canvas.html',
              ['android', 'qualcomm'], bug=499555)
    # This crashes in Android WebView on the Nexus 6, preventing the
    # suite from running further. Rather than add multiple
    # suppressions, skip it until it's passing at least in content
    # shell.
    self.Skip('conformance/extensions/oes-texture-float-with-video.html',
              ['android', 'qualcomm'], bug=499555)
    self.Fail('deqp/data/gles2/shaders/constants.html',
        ['android'], bug=478572)
    self.Fail('deqp/data/gles2/shaders/conversions.html',
        ['android'], bug=478572)
    self.Fail('deqp/data/gles2/shaders/declarations.html',
        ['android'], bug=478572)
    self.Fail('deqp/data/gles2/shaders/linkage.html',
        ['android'], bug=478572)

    # Nexus 5
    self.Fail('conformance/extensions/angle-instanced-arrays.html',
              ['android', ('qualcomm', 'Adreno (TM) 330')], bug=611943)
    self.Fail('conformance/extensions/ext-texture-filter-anisotropic.html',
              ['android', ('qualcomm', 'Adreno (TM) 330')], bug=611943)
    self.Fail('conformance/extensions/oes-texture-half-float.html',
              ['android', ('qualcomm', 'Adreno (TM) 330')], bug=611943)
    self.Fail('conformance/extensions/oes-texture-half-float-with-canvas.html',
              ['android', ('qualcomm', 'Adreno (TM) 330')], bug=611943)
    self.Fail('conformance/extensions/webgl-compressed-texture-atc.html',
              ['android', ('qualcomm', 'Adreno (TM) 330')], bug=611943)
    self.Fail('conformance/glsl/bugs/' +
              'array-of-struct-with-int-first-position.html',
              ['android', ('qualcomm', 'Adreno (TM) 330')], bug=611943)
    self.Fail('conformance/glsl/bugs/gl-fragcoord-multisampling-bug.html',
              ['android', ('qualcomm', 'Adreno (TM) 330')], bug=611943)
    self.Fail('conformance/glsl/bugs/qualcomm-loop-with-continue-crash.html',
              ['android', ('qualcomm', 'Adreno (TM) 330')], bug=527761)
    self.Fail('conformance/glsl/bugs/sampler-struct-function-arg.html',
              ['android', ('qualcomm', 'Adreno (TM) 330')], bug=611943)
    self.Fail('conformance/glsl/bugs/sketchfab-lighting-shader-crash.html',
              ['android', ('qualcomm', 'Adreno (TM) 330')], bug=551937)
    self.Fail('conformance/glsl/bugs/struct-constructor-highp-bug.html',
              ['android', ('qualcomm', 'Adreno (TM) 330')], bug=559342)
    self.Fail('conformance/glsl/matrices/glsl-mat4-to-mat3.html',
              ['android', ('qualcomm', 'Adreno (TM) 330')], bug=611943)
    self.Fail('conformance/glsl/misc/shader-struct-scope.html',
              ['android', ('qualcomm', 'Adreno (TM) 330')], bug=611943)
    self.Fail('conformance/glsl/misc/' +
              'shader-with-vec4-vec3-vec4-conditional.html',
              ['android', ('qualcomm', 'Adreno (TM) 330')], bug=611943)
    self.Fail('conformance/glsl/misc/shaders-with-invariance.html',
              ['android', ('qualcomm', 'Adreno (TM) 330')], bug=611943)
    self.Fail('conformance/glsl/misc/struct-equals.html',
              ['android', ('qualcomm', 'Adreno (TM) 330')], bug=611943)
    self.Fail('conformance/state/state-uneffected-after-compositing.html',
              ['android', ('qualcomm', 'Adreno (TM) 330')], bug=611943)

    # Nexus 5X
    self.Fail('conformance/extensions/oes-texture-float-with-image-data.html',
              ['android', ('qualcomm', 'Adreno (TM) 418')], bug=609883)
    self.Fail('conformance/extensions/oes-texture-float-with-image.html',
              ['android', ('qualcomm', 'Adreno (TM) 418')], bug=609883)
    self.Fail('conformance/extensions/oes-texture-half-float-with-canvas.html',
              ['android', ('qualcomm', 'Adreno (TM) 418')], bug=609883)
    self.Fail('conformance/extensions/' +
              'oes-texture-half-float-with-image-data.html',
              ['android', ('qualcomm', 'Adreno (TM) 418')], bug=609883)
    self.Fail('conformance/extensions/oes-texture-half-float-with-image.html',
              ['android', ('qualcomm', 'Adreno (TM) 418')], bug=609883)
    # This one is causing intermittent timeouts on the device, and it
    # looks like when that happens, the next test also always times
    # out. Skip it for now until it's fixed and running reliably.
    self.Skip('conformance/extensions/oes-texture-half-float-with-video.html',
              ['android', ('qualcomm', 'Adreno (TM) 418')], bug=609883)
    self.Fail('conformance/extensions/webgl-compressed-texture-atc.html',
              ['android', ('qualcomm', 'Adreno (TM) 418')], bug=609883)
    self.Fail('conformance/glsl/bugs/sampler-struct-function-arg.html',
              ['android', ('qualcomm', 'Adreno (TM) 418')], bug=609883)
    # This test is skipped because it is crashing the GPU process.
    self.Skip('conformance/glsl/misc/shader-with-non-reserved-words.html',
              ['android', ('qualcomm', 'Adreno (TM) 418')], bug=609883)
    self.Fail('conformance/textures/misc/' +
              'tex-image-and-sub-image-2d-with-array-buffer-view.html',
              ['android', ('qualcomm', 'Adreno (TM) 418')], bug=610951)
    self.Fail('conformance/textures/canvas/*',
              ['android', ('qualcomm', 'Adreno (TM) 418')], bug=610951)
    self.Fail('conformance/textures/image/*',
              ['android', ('qualcomm', 'Adreno (TM) 418')], bug=610951)
    self.Fail('conformance/textures/image_bitmap_from_canvas/*',
              ['android', ('qualcomm', 'Adreno (TM) 418')], bug=610951)
    self.Fail('conformance/textures/image_bitmap_from_blob/*',
              ['android', ('qualcomm', 'Adreno (TM) 418')], bug=610951)
    self.Fail('conformance/textures/image_bitmap_from_image/*',
              ['android', ('qualcomm', 'Adreno (TM) 418')], bug=610951)
    self.Fail('conformance/textures/image_bitmap_from_image_bitmap/*',
              ['android', ('qualcomm', 'Adreno (TM) 418')], bug=610951)
    self.Fail('conformance/textures/image_bitmap_from_image_data/*',
              ['android', ('qualcomm', 'Adreno (TM) 418')], bug=610951)
    self.Fail('conformance/textures/image_data/*',
              ['android', ('qualcomm', 'Adreno (TM) 418')], bug=610951)
    self.Fail('conformance/textures/misc/' +
              'copy-tex-sub-image-2d-partial-texture.html',
              ['android', ('qualcomm', 'Adreno (TM) 418')], bug=643361)
    self.Fail('conformance/textures/svg_image/*',
              ['android', ('qualcomm', 'Adreno (TM) 418')], bug=610951)
    self.Fail('conformance/textures/video/*',
              ['android', 'android-content-shell', 'android-chromium',
               ('qualcomm', 'Adreno (TM) 418')], bug=610951)
    self.Fail('conformance/textures/webgl_canvas/*',
              ['android', ('qualcomm', 'Adreno (TM) 418')], bug=610951)
    self.Fail('conformance/uniforms/uniform-samplers-test.html',
              ['android', ('qualcomm', 'Adreno (TM) 418')], bug=610951)

    # Nexus 6 (Adreno 420) and 6P (Adreno 430)
    self.Fail('conformance/context/' +
              'context-attributes-alpha-depth-stencil-antialias.html',
              ['android', ('qualcomm', 'Adreno (TM) 420')], bug=499555)
    self.Fail('conformance/context/context-size-change.html',
              ['android', ('qualcomm', 'Adreno (TM) 420')], bug=611945)
    self.Fail('conformance/context/premultiplyalpha-test.html',
              ['android', ('qualcomm', 'Adreno (TM) 420')], bug=499555)
    self.Fail('conformance/extensions/oes-texture-float-with-image-data.html',
              ['android',
               ('qualcomm', 'Adreno (TM) 420'),
               ('qualcomm', 'Adreno (TM) 430')], bug=499555)
    self.Fail('conformance/extensions/oes-texture-float-with-image.html',
              ['android',
               ('qualcomm', 'Adreno (TM) 420'),
               ('qualcomm', 'Adreno (TM) 430')], bug=499555)
    self.Fail('conformance/extensions/oes-texture-half-float.html',
              ['android', ('qualcomm', 'Adreno (TM) 420')], bug=611945)
    self.Fail('conformance/extensions/oes-texture-half-float-with-canvas.html',
              ['android',
               ('qualcomm', 'Adreno (TM) 420'),
               ('qualcomm', 'Adreno (TM) 430')], bug=611945)
    self.Fail('conformance/extensions/' +
              'oes-texture-half-float-with-image-data.html',
              ['android',
               ('qualcomm', 'Adreno (TM) 420'),
               ('qualcomm', 'Adreno (TM) 430')], bug=611945)
    self.Fail('conformance/extensions/oes-texture-half-float-with-image.html',
              ['android',
               ('qualcomm', 'Adreno (TM) 420'),
               ('qualcomm', 'Adreno (TM) 430')], bug=611945)
    self.Fail('conformance/extensions/oes-texture-half-float-with-video.html',
              ['android',
               ('qualcomm', 'Adreno (TM) 420'),
               ('qualcomm', 'Adreno (TM) 430')], bug=611945)
    self.Fail('conformance/extensions/webgl-compressed-texture-atc.html',
              ['android',
               ('qualcomm', 'Adreno (TM) 420'),
               ('qualcomm', 'Adreno (TM) 430')], bug=611945)
    self.Fail('conformance/glsl/bugs/gl-fragcoord-multisampling-bug.html',
              ['android', ('qualcomm', 'Adreno (TM) 420')], bug=611945)
    self.Fail('conformance/glsl/bugs/qualcomm-crash.html',
              ['android', ('qualcomm', 'Adreno (TM) 420')], bug=611945)
    self.Fail('conformance/glsl/bugs/sampler-struct-function-arg.html',
              ['android',
               ('qualcomm', 'Adreno (TM) 420'),
               ('qualcomm', 'Adreno (TM) 430')], bug=611945)
    # This test is skipped because running it causes a future test to fail.
    # The list of tests which may be that future test is very long. It is
    # almost (but not quite) every webgl conformance test.
    self.Skip('conformance/glsl/misc/shader-struct-scope.html',
              ['android', ('qualcomm', 'Adreno (TM) 420')], bug=614550)
    self.Fail('conformance/glsl/misc/shaders-with-invariance.html',
              ['android', ('qualcomm', 'Adreno (TM) 420')], bug=611945)
    self.Fail('conformance/glsl/misc/shader-with-_webgl-identifier.vert.html',
              ['android', ('qualcomm', 'Adreno (TM) 420')], bug=611945)
    # bindBufferBadArgs is causing the GPU thread to crash, taking
    # down the WebView shell, causing the next test to fail and
    # subsequent tests to be aborted.
    self.Skip('conformance/more/functions/bindBufferBadArgs.html',
              ['android', 'android-webview-shell',
               ('qualcomm', 'Adreno (TM) 420')], bug=499874)
    self.Fail('conformance/rendering/gl-scissor-test.html',
              ['android', ('qualcomm', 'Adreno (TM) 420')], bug=499555)
    self.Fail('conformance/rendering/gl-viewport-test.html',
              ['android', ('qualcomm', 'Adreno (TM) 420')], bug=611945)
    self.Fail('conformance/textures/canvas/*',
              ['android',
               ('qualcomm', 'Adreno (TM) 420'),
               ('qualcomm', 'Adreno (TM) 430')], bug=499555)
    self.Fail('conformance/textures/image/*',
              ['android', ('qualcomm', 'Adreno (TM) 420')], bug=499555)
    self.Fail('conformance/textures/image/*',
              ['android', ('qualcomm', 'Adreno (TM) 430')], bug=611945)
    self.Fail('conformance/textures/image_bitmap_from_blob/*',
              ['android', ('qualcomm', 'Adreno (TM) 420')], bug=585108)
    self.Fail('conformance/textures/image_bitmap_from_blob/*',
              ['android', ('qualcomm', 'Adreno (TM) 430')], bug=611945)
    self.Fail('conformance/textures/image_bitmap_from_canvas/*',
              ['android', ('qualcomm', 'Adreno (TM) 420')], bug=585108)
    self.Fail('conformance/textures/image_bitmap_from_canvas/*',
              ['android', ('qualcomm', 'Adreno (TM) 430')], bug=611945)
    self.Fail('conformance/textures/image_bitmap_from_image/*',
              ['android', ('qualcomm', 'Adreno (TM) 420')], bug=585108)
    self.Fail('conformance/textures/image_bitmap_from_image/*',
              ['android', ('qualcomm', 'Adreno (TM) 430')], bug=611945)
    self.Fail('conformance/textures/image_bitmap_from_image_data/*',
              ['android', ('qualcomm', 'Adreno (TM) 420')], bug=585108)
    self.Fail('conformance/textures/image_bitmap_from_image_data/*',
              ['android', ('qualcomm', 'Adreno (TM) 430')], bug=611945)
    self.Fail('conformance/textures/image_bitmap_from_image_bitmap/*',
              ['android', ('qualcomm', 'Adreno (TM) 420')], bug=598262)
    self.Fail('conformance/textures/image_bitmap_from_image_bitmap/*',
              ['android', ('qualcomm', 'Adreno (TM) 430')], bug=611945)
    self.Fail('conformance/textures/image_data/*',
              ['android', ('qualcomm', 'Adreno (TM) 420')], bug=499555)
    self.Fail('conformance/textures/image_data/*',
              ['android', ('qualcomm', 'Adreno (TM) 430')], bug=611945)
    self.Fail('conformance/textures/misc/' +
              'copy-tex-image-and-sub-image-2d.html',
              ['android', ('qualcomm', 'Adreno (TM) 420')], bug=499555)
    self.Fail('conformance/textures/misc/' +
              'copy-tex-sub-image-2d-partial-texture.html',
              ['android',
               ('qualcomm', 'Adreno (TM) 420'),
               ('qualcomm', 'Adreno (TM) 430')], bug=643361)
    self.Fail('conformance/textures/misc/' +
              'tex-image-and-sub-image-2d-with-array-buffer-view.html',
              ['android',
               ('qualcomm', 'Adreno (TM) 420'),
               ('qualcomm', 'Adreno (TM) 430')], bug=499555)
    self.Fail('conformance/textures/svg_image/*',
              ['android',
               ('qualcomm', 'Adreno (TM) 420'),
               ('qualcomm', 'Adreno (TM) 430')], bug=611945)
    # The following tests mention android-content-shell and android-chrome,
    # but not webview.
    # This is because webview already has this expectation below
    self.Fail('conformance/textures/video/*',
              ['android', 'android-content-shell', 'android-chromium',
               ('qualcomm', 'Adreno (TM) 420')], bug=499555)
    self.Fail('conformance/textures/video/*',
              ['android', 'android-content-shell', 'android-chromium',
               ('qualcomm', 'Adreno (TM) 430')], bug=611945)
    self.Fail('conformance/textures/webgl_canvas/*',
              ['android', ('qualcomm', 'Adreno (TM) 420')], bug=499555)
    self.Fail('conformance/textures/webgl_canvas/*',
              ['android', ('qualcomm', 'Adreno (TM) 430')], bug=611945)
    self.Flaky('conformance/uniforms/uniform-samplers-test.html',
              ['android', ('qualcomm', 'Adreno (TM) 430')], bug=663071)

    # Nexus 9
    self.Fail('deqp/data/gles2/shaders/functions.html',
              ['android', 'nvidia'], bug=478572)
    self.Skip('conformance/extensions/oes-texture-float-with-video.html',
              ['android', 'nvidia'], bug=499555) # flaky
    self.Fail('conformance/glsl/bugs/multiplication-assignment.html',
              ['android', 'nvidia'], bug=606096)
    self.Fail('conformance/glsl/constructors/glsl-construct-mat2.html',
              ['android', 'nvidia'], bug=606096)
    self.Fail('conformance/glsl/constructors/glsl-construct-mat3.html',
              ['android', 'nvidia'], bug=606096)
    self.Fail('conformance/glsl/constructors/glsl-construct-mat4.html',
              ['android', 'nvidia'], bug=606096)

    # Pixel C
    self.Flaky('conformance/textures/video/' +
               'tex-2d-rgb-rgb-unsigned_byte.html',
               ['android', 'android-chromium',
                ('nvidia', 'NVIDIA Tegra')], bug=624621)
    self.Flaky('conformance/textures/video/' +
               'tex-2d-rgb-rgb-unsigned_short_5_6_5.html',
               ['android', 'android-chromium',
                ('nvidia', 'NVIDIA Tegra')], bug=624621)
    self.Flaky('conformance/textures/video/tex-2d-rgba-rgba-unsigned_byte.html',
               ['android', 'android-chromium',
                ('nvidia', 'NVIDIA Tegra')], bug=624621)
    self.Flaky('conformance/textures/video/' +
               'tex-2d-rgba-rgba-unsigned_short_4_4_4_4.html',
               ['android', 'android-chromium',
                ('nvidia', 'NVIDIA Tegra')], bug=624621)
    self.Flaky('conformance/textures/video/' +
               'tex-2d-rgba-rgba-unsigned_short_5_5_5_1.html',
               ['android', 'android-chromium',
                ('nvidia', 'NVIDIA Tegra')], bug=624621)
    self.Fail('conformance/glsl/bugs/constant-precision-qualifier.html',
              ['android', 'android-chromium',
               ('nvidia', 'NVIDIA Tegra')], bug=624621)

    ############
    # ChromeOS #
    ############

    # ChromeOS: affecting all devices.
    self.Fail('conformance/extensions/webgl-depth-texture.html',
        ['chromeos'], bug=382651)

    # ChromeOS: all Intel except for pinetrail (stumpy, parrot, peppy,...)
    # We will just include pinetrail here for now as we don't want to list
    # every single Intel device ID.
    self.Fail('conformance/glsl/misc/empty_main.vert.html',
        ['chromeos', 'intel'], bug=375556)
    self.Fail('conformance/glsl/misc/gl_position_unset.vert.html',
        ['chromeos', 'intel'], bug=375556)
    self.Fail('conformance/glsl/misc/shaders-with-varyings.html',
        ['chromeos', 'intel'], bug=375556)
    self.Fail('conformance/renderbuffers/framebuffer-object-attachment.html',
        ['chromeos', 'intel'], bug=375556)
    self.Fail('conformance/textures/misc/texture-size-limit.html',
        ['chromeos', 'intel'], bug=385361)

    # ChromeOS: pinetrail (alex, mario, zgb).
    self.Fail('conformance/attribs/gl-vertex-attrib-render.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/glsl/functions/glsl-function-atan-xy.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/glsl/functions/glsl-function-cos.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/glsl/functions/glsl-function-sin.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/glsl/variables/gl-frontfacing.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/limits/gl-max-texture-dimensions.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/ogles/GL/acos/acos_001_to_006.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/ogles/GL/asin/asin_001_to_006.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/ogles/GL/atan/atan_001_to_008.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/ogles/GL/build/build_009_to_016.html',
        ['chromeos', ('intel', 0xa011)], bug=378938)
    self.Fail('conformance/ogles/GL/control_flow/control_flow_001_to_008.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/ogles/GL/cos/cos_001_to_006.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/ogles/GL/discard/discard_001_to_002.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/ogles/GL/functions/functions_001_to_008.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/ogles/GL/functions/functions_065_to_072.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/ogles/GL/functions/functions_081_to_088.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/ogles/GL/functions/functions_097_to_104.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/ogles/GL/functions/functions_105_to_112.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/ogles/GL/functions/functions_113_to_120.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/ogles/GL/functions/functions_121_to_126.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail(
        'conformance/ogles/GL/gl_FrontFacing/gl_FrontFacing_001_to_001.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/ogles/GL/log/log_001_to_008.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/ogles/GL/log2/log2_001_to_008.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/ogles/GL/normalize/normalize_001_to_006.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/ogles/GL/sin/sin_001_to_006.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/rendering/point-size.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/rendering/polygon-offset.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/textures/misc/texture-mips.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/textures/misc/texture-npot.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/textures/misc/texture-npot-video.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/textures/misc/texture-size.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/uniforms/gl-uniform-arrays.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Skip('conformance/uniforms/uniform-default-values.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
