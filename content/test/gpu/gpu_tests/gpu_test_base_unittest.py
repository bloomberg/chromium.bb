# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import unittest

from telemetry.core import discover

import gpu_test_base

def _GetGpuDir(*subdirs):
  gpu_dir = os.path.dirname(os.path.dirname(__file__))
  return os.path.join(gpu_dir, *subdirs)

#
# Unit tests verifying invariants of classes in GpuTestBase.
#

class NoOverridesTest(unittest.TestCase):
  def testValidatorBase(self):
    all_validators = discover.DiscoverClasses(
      _GetGpuDir('gpu_tests'), _GetGpuDir(),
      gpu_test_base.ValidatorBase,
      index_by_class_name=True).values()
    self.assertGreater(len(all_validators), 0)
    for validator in all_validators:
      self.assertEquals(gpu_test_base.ValidatorBase.ValidateAndMeasurePage,
                        validator.ValidateAndMeasurePage,
                        'Class %s should not override ValidateAndMeasurePage'
                        % validator.__name__)

  def testPageBase(self):
    all_pages = discover.DiscoverClasses(
      _GetGpuDir(), _GetGpuDir(),
      gpu_test_base.PageBase,
      index_by_class_name=True).values()
    self.assertGreater(len(all_pages), 0)
    for page in all_pages:
      self.assertEquals(gpu_test_base.PageBase.RunNavigateSteps,
                        page.RunNavigateSteps,
                        'Class %s should not override ValidateAndMeasurePage'
                        % page.__name__)
      self.assertEquals(gpu_test_base.PageBase.RunPageInteractions,
                        page.RunPageInteractions,
                        'Class %s should not override RunPageInteractions'
                        % page.__name__)
