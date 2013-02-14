# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from pylib.host_driven import python_test_base
from pylib.host_driven import tests_annotations


DUMMY_JAVA_TEST_PATH = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, 'javatests', 'src',
                 'org', 'chromium', 'chrome', 'browser', 'test',
                 'DummyTest.java'))


class DummyTest(python_test_base.PythonTestBase):
  """Dummy host-driven test for testing the framework itself."""

  @tests_annotations.Smoke
  def testPass(self):
    return self._RunJavaTests(
        DUMMY_JAVA_TEST_PATH, ['DummyTest.testPass'])

