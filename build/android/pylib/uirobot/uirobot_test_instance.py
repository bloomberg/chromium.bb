# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from pylib import constants
from pylib.base import test_instance
from pylib.utils import apk_helper

class UirobotTestInstance(test_instance.TestInstance):

  def __init__(self, args):
    """Constructor.

    Args:
      args: Command line arguments.
    """
    super(UirobotTestInstance, self).__init__()
    self._apk_under_test = os.path.join(
        constants.GetOutDirectory(), args.apk_under_test)
    self._minutes = args.minutes
    self._package_name = apk_helper.GetPackageName(self._apk_under_test)

  #override
  def TestType(self):
    """Returns type of test."""
    return 'uirobot'

  #override
  def SetUp(self):
    """Setup for test."""
    pass

  #override
  def TearDown(self):
    """Teardown for test."""
    pass

  @property
  def apk_under_test(self):
    """Returns the app to run the test on."""
    return self._apk_under_test

  @property
  def minutes(self):
    """Returns the number of minutes to run the uirobot for."""
    return self._minutes

  @property
  def package_name(self):
    """Returns the name of the package in the APK."""
    return self._package_name
