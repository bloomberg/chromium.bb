# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from gpu_tests.gpu_test_expectations import GpuTestExpectations

# See the GpuTestExpectations class for documentation.

class GpuProcessExpectations(GpuTestExpectations):
  def SetExpectations(self):
    # Accelerated 2D canvas is not available on Linux due to driver instability
    self.Fail('GpuProcess_canvas2d', ['linux'], bug=254724)

    self.Fail('GpuProcess_video', ['linux'], bug=257109)

    # Chrome on Android doesn't support software fallback.
    self.Skip('GpuProcess_no_gpu_process', ['android'], bug=643282)
    self.Skip('GpuProcess_skip_gpu_process', ['android'], bug=(610951, 610023))

    # There is no Android multi-gpu configuration and the helper
    # gpu_info_collector.cc::IdentifyActiveGPU is not even called.
    self.Skip('GpuProcess_identify_active_gpu1', ['android'])
    self.Skip('GpuProcess_identify_active_gpu2', ['android'])
    self.Skip('GpuProcess_identify_active_gpu3', ['android'])
    self.Skip('GpuProcess_identify_active_gpu4', ['android'])

    # There is currently no entry in kSoftwareRenderingListJson that enables
    # a software GL driver on Android.
    self.Skip('GpuProcess_software_gpu_process', ['android'])
