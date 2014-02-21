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

    # Win7/Intel failures
    self.Fail('conformance/rendering/gl-scissor-test.html',
        ['win7', 'intel'], bug=314997)
    self.Fail('conformance/context/premultiplyalpha-test.html',
        ['win7', 'intel'])
    self.Fail('conformance/textures/copy-tex-image-and-sub-image-2d.html',
        ['win7', 'intel'])

    # Mac/Intel failures
    self.Fail('conformance/rendering/gl-scissor-test.html',
        ['mac', 'intel'], bug=314997)
    # The following three tests only fail.
    # Radar 13499677
    self.Fail(
        'conformance/glsl/functions/glsl-function-smoothstep-gentype.html',
        ['mac', 'intel'], bug=225642)
    # Radar 13499466
    self.Fail('conformance/limits/gl-max-texture-dimensions.html',
        ['mac', 'intel'], bug=225642)
    # Radar 13499623
    self.Fail('conformance/textures/texture-size.html',
        ['mac', 'intel'], bug=225642)
    self.Skip('conformance/ogles/GL/control_flow/control_flow_009_to_010.html',
        ['mac', 'intel'], bug=322795)
    self.Fail('conformance/ogles/GL/operators/operators_009_to_016.html',
        ['mac', 'intel'], bug=322795)

    # Mac/Intel failures on 10.7
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
    self.Skip(
        'conformance/glsl/functions/glsl-function-smoothstep-gentype.html',
        ['lion', 'intel'])
    self.Skip('conformance/limits/gl-max-texture-dimensions.html',
        ['lion', 'intel'])
    self.Skip('conformance/rendering/line-loop-tri-fan.html',
        ['lion', 'intel'])
    self.Skip('conformance/ogles/GL/control_flow/control_flow_001_to_008.html',
        ['lion', 'intel'], bug=345575)
    self.Skip('conformance/ogles/GL/control_flow/control_flow_009_to_010.html',
        ['lion'], bug=322795)
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

    # Mac/ATI failures
    self.Skip('conformance/extensions/oes-texture-float-with-image-data.html',
        ['mac', 'amd'], bug=308328)
    self.Skip('conformance/rendering/gl-clear.html',
        ['mac', 'amd'], bug=308328)
    self.Skip('conformance/textures/tex-image-and-sub-image-2d-with-array-buffer-view.html',
        ['mac', 'amd'], bug=308328)
    self.Skip('conformance/textures/tex-image-and-sub-image-2d-with-image-data.html',
        ['mac', 'amd'], bug=308328)
    self.Skip('conformance/textures/tex-image-and-sub-image-2d-with-image-data-rgb565.html',
        ['mac', 'amd'], bug=308328)
    self.Skip('conformance/textures/tex-image-and-sub-image-2d-with-image-data-rgba4444.html',
        ['mac', 'amd'], bug=308328)
    self.Skip('conformance/textures/tex-image-and-sub-image-2d-with-image-data-rgba5551.html',
        ['mac', 'amd'], bug=308328)

    # Android failures
    # The following test is very slow and therefore times out on Android bot.
    self.Skip('conformance/rendering/multisample-corruption.html',
        ['android'])
    self.Fail('conformance/glsl/misc/empty_main.vert.html',
        ['android'], bug=315976)
    self.Fail('conformance/glsl/misc/gl_position_unset.vert.html',
        ['android'], bug=315976)
    self.Skip('conformance/textures/tex-image-and-sub-image-2d-with-video.html',
        ['android'], bug=341698)
    self.Skip('conformance/textures/tex-image-and-sub-image-2d-with-video-rgb565.html',
        ['android'], bug=341698)
    self.Skip('conformance/textures/tex-image-and-sub-image-2d-with-video-rgba4444.html',
        ['android'], bug=341698)
    self.Skip('conformance/textures/tex-image-and-sub-image-2d-with-video-rgba5551.html',
        ['android'], bug=341698)
