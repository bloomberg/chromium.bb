#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import commands
import glob
import logging
import os
import sys
import unittest

import pyauto_functional # Must import before pyauto
import pyauto

class CodesignTest(pyauto.PyUITest):
  """Test if the build is code signed"""

  def testCodeSign(self):
    """Check the app for codesign and bail out if it's non-branded."""
    browser_info = self.GetBrowserInfo()

    # bail out if not a branded build
    if browser_info['properties']['branding'] != 'Google Chrome':
      return

    # TODO: Add functionality for other operating systems (see crbug.com/47902)
    if self.IsMac():
      self._MacCodeSign(browser_info)

  def _MacCodeSign(self, browser_info):
    valid_text = 'valid on disk'
    app_name = 'Google Chrome.app'

    # Codesign of the app @ xcodebuild/Release/Google Chrome.app/
    app_path = browser_info['child_process_path']
    app_path = app_path[:app_path.find(app_name)]
    app_path = app_path + app_name
    self.assertTrue(valid_text in self._checkCodeSign(app_path))

    # Codesign of the frameWork
    framework_path = glob.glob(os.path.join(app_path, 'Contents', 'Versions',
                                            '*.*.*.*'))[0]
    framework_path = os.path.join(framework_path,
                                  'Google Chrome Framework.framework')
    self.assertTrue(valid_text in self._checkCodeSign(framework_path))

  def _checkCodeSign(self, file_path):
    """Return the output of the codesign"""
    return commands.getoutput('codesign -vvv "%s"' % file_path)


if __name__ == '__main__':
  pyauto_functional.Main()
