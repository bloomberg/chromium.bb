# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import test_expectations

# Valid expectation conditions are:
#
# Operating systems:
#     win, xp, vista, win7, mac, leopard, snowleopard, lion, mountainlion,
#     linux, chromeos, android
#
# GPU vendors:
#     amd, arm, broadcom, hisilicon, intel, imagination, nvidia, qualcomm,
#     vivante
#
# Specific GPUs can be listed as a tuple with vendor name and device ID.
# Examples: ('nvidia', 0x1234), ('arm', 'Mali-T604')
# Device IDs must be paired with a GPU vendor.

class WebGLConformanceExpectations(test_expectations.TestExpectations):
  def SetExpectations(self):
    # Sample Usage:
    # self.Fail('gl-enable-vertex-attrib.html',
    #     ['mac', 'amd', ('nvidia', 0x1234)], bug=123)

    # Fails on all platforms
    self.Fail('conformance/glsl/misc/shaders-with-mis-matching-uniforms.html',
        bug=351396)

    # Flaky on Win
    self.Fail('conformance/extensions/webgl-draw-buffers.html',
        ['win'], bug=369349)

    # Win failures
    self.Fail('conformance/glsl/misc/struct-equals.html',
        ['win'], bug=391957)

    # Win7 / Intel failures
    self.Fail('conformance/rendering/gl-scissor-test.html',
        ['win7', 'intel'], bug=314997)
    self.Fail('conformance/context/premultiplyalpha-test.html',
        ['win7', 'intel'])
    self.Fail('conformance/textures/copy-tex-image-and-sub-image-2d.html',
        ['win7', 'intel'])
    self.Fail('conformance/rendering/gl-viewport-test.html',
        ['win7', 'intel'], bug=372511)
    self.Fail('conformance/glsl/misc/shader-with-array-of-structs-uniform.html',
        ['win7', 'intel', 'nvidia'], bug=373972)

    # Mac failures
    self.Fail('conformance/glsl/misc/shader-struct-scope.html',
        ['mac'], bug=368910)

    # Mac / Intel failures
    # Radar 13499466
    self.Fail('conformance/limits/gl-max-texture-dimensions.html',
        ['mac', 'intel'], bug=225642)
    # Radar 13499623
    self.Fail('conformance/textures/texture-size.html',
        ['mac', 'intel'], bug=225642)

    # Mac / Intel HD 3000 failures
    self.Skip('conformance/ogles/GL/control_flow/control_flow_009_to_010.html',
        ['mac', ('intel', 0x116)], bug=322795)
    # Radar 13499677
    self.Fail('conformance/glsl/functions/glsl-function-smoothstep-gentype.html',
        ['mac', ('intel', 0x116)], bug=225642)
    self.Fail('conformance/extensions/webgl-draw-buffers.html',
        ['mac', ('intel', 0x116)], bug=369349)

    # Mac 10.8 / Intel HD 3000 failures
    self.Fail('conformance/rendering/gl-scissor-test.html',
        ['mountainlion', ('intel', 0x116)], bug=314997)
    self.Fail('conformance/ogles/GL/operators/operators_009_to_016.html',
        ['mountainlion', ('intel', 0x116)], bug=322795)

    # Mac Retina failures
    self.Fail(
        'conformance/glsl/bugs/array-of-struct-with-int-first-position.html',
        ['mac', ('nvidia', 0xfd5), ('nvidia', 0xfe9)], bug=368912)

    # Mac 10.8 / ATI failures
    self.Fail(
        'conformance/rendering/point-with-gl-pointcoord-in-fragment-shader.html',
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

    # Linux NVIDIA failures
    self.Fail('conformance/glsl/constructors/glsl-construct-bvec2.html',
        ['linux', 'nvidia'], bug=391960)
    self.Fail('conformance/glsl/constructors/glsl-construct-bvec3.html',
        ['linux', 'nvidia'], bug=391960)
    self.Fail('conformance/glsl/constructors/glsl-construct-bvec4.html',
        ['linux', 'nvidia'], bug=391960)
    self.Fail('conformance/glsl/constructors/glsl-construct-ivec2.html',
        ['linux', 'nvidia'], bug=391960)
    self.Fail('conformance/glsl/constructors/glsl-construct-ivec3.html',
        ['linux', 'nvidia'], bug=391960)
    self.Fail('conformance/glsl/constructors/glsl-construct-ivec4.html',
        ['linux', 'nvidia'], bug=391960)
    self.Fail('conformance/glsl/constructors/glsl-construct-vec2.html',
        ['linux', 'nvidia'], bug=391960)
    self.Fail('conformance/glsl/constructors/glsl-construct-vec3.html',
        ['linux', 'nvidia'], bug=391960)
    self.Fail('conformance/glsl/constructors/glsl-construct-vec4.html',
        ['linux', 'nvidia'], bug=391960)

    # Android failures
    # The following test is very slow and therefore times out on Android bot.
    self.Skip('conformance/rendering/multisample-corruption.html',
        ['android'])
    # The following test times out on Android bot.
    self.Fail('conformance/uniforms/gl-uniform-arrays.html',
        ['android'], bug=369300)
    self.Fail('conformance/glsl/misc/empty_main.vert.html',
        ['android'], bug=315976)
    self.Fail('conformance/glsl/misc/gl_position_unset.vert.html',
        ['android'], bug=315976)
    # The following tests are disabled due to security issues.
    self.Fail('conformance/textures/tex-image-and-sub-image-2d-with-video.html',
        ['android'], bug=334204)
    self.Fail('conformance/textures/tex-image-and-sub-image-2d-with-video-rgb565.html',
        ['android'], bug=334204)
    self.Fail('conformance/textures/tex-image-and-sub-image-2d-with-video-rgba4444.html',
        ['android'], bug=334204)
    self.Fail('conformance/textures/tex-image-and-sub-image-2d-with-video-rgba5551.html',
        ['android'], bug=334204)
    self.Fail('conformance/textures/texture-npot-video.html',
        ['android'], bug=334204)

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
    self.Fail('conformance/textures/texture-size-limit.html',
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
    self.Fail('conformance/glsl/misc/empty_main.vert.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/glsl/misc/gl_position_unset.vert.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/glsl/misc/shaders-with-varyings.html',
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
    self.Fail('conformance/textures/texture-mips.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/textures/texture-npot.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/textures/texture-npot-video.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/textures/texture-size.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/textures/texture-size-limit.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/uniforms/gl-uniform-arrays.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Skip('conformance/uniforms/uniform-default-values.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
