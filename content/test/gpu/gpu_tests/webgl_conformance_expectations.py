# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os

from gpu_test_expectations import GpuTestExpectations

# See the GpuTestExpectations class for documentation.

class WebGLConformanceExpectations(GpuTestExpectations):
  def __init__(self, conformance_path):
    self.conformance_path = conformance_path
    super(WebGLConformanceExpectations, self).__init__()

  def Fail(self, pattern, condition=None, bug=None):
    self.CheckPatternIsValid(pattern)
    GpuTestExpectations.Fail(self, pattern, condition, bug)

  def Skip(self, pattern, condition=None, bug=None):
    self.CheckPatternIsValid(pattern)
    GpuTestExpectations.Skip(self, pattern, condition, bug)

  def CheckPatternIsValid(self, pattern):
    # Look for basic wildcards.
    if not '*' in pattern:
      full_path = os.path.normpath(os.path.join(self.conformance_path, pattern))
      if not os.path.exists(full_path):
        raise Exception('The WebGL conformance test path specified in' +
          'expectation does not exist: ' + full_path)

  def SetExpectations(self):
    # Fails on all platforms
    self.Fail('deqp/data/gles2/shaders/functions.html',
        bug=478572)
    self.Fail('deqp/data/gles2/shaders/preprocessor.html',
        bug=478572)
    self.Fail('deqp/data/gles2/shaders/scoping.html',
        bug=478572)
    self.Fail('conformance/misc/expando-loss.html',
        bug=485634)

    # Win failures
    self.Fail('conformance/glsl/bugs/' +
              'pow-of-small-constant-in-user-defined-function.html',
        ['win'], bug=485641)
    self.Fail('conformance/glsl/bugs/sampler-struct-function-arg.html',
        ['win'], bug=485642)
    self.Fail('conformance/glsl/constructors/' +
              'glsl-construct-vec-mat-index.html',
              ['win'], bug=525188)

    # Win7 / Intel failures
    self.Fail('conformance/rendering/gl-scissor-test.html',
        ['win7', 'intel'], bug=314997)
    self.Fail('conformance/context/premultiplyalpha-test.html',
        ['win7', 'intel'])
    self.Fail('conformance/textures/misc/copy-tex-image-and-sub-image-2d.html',
        ['win7', 'intel'])
    self.Fail('conformance/rendering/gl-viewport-test.html',
        ['win7', 'intel'], bug=372511)

    # Win / AMD flakiness seen on new tryservers.
    # It's unfortunate that this suppression needs to be so broad, but
    # basically any test that uses readPixels is potentially flaky, and
    # it's infeasible to suppress individual failures one by one.
    self.Flaky('conformance/*', ['win', ('amd', 0x6779)], bug=491419)

    # Win / AMD D3D9 failures
    self.Fail('conformance/textures/misc/texparameter-test.html',
        ['win', 'amd', 'd3d9'], bug=839) # angle bug ID
    self.Fail('conformance/extensions/angle-instanced-arrays.html',
        ['win', 'amd', 'd3d9'], bug=475095)
    self.Fail('conformance/rendering/more-than-65536-indices.html',
        ['win', 'amd', 'd3d9'], bug=475095)

    # Win / D3D9 failures
    # Skipping these tests because they're causing assertion failures.
    self.Skip('conformance/extensions/oes-texture-float-with-canvas.html',
        ['win', 'd3d9'], bug=896) # angle bug ID
    self.Skip('conformance/extensions/oes-texture-half-float-with-canvas.html',
        ['win', 'd3d9'], bug=896) # angle bug ID

    # Win / OpenGL failures
    self.Fail('conformance/context/'+
        'context-attributes-alpha-depth-stencil-antialias.html',
        ['win', 'opengl'], bug=1007) # angle bug ID
    self.Fail('deqp/data/gles2/shaders/conditionals.html',
        ['win', 'opengl'], bug=1007) # angle bug ID

    # Win / OpenGL / NVIDIA failures
    self.Fail('conformance/attribs/gl-disabled-vertex-attrib.html',
        ['win', 'nvidia', 'opengl'], bug=1007) # angle bug ID

    # Win / OpenGL / AMD failures
    self.Skip('conformance/glsl/misc/shader-struct-scope.html',
        ['win', 'amd', 'opengl'], bug=1007) # angle bug ID
    self.Fail('conformance/glsl/misc/shaders-with-invariance.html',
        ['win', 'amd', 'opengl'], bug=1007) # angle bug ID
    self.Fail('conformance/glsl/misc/struct-nesting-of-variable-names.html',
        ['win', 'amd', 'opengl'], bug=1007) # angle bug ID
    self.Fail('conformance/rendering/gl-scissor-test.html',
        ['win', 'amd', 'opengl'], bug=1007) # angle bug ID
    self.Fail('deqp/data/gles2/shaders/constant_expressions.html',
        ['win', 'amd', 'opengl'], bug=1007) # angle bug ID
    self.Fail('deqp/data/gles2/shaders/constants.html',
        ['win', 'amd', 'opengl'], bug=1007) # angle bug ID
    self.Fail('deqp/data/gles2/shaders/swizzles.html',
        ['win', 'amd', 'opengl'], bug=1007) # angle bug ID

    # Skip all WebGL CTS on OpenGL+Intel
    self.Skip('*', ['win', 'opengl', 'intel'], bug=1007) # angle bug ID

    # Mac failures
    self.Fail('conformance/glsl/misc/shaders-with-invariance.html',
        ['mac'], bug=421710)

    # Mac / Intel failures
    # Radar 13499466
    self.Fail('conformance/limits/gl-max-texture-dimensions.html',
        ['mac', 'intel'], bug=225642)
    # Radar 13499623
    self.Fail('conformance/textures/misc/texture-size.html',
        ['mac', 'intel'], bug=225642)

    # Mac / Intel HD 3000 failures
    self.Skip('conformance/ogles/GL/control_flow/control_flow_009_to_010.html',
        ['mac', ('intel', 0x116)], bug=322795)
    # Radar 13499677
    self.Fail('conformance/glsl/functions/' +
        'glsl-function-smoothstep-gentype.html',
        ['mac', ('intel', 0x116)], bug=225642)
    self.Fail('conformance/extensions/webgl-draw-buffers.html',
        ['mac', ('intel', 0x116)], bug=369349)

    # Mac 10.8 / Intel HD 3000 failures
    self.Fail('conformance/rendering/gl-scissor-test.html',
        ['mountainlion', ('intel', 0x116)], bug=314997)
    self.Fail('conformance/ogles/GL/operators/operators_009_to_016.html',
        ['mountainlion', ('intel', 0x116)], bug=322795)
    self.Flaky('conformance/ogles/*',
        ['mountainlion', ('intel', 0x116)], bug=527250)

    # Mac 10.8 / Intel HD 4000 failures.
    self.Fail('conformance/context/context-hidden-alpha.html',
        ['mountainlion', ('intel', 0x166)], bug=518008)

    # Mac 10.9 / Intel HD 3000 failures
    self.Fail('conformance/ogles/GL/operators/operators_009_to_016.html',
        ['mavericks', ('intel', 0x116)], bug=417415)
    self.Fail('conformance/rendering/gl-scissor-test.html',
        ['mavericks', ('intel', 0x116)], bug=417415)

    # Mac Retina failures
    self.Fail(
        'conformance/glsl/bugs/array-of-struct-with-int-first-position.html',
        ['mac', ('nvidia', 0xfd5), ('nvidia', 0xfe9)], bug=368912)

    # Mac / AMD Failures
    self.Fail('deqp/data/gles2/shaders/conversions.html',
        ['mac', 'amd'], bug=478572)

    # Mac 10.8 / ATI failures
    self.Fail(
        'conformance/rendering/' +
        'point-with-gl-pointcoord-in-fragment-shader.html',
        ['mountainlion', 'amd'])

    # Mac 10.7 / Intel failures
    self.Skip('conformance/glsl/functions/glsl-function-asin.html',
        ['lion', 'intel'])
    self.Skip('conformance/glsl/functions/glsl-function-dot.html',
        ['lion', 'intel'])
    self.Skip('conformance/glsl/functions/glsl-function-faceforward.html',
        ['lion', 'intel'])
    self.Skip('conformance/glsl/functions/glsl-function-length.html',
        ['lion', 'intel'])
    self.Skip('conformance/glsl/functions/glsl-function-normalize.html',
        ['lion', 'intel'])
    self.Skip('conformance/glsl/functions/glsl-function-reflect.html',
        ['lion', 'intel'])
    self.Skip('conformance/rendering/line-loop-tri-fan.html',
        ['lion', 'intel'])
    self.Skip('conformance/ogles/GL/control_flow/control_flow_001_to_008.html',
        ['lion', 'intel'], bug=345575)
    self.Skip('conformance/ogles/GL/dot/dot_001_to_006.html',
        ['lion', 'intel'], bug=323736)
    self.Skip('conformance/ogles/GL/faceforward/faceforward_001_to_006.html',
        ['lion', 'intel'], bug=323736)
    self.Skip('conformance/ogles/GL/length/length_001_to_006.html',
        ['lion', 'intel'], bug=323736)
    self.Skip('conformance/ogles/GL/normalize/normalize_001_to_006.html',
        ['lion', 'intel'], bug=323736)
    self.Skip('conformance/ogles/GL/reflect/reflect_001_to_006.html',
        ['lion', 'intel'], bug=323736)
    self.Skip('conformance/ogles/GL/refract/refract_001_to_006.html',
        ['lion', 'intel'], bug=323736)
    self.Skip('conformance/ogles/GL/tan/tan_001_to_006.html',
        ['lion', 'intel'], bug=323736)
    # Two flaky tests.
    self.Fail('conformance/ogles/GL/functions/functions_049_to_056.html',
        ['lion', 'intel'], bug=393331)
    self.Fail('conformance/extensions/webgl-compressed-texture-size-limit.html',
        ['lion', 'intel'], bug=393331)

    # Linux failures
    # NVIDIA
    self.Fail('conformance/textures/misc/default-texture.html',
        ['linux', ('nvidia', 0x104a)], bug=422152)
    self.Flaky('conformance/extensions/oes-element-index-uint.html',
               ['linux', 'nvidia'], bug=524144)
    # AMD Radeon 5450
    self.Fail('conformance/programs/program-test.html',
        ['linux', ('amd', 0x68f9)], bug=436212)
    self.Fail('conformance/rendering/multisample-corruption.html',
        ['linux', ('amd', 0x68f9)], bug=436212)
    self.Fail('conformance/textures/misc/default-texture.html',
        ['linux', ('amd', 0x68f9)], bug=436212)
    self.Fail('conformance/textures/video/' +
        'tex-image-and-sub-image-2d-with-video-rgb-rgb-unsigned_byte.html',
        ['linux', ('amd', 0x68f9)], bug=436212)
    self.Fail('conformance/textures/video/tex-image-and-sub-image-2d-with-' +
        'video-rgb-rgb-unsigned_short_5_6_5.html',
        ['linux', ('amd', 0x68f9)], bug=436212)
    self.Fail('conformance/textures/video/' +
        'tex-image-and-sub-image-2d-with-video-rgba-rgba-unsigned_byte.html',
        ['linux', ('amd', 0x68f9)], bug=436212)
    self.Fail('conformance/textures/video/tex-image-and-sub-image-2d-with-' +
        'video-rgba-rgba-unsigned_short_4_4_4_4.html',
        ['linux', ('amd', 0x68f9)], bug=436212)
    self.Fail('conformance/textures/video/tex-image-and-sub-image-2d-with-' +
        'video-rgba-rgba-unsigned_short_5_5_5_1.html',
        ['linux', ('amd', 0x68f9)], bug=436212)
    self.Fail('conformance/textures/webgl_canvas/tex-image-and-sub-image-2d-' +
        'with-webgl-canvas-rgb-rgb-unsigned_byte.html',
        ['linux', ('amd', 0x68f9)], bug=436212)
    self.Fail('conformance/textures/webgl_canvas/tex-image-and-sub-image-2d-' +
        'with-webgl-canvas-rgb-rgb-unsigned_short_5_6_5.html',
        ['linux', ('amd', 0x68f9)], bug=436212)
    self.Fail('conformance/textures/webgl_canvas/tex-image-and-sub-image-2d-' +
        'with-webgl-canvas-rgba-rgba-unsigned_byte.html',
        ['linux', ('amd', 0x68f9)], bug=436212)
    self.Fail('conformance/textures/webgl_canvas/tex-image-and-sub-image-2d-' +
        'with-webgl-canvas-rgba-rgba-unsigned_short_4_4_4_4.html',
        ['linux', ('amd', 0x68f9)], bug=436212)
    self.Fail('conformance/textures/webgl_canvas/tex-image-and-sub-image-2d-' +
        'with-webgl-canvas-rgba-rgba-unsigned_short_5_5_5_1.html',
        ['linux', ('amd', 0x68f9)], bug=436212)
    self.Fail('conformance/textures/misc/texture-mips.html',
        ['linux', ('amd', 0x68f9)], bug=436212)
    self.Fail('conformance/textures/misc/texture-npot-video.html',
        ['linux', ('amd', 0x68f9)], bug=436212)
    self.Fail('conformance/textures/misc/texture-size.html',
        ['linux', ('amd', 0x68f9)], bug=436212)
    self.Fail('conformance/more/functions/copyTexSubImage2D.html',
        ['linux', ('amd', 0x68f9)], bug=436212)
    self.Fail('conformance/more/functions/drawArraysOutOfBounds.html',
        ['linux', ('amd', 0x68f9)], bug=436212)
    self.Fail('conformance/more/functions/texImage2DHTML.html',
        ['linux', ('amd', 0x68f9)], bug=436212)
    self.Fail('conformance/more/functions/texSubImage2DHTML.html',
        ['linux', ('amd', 0x68f9)], bug=436212)
    # AMD Radeon 6450
    self.Fail('conformance/extensions/angle-instanced-arrays.html',
        ['linux', ('amd', 0x6779)], bug=479260)
    self.Fail('conformance/extensions/ext-texture-filter-anisotropic.html',
        ['linux', ('amd', 0x6779)], bug=436212)
    self.Fail('conformance/glsl/misc/shader-struct-scope.html',
        ['linux', ('amd', 0x6779)], bug=436212)
    self.Fail('conformance/glsl/misc/struct-nesting-of-variable-names.html',
        ['linux', ('amd', 0x6779)], bug=436212)
    self.Fail('conformance/rendering/point-size.html',
        ['linux', ('amd', 0x6779)], bug=436212)
    self.Fail('conformance/textures/misc/texture-sub-image-cube-maps.html',
        ['linux', ('amd', 0x6779)], bug=436212)
    self.Fail('conformance/more/functions/uniformf.html',
        ['linux', ('amd', 0x6779)], bug=436212)
    self.Fail('conformance/glsl/misc/shaders-with-invariance.html',
        ['linux', ('amd', 0x6779)], bug=479952)
    self.Fail('conformance/textures/misc/texture-mips.html',
        ['linux', ('amd', 0x6779)], bug=479981)
    self.Fail('conformance/textures/misc/texture-size-cube-maps.html',
        ['linux', ('amd', 0x6779)], bug=479983)
    self.Fail('conformance/uniforms/uniform-default-values.html',
        ['linux', ('amd', 0x6779)], bug=482013)

    # Android failures
    self.Fail('deqp/data/gles2/shaders/constants.html',
        ['android'], bug=478572)
    self.Fail('deqp/data/gles2/shaders/conversions.html',
        ['android'], bug=478572)
    self.Fail('deqp/data/gles2/shaders/declarations.html',
        ['android'], bug=478572)
    self.Fail('deqp/data/gles2/shaders/linkage.html',
        ['android'], bug=478572)
    # The following WebView crashes are causing problems with further
    # tests in the suite, so skip them for now.
    self.Skip('conformance/textures/video/tex-image-and-sub-image-2d-with-' +
        'video-rgb-rgb-unsigned_byte.html',
        ['android', 'android-webview-shell'], bug=352645)
    self.Skip('conformance/textures/video/tex-image-and-sub-image-2d-with-' +
        'video-rgb-rgb-unsigned_short_5_6_5.html',
        ['android', 'android-webview-shell'], bug=352645)
    self.Skip('conformance/textures/video/tex-image-and-sub-image-2d-with-' +
        'video-rgba-rgba-unsigned_byte.html',
        ['android', 'android-webview-shell'], bug=352645)
    self.Skip('conformance/textures/video/tex-image-and-sub-image-2d-with-' +
        'video-rgba-rgba-unsigned_short_4_4_4_4.html',
        ['android', 'android-webview-shell'], bug=352645)
    self.Skip('conformance/textures/video/tex-image-and-sub-image-2d-with-' +
        'video-rgba-rgba-unsigned_short_5_5_5_1.html',
        ['android', 'android-webview-shell'], bug=352645)
    self.Skip('conformance/textures/misc/texture-npot-video.html',
        ['android', 'android-webview-shell'], bug=352645)
    # Recent regressions have caused these to fail on multiple devices
    self.Fail('conformance/textures/video/tex-image-and-sub-image-2d-with-' +
        'video-rgb-rgb-unsigned_byte.html',
        ['android', 'android-content-shell'], bug=499555)
    self.Fail('conformance/textures/video/tex-image-and-sub-image-2d-with-' +
        'video-rgba-rgba-unsigned_byte.html',
        ['android', 'android-content-shell'], bug=499555)
    self.Flaky('conformance/textures/misc/texture-npot-video.html',
        ['android', 'android-content-shell'], bug=520638)
    # These are failing on the Nexus 5 and 6
    self.Fail('conformance/extensions/oes-texture-float-with-canvas.html',
              ['android', 'qualcomm'], bug=499555)
    # This crashes in Android WebView on the Nexus 6, preventing the
    # suite from running further. Rather than add multiple
    # suppressions, skip it until it's passing at least in content
    # shell.
    self.Skip('conformance/extensions/oes-texture-float-with-video.html',
              ['android', 'qualcomm'], bug=499555)
    # Nexus 6 failures only
    self.Fail('conformance/context/' +
              'context-attributes-alpha-depth-stencil-antialias.html',
              ['android', ('qualcomm', 'Adreno (TM) 420')], bug=499555)
    self.Fail('conformance/context/premultiplyalpha-test.html',
              ['android', ('qualcomm', 'Adreno (TM) 420')], bug=499555)
    self.Fail('conformance/extensions/oes-texture-float-with-image-data.html',
              ['android', ('qualcomm', 'Adreno (TM) 420')], bug=499555)
    self.Fail('conformance/extensions/oes-texture-float-with-image.html',
              ['android', ('qualcomm', 'Adreno (TM) 420')], bug=499555)
    self.Fail('conformance/textures/video/tex-image-and-sub-image-2d-with-' +
        'video-rgb-rgb-unsigned_short_5_6_5.html',
        ['android', 'android-content-shell',
         ('qualcomm', 'Adreno (TM) 420')], bug=499555)
    self.Fail('conformance/textures/video/tex-image-and-sub-image-2d-with-' +
        'video-rgba-rgba-unsigned_short_4_4_4_4.html',
        ['android', 'android-content-shell',
         ('qualcomm', 'Adreno (TM) 420')], bug=499555)
    self.Fail('conformance/textures/video/tex-image-and-sub-image-2d-with-' +
        'video-rgba-rgba-unsigned_short_5_5_5_1.html',
        ['android', 'android-content-shell',
         ('qualcomm', 'Adreno (TM) 420')], bug=499555)
    # bindBufferBadArgs is causing the GPU thread to crash, taking
    # down the WebView shell, causing the next test to fail and
    # subsequent tests to be aborted.
    self.Skip('conformance/more/functions/bindBufferBadArgs.html',
              ['android', 'android-webview-shell',
               ('qualcomm', 'Adreno (TM) 420')], bug=499874)
    self.Fail('conformance/rendering/gl-scissor-test.html',
              ['android', ('qualcomm', 'Adreno (TM) 420')], bug=499555)
    self.Fail('conformance/textures/misc/' +
              'copy-tex-image-and-sub-image-2d.html',
              ['android', ('qualcomm', 'Adreno (TM) 420')], bug=499555)
    self.Fail('conformance/textures/misc/' +
              'tex-image-and-sub-image-2d-with-array-buffer-view.html',
              ['android', ('qualcomm', 'Adreno (TM) 420')], bug=499555)
    self.Fail('conformance/textures/canvas/*',
              ['android', ('qualcomm', 'Adreno (TM) 420')], bug=499555)
    self.Fail('conformance/textures/image_data/*',
              ['android', ('qualcomm', 'Adreno (TM) 420')], bug=499555)
    self.Fail('conformance/textures/image/*',
              ['android', ('qualcomm', 'Adreno (TM) 420')], bug=499555)
    self.Fail('conformance/textures/webgl_canvas/*',
              ['android', ('qualcomm', 'Adreno (TM) 420')], bug=499555)
    # Nexus 9 failures
    self.Skip('conformance/extensions/oes-texture-float-with-video.html',
              ['android', 'nvidia'], bug=499555) # flaky

    # The following test is very slow and therefore times out on Android bot.
    self.Skip('conformance/rendering/multisample-corruption.html',
        ['android'])

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

    # Flaky on Mac & Linux
    self.Fail('conformance/textures/misc/texture-upload-size.html',
        ['mac'], bug=436493)
    self.Fail('conformance/textures/misc/texture-upload-size.html',
        ['linux'], bug=436493)

    ##############################################################
    # WEBGL 2 TESTS FAILURES
    ##############################################################

    self.Skip('deqp/data/gles3/shaders/constant_expressions.html', bug=483282)
    self.Skip('deqp/data/gles3/shaders/constants.html', bug=483282)
    self.Skip('deqp/data/gles3/shaders/conversions.html', bug=483282)
    self.Skip('deqp/data/gles3/shaders/functions.html', bug=483282)
    self.Skip('deqp/data/gles3/shaders/linkage.html', bug=483282)
    self.Skip('deqp/data/gles3/shaders/preprocessor.html', bug=483282)

    self.Skip('deqp/framework/opengl/simplereference/referencecontext.html',
        bug=483282)

    self.Skip('deqp/functional/gles3/attriblocation.html', bug=483282)
    self.Skip('deqp/functional/gles3/booleanstatequery.html', bug=483282)
    self.Skip('deqp/functional/gles3/buffercopy.html', bug=483282)
    self.Skip('deqp/functional/gles3/builtinprecision*.html', bug=483282)
    self.Skip('deqp/functional/gles3/clipping.html', bug=483282)
    self.Skip('deqp/functional/gles3/draw.html', bug=483282)
    self.Skip('deqp/functional/gles3/fbocolorbuffer.html', bug=483282)
    self.Skip('deqp/functional/gles3/fbocompleteness.html', bug=483282)
    self.Skip('deqp/functional/gles3/fbodepthbuffer.html', bug=483282)
    self.Skip('deqp/functional/gles3/fboinvalidate.html', bug=483282)
    self.Skip('deqp/functional/gles3/fbomultisample.html', bug=483282)
    self.Skip('deqp/functional/gles3/fborender.html', bug=483282)
    self.Skip('deqp/functional/gles3/fbostatequery.html', bug=483282)
    self.Skip('deqp/functional/gles3/fragdepth.html', bug=483282)
    self.Skip('deqp/functional/gles3/fragmentoutput.html', bug=483282)
    self.Skip('deqp/functional/gles3/framebufferblit.html', bug=483282)
    self.Skip('deqp/functional/gles3/indexedstatequery.html', bug=483282)
    self.Skip('deqp/functional/gles3/instancedrendering.html', bug=483282)
    self.Skip('deqp/functional/gles3/internalformatquery.html', bug=483282)
    self.Skip('deqp/functional/gles3/lifetime.html', bug=483282)
    self.Skip('deqp/functional/gles3/multisample.html', bug=483282)
    self.Skip('deqp/functional/gles3/negativebufferapi.html', bug=483282)
    self.Skip('deqp/functional/gles3/negativefragmentapi.html', bug=483282)
    self.Skip('deqp/functional/gles3/negativeshaderapi.html', bug=483282)
    self.Skip('deqp/functional/gles3/negativestateapi.html', bug=483282)
    self.Skip('deqp/functional/gles3/negativetextureapi.html', bug=483282)
    self.Skip('deqp/functional/gles3/negativevertexarrayapi.html', bug=483282)
    self.Skip('deqp/functional/gles3/occlusionquery.html', bug=483282)
    self.Skip('deqp/functional/gles3/pixelbufferobject.html', bug=483282)
    self.Skip('deqp/functional/gles3/primitiverestart.html', bug=483282)
    self.Skip('deqp/functional/gles3/rasterizerdiscard.html', bug=483282)
    self.Skip('deqp/functional/gles3/samplerobject.html', bug=483282)
    self.Skip('deqp/functional/gles3/shaderbuiltinvar.html', bug=483282)
    self.Skip('deqp/functional/gles3/shadercommonfunction.html', bug=483282)
    self.Skip('deqp/functional/gles3/shaderderivate.html', bug=483282)
    self.Skip('deqp/functional/gles3/shaderindexing.html', bug=483282)
    self.Skip('deqp/functional/gles3/shaderloop.html', bug=483282)
    self.Skip('deqp/functional/gles3/shadermatrix.html', bug=483282)
    self.Skip('deqp/functional/gles3/shaderpackingfunction.html', bug=483282)
    self.Skip('deqp/functional/gles3/shaderprecision.html', bug=483282)
    self.Skip('deqp/functional/gles3/shaderstatequery.html', bug=483282)
    self.Skip('deqp/functional/gles3/shaderstruct.html', bug=483282)
    self.Skip('deqp/functional/gles3/shadertexturefunction*.html', bug=483282)
    self.Skip('deqp/functional/gles3/sync.html', bug=483282)
    self.Skip('deqp/functional/gles3/texturefiltering*.html', bug=483282)
    self.Skip('deqp/functional/gles3/textureformat.html', bug=483282)
    self.Skip('deqp/functional/gles3/textureshadow.html', bug=483282)
    self.Skip('deqp/functional/gles3/texturespecification*.html', bug=483282)
    self.Skip('deqp/functional/gles3/texturewrap.html', bug=483282)
    self.Skip('deqp/functional/gles3/transformfeedback.html', bug=483282)
    self.Skip('deqp/functional/gles3/uniformapi.html', bug=483282)
    self.Skip('deqp/functional/gles3/uniformbuffers.html', bug=483282)
    self.Skip('deqp/functional/gles3/vertexarrays.html', bug=483282)

    self.Fail('conformance2/buffers/uniform-buffers.html', bug=483282)
    self.Fail('conformance2/glsl3/array-complex-indexing.html', bug=483282)
    self.Fail('conformance2/glsl3/frag-depth.html', bug=483282)
    self.Fail('conformance2/glsl3/invalid-default-precision.html', bug=483282)
    self.Fail('conformance2/glsl3/sequence-operator-returns-non-constant.html',
        bug=483282)
    self.Fail('conformance2/glsl3/ternary-operator-on-arrays-glsl3.html',
        bug=483282)
    self.Fail('conformance2/reading/read-pixels-into-pixel-pack-buffer.html',
        bug=483282)
    self.Fail('conformance2/renderbuffers/framebuffer-test.html', bug=483282)
    self.Fail('conformance2/renderbuffers/invalidate-framebuffer.html',
        bug=483282)
    self.Fail('conformance2/samplers/sampler-drawing-test.html', bug=483282)
    self.Skip('conformance2/textures/webgl_canvas/*', bug=483282)
    self.Fail('conformance2/textures/misc/tex-mipmap-levels.html', bug=483282)
    self.Fail('conformance2/textures/misc/tex-storage-2d.html', bug=483282)

    # Windows only.
    self.Skip('deqp/functional/gles3/readpixel.html', ['win'], bug=483282)
    self.Fail('conformance2/glsl3/array-in-complex-expression.html',
        ['win'], bug=483282)
    self.Fail('conformance2/glsl3/short-circuiting-in-loop-condition.html',
        ['win'], bug=483282)
    self.Fail('conformance2/reading/read-pixels-from-fbo-test.html',
        ['win'], bug=483282)
    self.Fail('conformance2/renderbuffers/framebuffer-object-attachment.html',
        ['win'], bug=1082) # angle bug ID
    self.Fail('conformance2/rendering/draw-buffers.html',
        ['win'], bug=483282)
    self.Fail('conformance2/state/gl-get-calls.html',
        ['win'], bug=483282)
    self.Fail('conformance2/state/gl-object-get-calls.html',
        ['win'], bug=483282)
    self.Fail('conformance2/textures/canvas/*', ['win'], bug=483282)
    self.Fail('conformance2/textures/misc/gl-get-tex-parameter.html',
        ['win'], bug=483282)
    self.Fail('conformance2/textures/misc/tex-input-validation.html',
        ['win'], bug=483282)
    # Windows 8 only.
    self.Fail('conformance2/textures/image_data/tex-image-and-sub-image-2d' +
        '-with-image-data-rgb565-rgb-unsigned_byte.html',
        ['win8'], bug=483282)
    self.Fail('conformance2/textures/image_data/tex-image-and-sub-image-2d' +
        '-with-image-data-rgb5_a1-rgba-unsigned_byte.html',
        ['win8'], bug=483282)
    self.Fail('conformance2/textures/image/tex-image-and-sub-image-2d' +
        '-with-image-rgb565-rgb-unsigned_byte.html',
        ['win8'], bug=483282)
    self.Fail('conformance2/textures/image/tex-image-and-sub-image-2d' +
        '-with-image-rgb5_a1-rgba-unsigned_byte.html',
        ['win8'], bug=483282)
    self.Fail('conformance2/textures/svg_image/tex-image-and-sub-image-2d' +
        '-with-svg-image-rgb565-rgb-unsigned_byte.html',
        ['win8'], bug=483282)
    self.Fail('conformance2/textures/svg_image/tex-image-and-sub-image-2d' +
        '-with-svg-image-rgb5_a1-rgba-unsigned_byte.html',
        ['win8'], bug=483282)
    self.Fail('conformance2/textures/video/tex-image-and-sub-image-2d' +
        '-with-video-rgb565-rgb-unsigned_byte.html',
        ['win8'], bug=483282)
    self.Fail('conformance2/textures/video/tex-image-and-sub-image-2d' +
        '-with-video-rgb5_a1-rgba-unsigned_byte.html',
        ['win8'], bug=483282)

    # Mac only.
    self.Skip('deqp/data/gles3/shaders/qualification_order.html',
        ['mac'], bug=483282)
    self.Skip('deqp/data/gles3/shaders/scoping.html',
        ['mac'], bug=483282)
    self.Skip('deqp/functional/gles3/defaultvertexattribute.html',
        ['mac'], bug=483282)
    self.Skip('deqp/functional/gles3/vertexarrayobject.html',
        ['mac'], bug=483282)
    self.Skip('deqp/functional/gles3/shaderswitch.html',
        ['mavericks'], bug=483282)
    self.Fail('conformance2/attribs/gl-vertex-attrib-i-render.html',
        ['mac'], bug=483282)
    self.Fail('conformance2/buffers/buffer-overflow-test.html',
        ['mac'], bug=483282)
    self.Fail('conformance2/buffers/buffer-type-restrictions.html',
        ['mac'], bug=483282)
    self.Fail('conformance2/renderbuffers/' +
        'multisampled-renderbuffer-initialization.html',
        ['mac'], bug=483282)
    self.Fail('conformance2/rendering/instanced-arrays.html',
        ['mac'], bug=483282)
    self.Fail('conformance2/samplers/samplers.html',
        ['mac'], bug=483282)
    self.Fail('conformance2/state/gl-object-get-calls.html',
        ['mac'], bug=483282)
    self.Fail('conformance2/textures/canvas/*', ['mac'], bug=483282)
    self.Fail('conformance2/textures/video/*', ['mac'], bug=483282)
    self.Fail('conformance2/textures/misc/gl-get-tex-parameter.html',
        ['mac'], bug=483282)
    self.Fail('conformance2/textures/misc/tex-storage-compressed-formats.html',
        ['mac'], bug=483282)
    self.Fail('conformance2/textures/misc/texture-npot.html',
        ['mac'], bug=483282)
    # Mac Intel only.
    self.Fail('deqp/data/gles3/shaders/arrays.html', ['mac', 'intel'],
        bug=536887)
    self.Fail('deqp/data/gles3/shaders/conditionals.html', ['mac', 'intel'],
        bug=536887)
    self.Fail('deqp/data/gles3/shaders/declarations.html', ['mac', 'intel'],
        bug=536887)
    self.Fail('deqp/data/gles3/shaders/fragdata.html', ['mac', 'intel'],
        bug=536887)
    self.Fail('deqp/data/gles3/shaders/invalid_texture_functions.html',
        ['mac', 'intel'], bug=536887)
    self.Fail('deqp/data/gles3/shaders/keywords.html', ['mac', 'intel'],
        bug=536887)
    self.Fail('deqp/data/gles3/shaders/negative.html', ['mac', 'intel'],
        bug=536887)
    self.Fail('deqp/data/gles3/shaders/switch.html', ['mac', 'intel'],
        bug=536887)
    self.Fail('deqp/data/gles3/shaders/swizzles.html', ['mac', 'intel'],
        bug=536887)
    self.Fail('deqp/functional/gles3/bufferobjectquery.html', ['mac', 'intel'],
        bug=536887)
    self.Fail('deqp/functional/gles3/fbostencilbuffer.html', ['mac', 'intel'],
        bug=536887)

    # Linux only.
    self.Fail('conformance2/glsl3/vector-dynamic-indexing.html',
        ['linux'], bug=483282)
    self.Fail('conformance2/rendering/draw-buffers.html',
        ['linux'], bug=483282)
    self.Fail('conformance2/samplers/samplers.html',
        ['linux'], bug=483282)
    # Linux NVIDIA only.
    self.Skip('deqp/functional/gles3/shaderswitch.html',
        ['linux', 'nvidia'], bug=483282)
    # Linux AMD only (but fails on all Linux, so not specified as AMD specific)
    # It looks like AMD shader compiler rejects many valid ES3 semantics.
    self.Skip('deqp/data/gles3/shaders/arrays.html', ['linux'], bug=483282)
    self.Skip('deqp/data/gles3/shaders/constant_expressions.html',
        ['linux'], bug=483282)
    self.Skip('deqp/data/gles3/shaders/qualification_order.html',
        ['linux'], bug=483282)
