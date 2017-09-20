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


if __name__ == '__main__':
  unittest.main()
