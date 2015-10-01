# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import unittest

from telemetry.testing import fakes

import fake_win_amd_gpu_info
import gpu_test_base
import path_util
import webgl_conformance_expectations

class FakeWindowsPlatform(fakes.FakePlatform):
  @property
  def is_host_platform(self):
    return True

  def GetDeviceTypeName(self):
    return 'Desktop'

  def GetArchName(self):
    return 'x86_64'

  def GetOSName(self):
    return 'win'

  def GetOSVersionName(self):
    return 'win8'


class FakePage(gpu_test_base.PageBase):
  def __init__(self, url, expectations):
    super(FakePage, self).__init__(
      name=('WebglConformance.%s' %
            url.replace('/', '_').replace('-', '_').
            replace('\\', '_').rpartition('.')[0].replace('.', '_')),
      url='file://' + url,
      page_set=None,
      shared_page_state_class=gpu_test_base.FakeGpuSharedPageState,
      expectations=expectations)

class WebGLConformanceExpectationsTest(unittest.TestCase):
  def testGlslConstructVecMatIndexExpectationOnWin(self):
    possible_browser = fakes.FakePossibleBrowser()
    browser = possible_browser.Create(None)
    browser.platform = FakeWindowsPlatform()
    browser.returned_system_info = fakes.FakeSystemInfo(
      gpu_dict=fake_win_amd_gpu_info.FAKE_GPU_INFO)
    expectations = webgl_conformance_expectations.\
                   WebGLConformanceExpectations(
                     os.path.join(
                       path_util.GetChromiumSrcDir(),
                       'third_party', 'webgl', 'src', 'sdk', 'tests'))
    page = FakePage(
      'conformance/glsl/constructors/glsl-construct-vec-mat-index.html',
      expectations)
    expectation = expectations.GetExpectationForPage(browser, page)
    # TODO(kbr): change this expectation back to "flaky". crbug.com/534697
    self.assertEquals(expectation, 'fail')
