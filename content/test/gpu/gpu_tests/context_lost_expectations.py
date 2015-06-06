# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from gpu_test_expectations import GpuTestExpectations

# See the GpuTestExpectations class for documentation.

class ContextLostExpectations(GpuTestExpectations):
  def SetExpectations(self):
    # Sample Usage:
    # self.Fail('ContextLost.WebGLContextLostFromGPUProcessExit',
    #     ['mac', 'amd', ('nvidia', 0x1234)], bug=123)

    # AMD Radeon 6450
    self.Fail('ContextLost.WebGLContextLostFromGPUProcessExit',
        ['linux', ('amd', 0x6779)], bug=479975)

    # Mac 10.8 (ideally should restrict this to Debug, too)
    self.Fail('ContextLost.WebGLContextLostFromSelectElement',
              ['mountainlion'], bug=497411)
