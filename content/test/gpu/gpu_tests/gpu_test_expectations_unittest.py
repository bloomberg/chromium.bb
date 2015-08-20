# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import unittest

from telemetry.internal.platform import system_info
from telemetry.page import page as page_module
from telemetry.story import story_set

import gpu_test_expectations

class StubPlatform(object):
  def __init__(self, os_name, os_version_name=None):
    self.os_name = os_name
    self.os_version_name = os_version_name

  def GetOSName(self):
    return self.os_name

  def GetOSVersionName(self):
    return self.os_version_name

class StubBrowser(object):
  def __init__(self, platform, gpu, device, vendor_string, device_string,
               browser_type=None, gl_renderer=None):
    self.platform = platform
    self.browser_type = browser_type
    sys_info = {
      'model_name': '',
      'gpu': {
        'devices': [
          {'vendor_id': gpu, 'device_id': device,
           'vendor_string': vendor_string, 'device_string': device_string},
        ]
      }
    }
    if gl_renderer:
      sys_info['gpu']['aux_attributes'] = {
        'gl_renderer': gl_renderer
      }
    self.system_info = system_info.SystemInfo.FromDict(sys_info)

  @property
  def supports_system_info(self):
    return False if not self.system_info else True

  def GetSystemInfo(self):
    return self.system_info

class SampleTestExpectations(gpu_test_expectations.GpuTestExpectations):
  def SetExpectations(self):
    self.Flaky('test1.html', bug=123, max_num_retries=5)
    self.Flaky('test2.html', ['win'], bug=123, max_num_retries=6)
    self.Flaky('wildcardtest*.html', ['win'], bug=123, max_num_retries=7)
    # Test ANGLE conditions.
    self.Fail('test3.html', ['win', 'd3d9'], bug=345)
    # Test browser conditions.
    self.Fail('test4.html', ['android', 'android-webview-shell'], bug=456)

class GpuTestExpectationsTest(unittest.TestCase):
  def setUp(self):
    self.expectations = SampleTestExpectations()

  def assertExpectationEquals(self, expected, page, platform='', gpu=0,
                              device=0, vendor_string='', device_string='',
                              browser_type=None, gl_renderer=None):
    result = self.expectations.GetExpectationForPage(StubBrowser(
      platform, gpu, device, vendor_string, device_string,
      browser_type=browser_type, gl_renderer=gl_renderer), page)
    self.assertEquals(expected, result)

  def getRetriesForPage(self, page, platform='', gpu=0,
      device=0, vendor_string='', device_string=''):
    return self.expectations.GetFlakyRetriesForPage(page, StubBrowser(
      platform, gpu, device, vendor_string, device_string))

  # Ensure retry mechanism is working.
  def testFlakyExpectation(self):
    ps = story_set.StorySet()
    page = page_module.Page('http://test.com/test1.html', ps)
    self.assertExpectationEquals('pass', page)
    self.assertEquals(5, self.getRetriesForPage(page))

  # Ensure the filtering from the TestExpectations superclass still works.
  def testFlakyPerPlatformExpectation(self):
    ps = story_set.StorySet()
    page1 = page_module.Page('http://test.com/test2.html', ps)
    self.assertExpectationEquals('pass', page1, StubPlatform('win'))
    self.assertEquals(6, self.getRetriesForPage(page1, StubPlatform('win')))
    self.assertExpectationEquals('pass', page1, StubPlatform('mac'))
    self.assertEquals(0, self.getRetriesForPage(page1, StubPlatform('mac')))
    page2 = page_module.Page('http://test.com/wildcardtest1.html', ps)
    self.assertExpectationEquals('pass', page2, StubPlatform('win'))
    self.assertEquals(7, self.getRetriesForPage(page2, StubPlatform('win')))
    self.assertExpectationEquals('pass', page2, StubPlatform('mac'))
    self.assertEquals(0, self.getRetriesForPage(page2, StubPlatform('mac')))

  # Test ANGLE conditions.
  def testANGLEConditions(self):
    ps = story_set.StorySet()
    page = page_module.Page('http://test.com/test3.html', ps)
    self.assertExpectationEquals('pass', page, StubPlatform('win'),
                                 gl_renderer='Direct3D11')
    self.assertExpectationEquals('fail', page, StubPlatform('win'),
                                 gl_renderer='Direct3D9')

  # Test browser type conditions.
  def testBrowserTypeConditions(self):
    ps = story_set.StorySet()
    page = page_module.Page('http://test.com/test4.html', ps)
    self.assertExpectationEquals('pass', page, StubPlatform('android'),
                                 browser_type='android-content-shell')
    self.assertExpectationEquals('fail', page, StubPlatform('android'),
                                 browser_type='android-webview-shell')
