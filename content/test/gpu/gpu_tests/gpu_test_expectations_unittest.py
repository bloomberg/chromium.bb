# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import unittest

from telemetry.internal.platform import system_info
from telemetry.page import page as page_module
from telemetry.story import story_set

from gpu_tests import gpu_test_expectations

VENDOR_NVIDIA = 0x10DE
VENDOR_AMD = 0x1002
VENDOR_INTEL = 0x8086

VENDOR_STRING_IMAGINATION = 'Imagination Technologies'
DEVICE_STRING_SGX = 'PowerVR SGX 554'

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
    # Test GPU conditions.
    self.Fail('test1.html', ['nvidia', 'intel'], bug=123)
    self.Fail('test2.html', [('nvidia', 0x1001), ('nvidia', 0x1002)], bug=123)
    self.Fail('test3.html', ['win', 'intel', ('amd', 0x1001)], bug=123)
    self.Fail('test4.html', ['imagination'])
    self.Fail('test5.html', [('imagination', 'PowerVR SGX 554')])
    # Test ANGLE conditions.
    self.Fail('test6.html', ['win', 'd3d9'], bug=345)
    # Test flaky expectations.
    self.Flaky('test7.html', bug=123, max_num_retries=5)
    self.Flaky('test8.html', ['win'], bug=123, max_num_retries=6)
    self.Flaky('wildcardtest*.html', ['win'], bug=123, max_num_retries=7)

class GpuTestExpectationsTest(unittest.TestCase):
  def setUp(self):
    self.expectations = SampleTestExpectations()

  def assertExpectationEquals(self, expected, page, platform=StubPlatform(''),
                              gpu=0, device=0, vendor_string='',
                              device_string='', browser_type=None,
                              gl_renderer=None):
    self.expectations.ClearExpectationsCacheForTesting()
    result = self.expectations.GetExpectationForPage(StubBrowser(
      platform, gpu, device, vendor_string, device_string,
      browser_type=browser_type, gl_renderer=gl_renderer), page)
    self.assertEquals(expected, result)

  def getRetriesForPage(self, page, platform=StubPlatform(''), gpu=0,
      device=0, vendor_string='', device_string=''):
    self.expectations.ClearExpectationsCacheForTesting()
    return self.expectations.GetFlakyRetriesForPage(StubBrowser(
      platform, gpu, device, vendor_string, device_string), page)

  # Pages with expectations for a GPU should only return them when running with
  # that GPU
  def testGpuExpectations(self):
    ps = story_set.StorySet()
    page = page_module.Page('http://test.com/test1.html', ps)
    self.assertExpectationEquals('fail', page, gpu=VENDOR_NVIDIA)
    self.assertExpectationEquals('fail', page, gpu=VENDOR_INTEL)
    self.assertExpectationEquals('pass', page, gpu=VENDOR_AMD)

  # Pages with expectations for a GPU should only return them when running with
  # that GPU
  def testGpuDeviceIdExpectations(self):
    ps = story_set.StorySet()
    page = page_module.Page('http://test.com/test2.html', ps)
    self.assertExpectationEquals('fail', page, gpu=VENDOR_NVIDIA, device=0x1001)
    self.assertExpectationEquals('fail', page, gpu=VENDOR_NVIDIA, device=0x1002)
    self.assertExpectationEquals('pass', page, gpu=VENDOR_NVIDIA, device=0x1003)
    self.assertExpectationEquals('pass', page, gpu=VENDOR_AMD, device=0x1001)

  # Pages with multiple expectations should only return them when all criteria
  # are met
  def testMultipleExpectations(self):
    ps = story_set.StorySet()
    page = page_module.Page('http://test.com/test3.html', ps)
    self.assertExpectationEquals('fail', page,
        StubPlatform('win'), VENDOR_AMD, 0x1001)
    self.assertExpectationEquals('fail', page,
        StubPlatform('win'), VENDOR_INTEL, 0x1002)
    self.assertExpectationEquals('pass', page,
        StubPlatform('win'), VENDOR_NVIDIA, 0x1001)
    self.assertExpectationEquals('pass', page,
        StubPlatform('mac'), VENDOR_AMD, 0x1001)
    self.assertExpectationEquals('pass', page,
        StubPlatform('win'), VENDOR_AMD, 0x1002)

  # Pages with expectations based on GPU vendor string.
  def testGpuVendorStringExpectations(self):
    ps = story_set.StorySet()
    page = page_module.Page('http://test.com/test4.html', ps)
    self.assertExpectationEquals('fail', page,
                                 vendor_string=VENDOR_STRING_IMAGINATION,
                                 device_string=DEVICE_STRING_SGX)
    self.assertExpectationEquals('fail', page,
                                 vendor_string=VENDOR_STRING_IMAGINATION,
                                 device_string='Triangle Monster 3000')
    self.assertExpectationEquals('pass', page,
                                 vendor_string='Acme',
                                 device_string=DEVICE_STRING_SGX)

  # Pages with expectations based on GPU vendor and renderer string pairs.
  def testGpuDeviceStringExpectations(self):
    ps = story_set.StorySet()
    page = page_module.Page('http://test.com/test5.html', ps)
    self.assertExpectationEquals('fail', page,
                                 vendor_string=VENDOR_STRING_IMAGINATION,
                                 device_string=DEVICE_STRING_SGX)
    self.assertExpectationEquals('pass', page,
                                 vendor_string=VENDOR_STRING_IMAGINATION,
                                 device_string='Triangle Monster 3000')
    self.assertExpectationEquals('pass', page,
                                 vendor_string='Acme',
                                 device_string=DEVICE_STRING_SGX)

  # Test ANGLE conditions.
  def testANGLEConditions(self):
    ps = story_set.StorySet()
    page = page_module.Page('http://test.com/test6.html', ps)
    self.assertExpectationEquals('pass', page, StubPlatform('win'),
                                 gl_renderer='Direct3D11')
    self.assertExpectationEquals('fail', page, StubPlatform('win'),
                                 gl_renderer='Direct3D9')

  # Ensure retry mechanism is working.
  def testFlakyExpectation(self):
    ps = story_set.StorySet()
    page = page_module.Page('http://test.com/test7.html', ps)
    self.assertExpectationEquals('flaky', page)
    self.assertEquals(5, self.getRetriesForPage(page))

  # Ensure the filtering from the TestExpectations superclass still works.
  def testFlakyPerPlatformExpectation(self):
    ps = story_set.StorySet()
    page1 = page_module.Page('http://test.com/test8.html', ps)
    self.assertExpectationEquals('flaky', page1, StubPlatform('win'))
    self.assertEquals(6, self.getRetriesForPage(page1, StubPlatform('win')))
    self.assertExpectationEquals('pass', page1, StubPlatform('mac'))
    self.assertEquals(0, self.getRetriesForPage(page1, StubPlatform('mac')))
    page2 = page_module.Page('http://test.com/wildcardtest1.html', ps)
    self.assertExpectationEquals('flaky', page2, StubPlatform('win'))
    self.assertEquals(7, self.getRetriesForPage(page2, StubPlatform('win')))
    self.assertExpectationEquals('pass', page2, StubPlatform('mac'))
    self.assertEquals(0, self.getRetriesForPage(page2, StubPlatform('mac')))
