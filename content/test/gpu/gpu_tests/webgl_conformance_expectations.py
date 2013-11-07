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

    # Windows/Intel failures
    self.Fail('conformance/textures/texture-size.html',
        ['win', 'intel'], bug=121139)
    self.Fail('conformance/rendering/gl-scissor-test.html',
        ['win', 'intel'], bug=314997)

    # Windows/AMD failures
    self.Fail('conformance/rendering/more-than-65536-indices.html',
        ['win', 'amd'], bug=314997)

    # Windows 7/Intel failures
    self.Fail('conformance/context/context-lost-restored.html',
        ['win7', 'intel'])
    self.Fail('conformance/context/premultiplyalpha-test.html',
        ['win7', 'intel'])
    self.Fail('conformance/extensions/oes-texture-float-with-image-data.html',
        ['win7', 'intel'])
    self.Fail('conformance/extensions/oes-texture-float.html',
        ['win7', 'intel'])
    self.Fail('conformance/limits/gl-min-attribs.html',
        ['win7', 'intel'])
    self.Fail('conformance/limits/gl-max-texture-dimensions.html',
        ['win7', 'intel'])
    self.Fail('conformance/limits/gl-min-textures.html',
        ['win7', 'intel'])
    self.Fail('conformance/limits/gl-min-uniforms.html',
        ['win7', 'intel'])
    self.Fail('conformance/rendering/gl-clear.html',
        ['win7', 'intel'])
    self.Fail('conformance/textures/copy-tex-image-and-sub-image-2d.html',
        ['win7', 'intel'])
    self.Fail('conformance/textures/gl-teximage.html',
        ['win7', 'intel'])
    self.Fail('conformance/textures/tex-image-and-sub-image-2d-with-array-buffer-view.html',
        ['win7', 'intel'])
    self.Fail('conformance/textures/tex-image-and-sub-image-2d-with-image-data.html',
        ['win7', 'intel'])
    self.Fail('conformance/textures/tex-image-and-sub-image-2d-with-image-data-rgb565.html',
        ['win7', 'intel'])
    self.Fail('conformance/textures/tex-image-and-sub-image-2d-with-image-data-rgba4444.html',
        ['win7', 'intel'])
    self.Fail('conformance/textures/tex-image-and-sub-image-2d-with-image-data-rgba5551.html',
        ['win7', 'intel'])
    self.Fail('conformance/textures/tex-image-with-format-and-type.html',
        ['win7', 'intel'])
    self.Fail('conformance/textures/tex-sub-image-2d.html',
        ['win7', 'intel'])
    self.Fail('conformance/textures/texparameter-test.html',
        ['win7', 'intel'])
    self.Fail('conformance/textures/texture-active-bind-2.html',
        ['win7', 'intel'])
    self.Fail('conformance/textures/texture-active-bind.html',
        ['win7', 'intel'])
    self.Fail('conformance/textures/texture-complete.html',
        ['win7', 'intel'])
    self.Fail('conformance/textures/texture-formats-test.html',
        ['win7', 'intel'])
    self.Fail('conformance/textures/texture-mips.html',
        ['win7', 'intel'])
    self.Fail('conformance/textures/texture-npot.html',
        ['win7', 'intel'])
    self.Fail('conformance/textures/texture-size-cube-maps.html',
        ['win7', 'intel'])

    # Mac/Intel failures
    self.Fail('conformance/rendering/gl-scissor-test.html',
        ['mac', 'intel'], bug=314997)
    # The following two tests hang the WindowServer.
    self.Skip('conformance/canvas/drawingbuffer-static-canvas-test.html',
        ['mac', 'intel'], bug=303915)
    self.Skip('conformance/canvas/drawingbuffer-test.html',
        ['mac', 'intel'], bug=303915)
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
    self.Fail('conformance/canvas/drawingbuffer-test.html',
        ['mac', 'amd'], bug=314997)

    # Android failures
    self.Fail('conformance/textures/texture-npot-video.html',
        ['android'], bug=306485)
    # The following test is very slow and therefore times out on Android bot.
    self.Skip('conformance/rendering/multisample-corruption.html',
        ['android'])
    self.Fail('conformance/canvas/drawingbuffer-test.html',
        ['android'], bug=314997)
    self.Fail('conformance/glsl/misc/empty_main.vert.html',
        ['android'], bug=315976)
    self.Fail('conformance/glsl/misc/gl_position_unset.vert.html',
        ['android'], bug=315976)
