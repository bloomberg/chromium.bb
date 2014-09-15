# Copyright 2014 The Chromium Authors. All rights reserved.
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

class MemoryExpectations(test_expectations.TestExpectations):
  def SetExpectations(self):
    # Sample Usage:
    # self.Fail('Memory.CSS3D',
    #     ['mac', 'amd', ('nvidia', 0x1234)], bug=123)

    self.Fail('Memory.CSS3D', ['mac', ('nvidia', 0x0fd5)], bug=368037)

    # TODO(vmpstr): Memory drops and increases again, and this
    # particular bot happens to catch it when its low. Remove
    # once the bug is fixed.
    self.Fail('Memory.CSS3D', ['win'], bug=373098)

    # Test has turned flaky on Linux also.  Remove once the bug is fixed.
    self.Fail('Memory.CSS3D', ['linux'], bug=373098)
