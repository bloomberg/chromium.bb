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
# Specific GPUs can be listed as a tuple with vendor name and device ID.
# Examples: ('nvidia', 0x1234), ('arm', 'Mali-T604')
# Device IDs must be paired with a GPU vendor.

class ContextLostExpectations(test_expectations.TestExpectations):
  def SetExpectations(self):
    # Sample Usage:
    # self.Fail('ContextLost.WebGLContextLostFromGPUProcessExit',
    #     ['mac', 'amd', ('nvidia', 0x1234)], bug=123)

    self.Fail('GpuCrash.GPUProcessCrashesExactlyOnce',
              ['win'], bug=392891)
