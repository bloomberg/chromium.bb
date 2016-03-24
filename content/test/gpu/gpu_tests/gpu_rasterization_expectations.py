# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from gpu_tests.gpu_test_expectations import GpuTestExpectations

# See the GpuTestExpectations class for documentation.

class GpuRasterizationExpectations(GpuTestExpectations):
  def SetExpectations(self):
    # Sample Usage:
    # self.Fail('GpuRasterization.BlueBox',
    #     ['mac', 'amd', ('nvidia', 0x1234)], bug=123)

    # Failing on Nexus 5 and 6
    self.Fail('GpuRasterization.ConcavePaths',
              ['android', 'qualcomm'], bug=499555)
