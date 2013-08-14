# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import test_expectations

# Valid expectation conditions are:
# win xp vista win7
# mac leopard snowleopard lion mountainlion
# linux chromeos android
# nvidia amd intel

class WebGLConformanceExpectations(test_expectations.TestExpectations):
  def SetExpectations(self):
    # Sample Usage:
    # self.Fail("gl-enable-vertex-attrib.html", ["mac", "win"], bug=1234)
    self.Fail('uniform-samplers-test.html', ['android'], bug=272080)
