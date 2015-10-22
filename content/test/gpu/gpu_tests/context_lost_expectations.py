# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from gpu_tests.gpu_test_expectations import GpuTestExpectations

# See the GpuTestExpectations class for documentation.

class ContextLostExpectations(GpuTestExpectations):
  def SetExpectations(self):
    # Sample Usage:
    # self.Fail('ContextLost.WebGLContextLostFromGPUProcessExit',
    #     ['mac', 'amd', ('nvidia', 0x1234)], bug=123)

    # AMD Radeon 6450
    self.Fail('ContextLost.WebGLContextLostFromGPUProcessExit',
        ['linux', ('amd', 0x6779)], bug=479975)

    # Win8 Release and Debug NVIDIA bots.
    self.Skip('ContextLost.WebGLContextLostFromSelectElement',
              ['win8', 'nvidia'], bug=524808)

    # Flaky on Mac 10.7 and 10.8 resulting in crashes during browser
    # startup, so skip this test in those configurations.
    self.Skip('ContextLost.WebGLContextLostFromSelectElement',
              ['mountainlion', 'debug'], bug=497411)
    self.Skip('ContextLost.WebGLContextLostFromSelectElement',
              ['lion', 'debug'], bug=498149)
