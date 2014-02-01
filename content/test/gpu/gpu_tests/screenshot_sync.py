# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os

import screenshot_sync_expectations as expectations

from telemetry import test
from telemetry.core import util
from telemetry.page import page_set
from telemetry.page import page_test

data_path = os.path.join(
    util.GetChromiumSrcDir(), 'content', 'test', 'data', 'gpu')

class ScreenshotSyncValidator(page_test.PageTest):
  def __init__(self):
    super(ScreenshotSyncValidator, self).__init__('ValidatePage')

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--enable-gpu-benchmarking')

  def ValidatePage(self, page, tab, results):
    test_success = tab.EvaluateJavaScript('window.__testSuccess')
    if not test_success:
      message = tab.EvaluateJavaScript('window.__testMessage')
      raise page_test.Failure(message)

class ScreenshotSyncProcess(test.Test):
  """Tests that screenhots are properly synchronized with the frame one which
  they were requested"""
  test = ScreenshotSyncValidator

  def CreateExpectations(self, page_set):
    return expectations.ScreenshotSyncExpectations()

  def CreatePageSet(self, options):
    page_set_dict = {
      'description': 'Test cases for screenshot synchronization',
      'user_agent_type': 'desktop',
      'serving_dirs': [''],
      'pages': [
        {
          'name': 'ScreenshotSync',
          'url': 'file://screenshot_sync.html',
          'navigate_steps': [
            { 'action': 'navigate' },
            { 'action': 'wait',
              'javascript': 'window.__testComplete',
              'timeout': 120 }
          ]
        }
      ]
    }
    return page_set.PageSet.FromDict(page_set_dict, data_path)