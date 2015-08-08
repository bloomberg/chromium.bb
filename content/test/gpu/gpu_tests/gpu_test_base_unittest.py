# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import unittest

from telemetry import benchmark
from telemetry.core import discover
from telemetry.core import util
from telemetry.story import story_set as story_set_module
from telemetry.testing import fakes

util.AddDirToPythonPath(util.GetTelemetryDir(), 'third_party', 'mock')
import mock # pylint: disable=import-error

import gpu_test_base

def _GetGpuDir(*subdirs):
  gpu_dir = os.path.dirname(os.path.dirname(__file__))
  return os.path.join(gpu_dir, *subdirs)

# Unit tests verifying invariants of classes in GpuTestBase.

class NoOverridesTest(unittest.TestCase):
  def testValidatorBase(self):
    all_validators = discover.DiscoverClasses(
      _GetGpuDir('gpu_tests'), _GetGpuDir(),
      gpu_test_base.ValidatorBase,
      index_by_class_name=True).values()
    self.assertGreater(len(all_validators), 0)
    for validator in all_validators:
      self.assertEquals(gpu_test_base.ValidatorBase.WillNavigateToPage,
                        validator.WillNavigateToPage,
                        'Class %s should not override WillNavigateToPage'
                        % validator.__name__)
      self.assertEquals(gpu_test_base.ValidatorBase.DidNavigateToPage,
                        validator.DidNavigateToPage,
                        'Class %s should not override DidNavigateToPage'
                        % validator.__name__)
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
                        'Class %s should not override RunNavigateSteps'
                        % page.__name__)
      self.assertEquals(gpu_test_base.PageBase.RunPageInteractions,
                        page.RunPageInteractions,
                        'Class %s should not override RunPageInteractions'
                        % page.__name__)

#
# Tests verifying interactions between Telemetry and GpuTestBase.
#

class FakeValidator(gpu_test_base.ValidatorBase):
  def __init__(self):
    super(FakeValidator, self).__init__()
    self.WillNavigateToPageInner = mock.Mock()
    self.DidNavigateToPageInner = mock.Mock()
    self.ValidateAndMeasurePageInner = mock.Mock()


class FakeGpuSharedPageState(fakes.FakeSharedPageState):
  # NOTE: if you change this logic you must change the logic in
  # GpuSharedPageState (gpu_test_base.py) as well.
  def CanRunOnBrowser(self, browser_info, page):
    expectations = page.GetExpectations()
    return expectations.GetExpectationForPage(
      browser_info.browser, page) != 'skip'


class FakePage(gpu_test_base.PageBase):
  def __init__(self, benchmark, name):
    super(FakePage, self).__init__(
      name=name,
      url='http://nonexistentserver.com/' + name,
      page_set=benchmark.GetFakeStorySet(),
      shared_page_state_class=FakeGpuSharedPageState,
      expectations=benchmark.GetExpectations())
    self.RunNavigateStepsInner = mock.Mock()
    self.RunPageInteractionsInner = mock.Mock()


class FakeTest(gpu_test_base.TestBase):
  def __init__(self, max_failures=None):
    super(FakeTest, self).__init__(max_failures)
    self._fake_pages = []
    self._fake_story_set = story_set_module.StorySet()
    self._created_story_set = False
    self.validator = FakeValidator()

  def _CreateExpectations(self):
    return super(FakeTest, self)._CreateExpectations()

  def CreatePageTest(self, options):
    return self.validator

  def GetFakeStorySet(self):
    return self._fake_story_set

  def AddFakePage(self, page):
    if self._created_story_set:
      raise Exception('Can not add any more fake pages')
    self._fake_pages.append(page)

  def CreateStorySet(self, options):
    if self._created_story_set:
      raise Exception('Can only create the story set once per FakeTest')
    for page in self._fake_pages:
      self._fake_story_set.AddStory(page)
    self._created_story_set = True
    return self._fake_story_set


class FailingPage(FakePage):
  def __init__(self, benchmark, name):
    super(FailingPage, self).__init__(benchmark, name)
    self.RunNavigateStepsInner.side_effect = Exception('Deliberate exception')


class PageExecutionTest(unittest.TestCase):
  def setupTest(self):
    finder_options = fakes.CreateBrowserFinderOptions()
    finder_options.browser_options.platform = fakes.FakeLinuxPlatform()
    finder_options.output_formats = ['none']
    finder_options.suppress_gtest_report = True
    finder_options.output_dir = None
    finder_options.upload_bucket = 'public'
    finder_options.upload_results = False
    testclass = FakeTest
    parser = finder_options.CreateParser()
    benchmark.AddCommandLineArgs(parser)
    testclass.AddCommandLineArgs(parser)
    options, dummy_args = parser.parse_args([])
    benchmark.ProcessCommandLineArgs(parser, options)
    testclass.ProcessCommandLineArgs(parser, options)
    test = testclass()
    return test, finder_options

  def testPassingPage(self):
    test, finder_options = self.setupTest()
    manager = mock.Mock()
    page = FakePage(test, 'page1')
    page.RunNavigateStepsInner = manager.page.RunNavigateStepsInner
    page.RunPageInteractionsInner = manager.page.RunPageInteractionsInner
    test.validator.WillNavigateToPageInner = (
      manager.validator.WillNavigateToPageInner)
    test.validator.DidNavigateToPageInner = (
      manager.validator.DidNavigateToPageInner)
    test.validator.ValidateAndMeasurePageInner = (
      manager.validator.ValidateAndMeasurePageInner)
    test.AddFakePage(page)
    self.assertEqual(test.Run(finder_options), 0,
                     'Test should run with no errors')
    expected = [mock.call.validator.WillNavigateToPageInner(
                  page, mock.ANY),
                mock.call.page.RunNavigateStepsInner(mock.ANY),
                mock.call.validator.DidNavigateToPageInner(
                  page, mock.ANY),
                mock.call.page.RunPageInteractionsInner(mock.ANY),
                mock.call.validator.ValidateAndMeasurePageInner(
                  page, mock.ANY, mock.ANY)]
    self.assertTrue(manager.mock_calls == expected)

  def testFailingPage(self):
    test, finder_options = self.setupTest()
    page = FailingPage(test, 'page1')
    test.AddFakePage(page)
    self.assertNotEqual(test.Run(finder_options), 0, 'Test should fail')

  def testExpectedFailure(self):
    test, finder_options = self.setupTest()
    page = FailingPage(test, 'page1')
    test.AddFakePage(page)
    test.GetExpectations().Fail('page1')
    self.assertEqual(test.Run(finder_options), 0,
                     'Test should run with no errors')
    self.assertFalse(page.RunPageInteractionsInner.called)
    self.assertFalse(test.validator.ValidateAndMeasurePageInner.called)

  def testSkipAtPageBaseLevel(self):
    test, finder_options = self.setupTest()
    page = FailingPage(test, 'page1')
    test.AddFakePage(page)
    test.GetExpectations().Skip('page1')
    self.assertEqual(test.Run(finder_options), 0,
                     'Test should run with no errors')
    self.assertFalse(test.validator.WillNavigateToPageInner.called)
    self.assertFalse(page.RunNavigateStepsInner.called)
    self.assertFalse(test.validator.DidNavigateToPageInner.called)
    self.assertFalse(page.RunPageInteractionsInner.called)
    self.assertFalse(test.validator.ValidateAndMeasurePageInner.called)

  def testSkipAtPageLevel(self):
    test, finder_options = self.setupTest()
    page = FakePage(test, 'page1')
    page.RunNavigateSteps = mock.Mock()
    page.RunPageInteractions = mock.Mock()
    test.validator.WillNavigateToPage = mock.Mock()
    test.validator.DidNavigateToPage = mock.Mock()
    test.validator.ValidateAndMeasurePage = mock.Mock()
    test.AddFakePage(page)
    test.GetExpectations().Skip('page1')
    self.assertEqual(test.Run(finder_options), 0,
                     'Test should run with no errors')
    self.assertFalse(test.validator.WillNavigateToPage.called)
    self.assertFalse(page.RunNavigateSteps.called)
    self.assertFalse(test.validator.DidNavigateToPage.called)
    self.assertFalse(page.RunPageInteractions.called)
    self.assertFalse(test.validator.ValidateAndMeasurePage.called)

  def testPassAfterExpectedFailure(self):
    test, finder_options = self.setupTest()
    manager = mock.Mock()
    page1 = FailingPage(test, 'page1')
    test.AddFakePage(page1)
    test.GetExpectations().Fail('page1')
    page2 = FakePage(test, 'page2')
    page2.RunNavigateStepsInner = manager.page.RunNavigateStepsInner
    page2.RunPageInteractionsInner = manager.page.RunPageInteractionsInner
    test.validator.WillNavigateToPageInner = (
      manager.validator.WillNavigateToPageInner)
    test.validator.DidNavigateToPageInner = (
      manager.validator.DidNavigateToPageInner)
    test.validator.ValidateAndMeasurePageInner = (
      manager.validator.ValidateAndMeasurePageInner)
    test.AddFakePage(page2)
    self.assertEqual(test.Run(finder_options), 0,
                     'Test should run with no errors')
    expected = [mock.call.validator.WillNavigateToPageInner(
                  page1, mock.ANY),
                mock.call.validator.WillNavigateToPageInner(
                  page2, mock.ANY),
                mock.call.page.RunNavigateStepsInner(mock.ANY),
                mock.call.validator.DidNavigateToPageInner(
                  page2, mock.ANY),
                mock.call.page.RunPageInteractionsInner(mock.ANY),
                mock.call.validator.ValidateAndMeasurePageInner(
                  page2, mock.ANY, mock.ANY)]
    self.assertTrue(manager.mock_calls == expected)
