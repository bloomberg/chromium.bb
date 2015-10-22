# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from gpu_tests.gpu_test_expectations import GpuTestExpectations

# See the GpuTestExpectations class for documentation.

class MemoryTestExpectations(GpuTestExpectations):
  def SetExpectations(self):
    # Sample Usage:
    # self.Fail('Memory.CSS3D',
    #     ['mac', 'amd', ('nvidia', 0x1234)], bug=123)

    self.Fail('Memory.CSS3D', bug=435899)
