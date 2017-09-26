#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""Unittests for writers.chromeos_adml_writer."""


import os
import sys
import unittest
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '../../../..'))


from writers import chromeos_adml_writer
from writers import adml_writer_unittest


class ChromeOsAdmlWriterUnittest(
    adml_writer_unittest.AdmlWriterUnittest):

  # Overridden.
  def _GetWriter(self, config):
    return chromeos_adml_writer.GetWriter(config)

  # Overridden
  def GetCategory(self):
    return "cros_test_category";

  # Overridden
  def GetCategoryString(self):
    return "CrOSTestCategory";

  # Overridden.
  def testPlatform(self):
    # Test that the writer correctly chooses policies of platform Chrome OS.
    self.assertTrue(self.writer.IsPolicySupported({
      'supported_on': [
        {'platforms': ['chrome_os', 'zzz']}, {'platforms': ['aaa']}
      ]
    }))
    self.assertFalse(self.writer.IsPolicySupported({
      'supported_on': [
        {'platforms': ['win', 'mac', 'linux']}, {'platforms': ['aaa']}
      ]
    }))

  def testOnlySupportsAdPolicies(self):
    # Tests whether only Active Directory managed policies are supported (Google
    # cloud only managed polices are not put in the ADMX file).
    policy = {
      'name': 'PolicyName',
      'supported_on': [{
        'product': 'chrome_os',
        'platforms': ['chrome_os'],
        'since_version': '8',
        'until_version': '',
      }],
    }
    self.assertTrue(self.writer.IsPolicySupported(policy))

    policy['supported_chrome_os_management'] = ['google_cloud']
    self.assertFalse(self.writer.IsPolicySupported(policy))

    policy['supported_chrome_os_management'] = ['active_directory']
    self.assertTrue(self.writer.IsPolicySupported(policy))

if __name__ == '__main__':
  unittest.main()
