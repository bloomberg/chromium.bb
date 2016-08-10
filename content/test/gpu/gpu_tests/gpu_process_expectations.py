# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from gpu_tests.gpu_test_expectations import GpuTestExpectations

# See the GpuTestExpectations class for documentation.

class GpuProcessExpectations(GpuTestExpectations):
  def SetExpectations(self):
    # Accelerated 2D canvas is not available on Linux due to driver instability
    self.Fail('GpuProcess.canvas2d', ['linux'], bug=254724)

    self.Fail('GpuProcess.video', ['linux'], bug=257109)

    # Android
    self.Fail('GpuProcess.no_gpu_process',
              ['android'], bug=611930)
    self.Fail('GpuProcess.identify_active_gpu1',
              ['android'], bug=611930)
    self.Fail('GpuProcess.identify_active_gpu2',
              ['android'], bug=611930)
    self.Fail('GpuProcess.identify_active_gpu3',
              ['android'], bug=611930)
    self.Fail('GpuProcess.identify_active_gpu4',
              ['android'], bug=611930)
    self.Fail('GpuProcess.readback_webgl_gpu_process',
              ['android'], bug=611930)
    self.Fail('GpuProcess.driver_bug_workarounds_upon_gl_renderer',
              ['android'], bug=611930)

    # Nexus 5X
    # Skip this test because expecting it to fail will still run it.
    self.Skip('GpuProcess.skip_gpu_process',
              ['android', ('qualcomm', 'Adreno (TM) 418')], bug=610951)

    # Nexus 9
    # Skip this test because expecting it to fail will still run it.
    self.Skip('GpuProcess.skip_gpu_process',
              ['android', 'nvidia'], bug=610023)

    # There are currently no blacklist entries disabling all GPU
    # functionality on Mac OS.
    self.Fail('GpuProcess.no_gpu_process', ['mac'], bug=579255)
