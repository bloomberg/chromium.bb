# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from pylib import constants
from pylib.base import test_instance
from pylib.utils import apk_helper

class UirobotTestInstance(test_instance.TestInstance):

  def __init__(self, args, error_func):
    """Constructor.

    Args:
      args: Command line arguments.
    """
    super(UirobotTestInstance, self).__init__()
    if not args.app_under_test:
      error_func('Must set --app-under-test.')
    self._app_under_test = args.app_under_test

    if args.device_type == 'Android':
      self._suite = 'Android Uirobot'
      self._package_name = apk_helper.GetPackageName(self._app_under_test)

    elif args.device_type == 'iOS':
      self._suite = 'iOS Uirobot'
      self._package_name = self._app_under_test

    self._minutes = args.minutes

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
  def app_under_test(self):
    """Returns the app to run the test on."""
    return self._app_under_test

  @property
  def minutes(self):
    """Returns the number of minutes to run the uirobot for."""
    return self._minutes

  @property
  def package_name(self):
    """Returns the name of the package in the APK."""
    return self._package_name

  @property
  def suite(self):
    return self._suite
