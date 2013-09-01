# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Host-driven java_tests which exercise dummy functionality.

This test class is only here to ensure that the test framework for host driven
tests work.
"""

from pylib.host_driven import test_case
from pylib.host_driven import tests_annotations


class DummyTest(test_case.HostDrivenTestCase):
  """Dummy host-driven test for testing the framework itself."""

  @tests_annotations.Smoke
  def testPass(self):
    return self._RunJavaTestFilters(['DummyTest.testPass'])
