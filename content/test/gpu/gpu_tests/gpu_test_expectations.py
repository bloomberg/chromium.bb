# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import test_expectations

# Valid expectation conditions are:
#
# Operating systems:
#     win, xp, vista, win7, mac, leopard, snowleopard, lion, mountainlion,
#     mavericks, yosemite, linux, chromeos, android
#
# GPU vendors:
#     amd, arm, broadcom, hisilicon, intel, imagination, nvidia, qualcomm,
#     vivante
#
# Direct3D status:
#     d3d9, d3d11
#
# Specific GPUs can be listed as a tuple with vendor name and device ID.
# Examples: ('nvidia', 0x1234), ('arm', 'Mali-T604')
# Device IDs must be paired with a GPU vendor.
#
# Sample usage in SetExpectations in subclasses:
#   self.Fail('gl-enable-vertex-attrib.html',
#       ['mac', 'amd', ('nvidia', 0x1234)], bug=123)

class GpuTestExpectations(test_expectations.TestExpectations):
  def IsValidUserDefinedCondition(self, condition):
    # Add support for d3d9 and d3d11-specific expectations.
    if condition in ('d3d9', 'd3d11'):
      return True
    return super(GpuTestExpectations,
        self).IsValidUserDefinedCondition(condition)

  def ModifiersApply(self, platform, gpu_info, expectation):
    if not super(GpuTestExpectations, self).ModifiersApply(
        platform, gpu_info, expectation):
      return False
    # We'll only get here if the OS and GPU matched the expectation.
    d3d_version = ''
    if gpu_info.aux_attributes:
      gl_renderer = gpu_info.aux_attributes.get('gl_renderer')
      if gl_renderer:
        if 'Direct3D11' in gl_renderer:
          d3d_version = 'd3d11'
        elif 'Direct3D9' in gl_renderer:
          d3d_version = 'd3d9'
    d3d_matches = ((not expectation.user_defined_conditions) or
        d3d_version in expectation.user_defined_conditions)
    return d3d_matches
