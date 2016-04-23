# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from gpu_tests.gpu_test_expectations import GpuTestExpectations

# See the GpuTestExpectations class for documentation.

class ScreenshotSyncExpectations(GpuTestExpectations):
  def __init__(self, *args, **kwargs):
    super(ScreenshotSyncExpectations, self).__init__(*args, **kwargs)

  def SetExpectations(self):
    self.Flaky('ScreenshotSync.WithCanvas', ['win', 'amd'], bug=599776)
    self.Flaky('ScreenshotSync.WithCanvas', ['mac', 'intel'], bug=599776)
    self.Flaky('ScreenshotSync.WithDivs', ['mac', 'intel'], bug=599776)
