# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import test_expectations

# Valid expectation conditions are:
# win xp vista win7
# mac leopard snowleopard lion mountainlion
# linux chromeos android
# nvidia amd intel
# Specific gpu's can be listed as a tuple with vendor name and device ID.
# Example: ('nvidia', 0x1234)
# Device ID's must be paired with a gpu vendor.

class WebGLConformanceExpectations(test_expectations.TestExpectations):
  def SetExpectations(self):
    # Sample Usage:
    # self.Fail("gl-enable-vertex-attrib.html",
    #     ['mac', 'amd', ('nvidia', 0x1234)], bug=123)
    self.Fail('uniform-samplers-test.html', ['android'], bug=272080)
