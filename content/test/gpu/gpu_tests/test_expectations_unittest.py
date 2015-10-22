# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import unittest

from gpu_tests import test_expectations

from telemetry.page import page as page_module
from telemetry.story import story_set

class StubPlatform(object):
  def __init__(self, os_name, os_version_name=None):
    self.os_name = os_name
    self.os_version_name = os_version_name

  def GetOSName(self):
    return self.os_name

  def GetOSVersionName(self):
    return self.os_version_name


class StubBrowser(object):
  def __init__(self, platform, browser_type=None):
    self.platform = platform
    self.browser_type = browser_type

  @property
  def supports_system_info(self):
    return False


class SampleExpectationSubclass(test_expectations.Expectation):
  def __init__(self, expectation, pattern, conditions=None, bug=None):
    self.valid_condition_matched = False
    self.valid_condition_unmatched = False
    super(SampleExpectationSubclass, self).__init__(
      expectation, pattern, conditions=conditions, bug=bug)

  def ParseCondition(self, condition):
    if condition == 'valid_condition_matched':
      self.valid_condition_matched = True
    elif condition == 'valid_condition_unmatched':
      self.valid_condition_unmatched = True
    else:
      super(SampleExpectationSubclass, self).ParseCondition(condition)


class SampleTestExpectations(test_expectations.TestExpectations):
  def CreateExpectation(self, expectation, url_pattern, conditions=None,
                        bug=None):
    return SampleExpectationSubclass(expectation, url_pattern,
                                     conditions=conditions, bug=bug)

  def SetExpectations(self):
    self.Fail('page1.html', ['win', 'mac'], bug=123)
    self.Fail('page2.html', ['vista'], bug=123)
    self.Fail('page3.html', bug=123)
    self.Fail('page4.*', bug=123)
    self.Fail('http://test.com/page5.html', bug=123)
    self.Fail('Pages.page_6')
    self.Fail('page7.html', ['mountainlion'])
    self.Fail('page8.html', ['mavericks'])
    self.Fail('page9.html', ['yosemite'])
    # Test browser conditions.
    self.Fail('page10.html', ['android', 'android-webview-shell'], bug=456)
    # Test user defined conditions.
    self.Fail('page11.html', ['win', 'valid_condition_matched'])
    self.Fail('page12.html', ['win', 'valid_condition_unmatched'])
    # Test file:// scheme.
    self.Fail('conformance/attribs/page13.html')
    # Test file:// scheme on Windows.
    self.Fail('conformance/attribs/page14.html', ['win'])
    # Explicitly matched paths have precedence over wildcards.
    self.Fail('conformance/glsl/*')
    self.Skip('conformance/glsl/page15.html')

  def ExpectationAppliesToPage(self, expectation, browser, page):
    if not super(SampleTestExpectations,
        self).ExpectationAppliesToPage(expectation, browser, page):
      return False
    # These tests can probably be expressed more tersely, but that
    # would reduce readability.
    if (not expectation.valid_condition_matched and
        not expectation.valid_condition_unmatched):
      return True
    return expectation.valid_condition_matched

class TestExpectationsTest(unittest.TestCase):
  def setUp(self):
    self.expectations = SampleTestExpectations()

  def assertExpectationEquals(self, expected, page, platform=StubPlatform(''),
                              browser_type=None):
    self.expectations.ClearExpectationsCacheForTesting()
    result = self.expectations.GetExpectationForPage(
      StubBrowser(platform, browser_type=browser_type), page)
    self.assertEquals(expected, result)

  # Pages with no expectations should always return 'pass'
  def testNoExpectations(self):
    ps = story_set.StorySet()
    page = page_module.Page('http://test.com/page0.html', ps)
    self.assertExpectationEquals('pass', page, StubPlatform('win'))

  # Pages with expectations for an OS should only return them when running on
  # that OS
  def testOSExpectations(self):
    ps = story_set.StorySet()
    page = page_module.Page('http://test.com/page1.html', ps)
    self.assertExpectationEquals('fail', page, StubPlatform('win'))
    self.assertExpectationEquals('fail', page, StubPlatform('mac'))
    self.assertExpectationEquals('pass', page, StubPlatform('linux'))

  # Pages with expectations for an OS version should only return them when
  # running on that OS version
  def testOSVersionExpectations(self):
    ps = story_set.StorySet()
    page = page_module.Page('http://test.com/page2.html', ps)
    self.assertExpectationEquals('fail', page, StubPlatform('win', 'vista'))
    self.assertExpectationEquals('pass', page, StubPlatform('win', 'win7'))

  # Pages with non-conditional expectations should always return that
  # expectation regardless of OS or OS version
  def testConditionlessExpectations(self):
    ps = story_set.StorySet()
    page = page_module.Page('http://test.com/page3.html', ps)
    self.assertExpectationEquals('fail', page, StubPlatform('win'))
    self.assertExpectationEquals('fail', page, StubPlatform('mac', 'lion'))
    self.assertExpectationEquals('fail', page, StubPlatform('linux'))

  # Expectations with wildcard characters should return for matching patterns
  def testWildcardExpectations(self):
    ps = story_set.StorySet()
    page = page_module.Page('http://test.com/page4.html', ps)
    page_js = page_module.Page('http://test.com/page4.html', ps)
    self.assertExpectationEquals('fail', page, StubPlatform('win'))
    self.assertExpectationEquals('fail', page_js, StubPlatform('win'))

  # Expectations with absolute paths should match the entire path
  def testAbsoluteExpectations(self):
    ps = story_set.StorySet()
    page = page_module.Page('http://test.com/page5.html', ps)
    page_org = page_module.Page('http://test.org/page5.html', ps)
    page_https = page_module.Page('https://test.com/page5.html', ps)
    self.assertExpectationEquals('fail', page, StubPlatform('win'))
    self.assertExpectationEquals('pass', page_org, StubPlatform('win'))
    self.assertExpectationEquals('pass', page_https, StubPlatform('win'))

  # Expectations can be set against page names as well as urls
  def testPageNameExpectations(self):
    ps = story_set.StorySet()
    page = page_module.Page('http://test.com/page6.html', ps,
                            name='Pages.page_6')
    self.assertExpectationEquals('fail', page)

  # Verify version-specific Mac expectations.
  def testMacVersionExpectations(self):
    ps = story_set.StorySet()
    page = page_module.Page('http://test.com/page7.html', ps)
    self.assertExpectationEquals('fail', page,
                                 StubPlatform('mac', 'mountainlion'))
    self.assertExpectationEquals('pass', page,
                                 StubPlatform('mac', 'mavericks'))
    self.assertExpectationEquals('pass', page,
                                 StubPlatform('mac', 'yosemite'))
    ps = story_set.StorySet()
    page = page_module.Page('http://test.com/page8.html', ps)
    self.assertExpectationEquals('pass', page,
                                 StubPlatform('mac', 'mountainlion'))
    self.assertExpectationEquals('fail', page,
                                 StubPlatform('mac', 'mavericks'))
    self.assertExpectationEquals('pass', page,
                                 StubPlatform('mac', 'yosemite'))
    ps = story_set.StorySet()
    page = page_module.Page('http://test.com/page9.html', ps)
    self.assertExpectationEquals('pass', page,
                                 StubPlatform('mac', 'mountainlion'))
    self.assertExpectationEquals('pass', page,
                                 StubPlatform('mac', 'mavericks'))
    self.assertExpectationEquals('fail', page,
                                 StubPlatform('mac', 'yosemite'))

  # Test browser type conditions.
  def testBrowserTypeConditions(self):
    ps = story_set.StorySet()
    page = page_module.Page('http://test.com/page10.html', ps)
    self.assertExpectationEquals('pass', page, StubPlatform('android'),
                                 browser_type='android-content-shell')
    self.assertExpectationEquals('fail', page, StubPlatform('android'),
                                 browser_type='android-webview-shell')

  # Pages with user-defined conditions.
  def testUserDefinedConditions(self):
    ps = story_set.StorySet()
    page = page_module.Page('http://test.com/page11.html', ps)
    self.assertExpectationEquals('fail', page, StubPlatform('win'))
    page = page_module.Page('http://test.com/page12.html', ps)
    self.assertExpectationEquals('pass', page, StubPlatform('win'))

  # The file:// scheme is treated specially by Telemetry; it's
  # translated into an HTTP request against localhost. Expectations
  # against it must continue to work.
  def testFileScheme(self):
    ps = story_set.StorySet()
    page = page_module.Page('file://conformance/attribs/page13.html', ps)
    self.assertExpectationEquals('fail', page)

  # Telemetry uses backslashes in its file:// URLs on Windows.
  def testFileSchemeOnWindows(self):
    ps = story_set.StorySet()
    page = page_module.Page('file://conformance\\attribs\\page14.html', ps)
    self.assertExpectationEquals('pass', page)
    self.assertExpectationEquals('fail', page, StubPlatform('win'))

  def testExplicitPathsHavePrecedenceOverWildcards(self):
    ps = story_set.StorySet()
    page = page_module.Page('http://test.com/conformance/glsl/page00.html', ps)
    self.assertExpectationEquals('fail', page)
    page = page_module.Page('http://test.com/conformance/glsl/page15.html', ps)
    self.assertExpectationEquals('skip', page)

  def testQueryArgsAreStrippedFromFileURLs(self):
    ps = story_set.StorySet()
    page = page_module.Page(
      'file://conformance/glsl/page15.html?webglVersion=2', ps)
    self.assertExpectationEquals('skip', page)
