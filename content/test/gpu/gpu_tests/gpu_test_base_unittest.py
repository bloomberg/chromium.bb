# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import unittest

from telemetry import benchmark
from telemetry.core import util
from telemetry.story import story_set as story_set_module
from telemetry.testing import fakes
from telemetry.util import classes_util

util.AddDirToPythonPath(util.GetTelemetryDir(), 'third_party', 'mock')
import mock # pylint: disable=import-error

import gpu_test_base

def _GetGpuDir(*subdirs):
  gpu_dir = os.path.dirname(os.path.dirname(__file__))
  return os.path.join(gpu_dir, *subdirs)

# Unit tests verifying invariants of classes in GpuTestBase.

class NoOverridesTest(unittest.TestCase):
  def testValidatorBase(self):
    all_validators = classes_util.DiscoverClasses(
      _GetGpuDir('gpu_tests'), _GetGpuDir(), gpu_test_base.ValidatorBase)
    self.assertGreater(len(all_validators), 0)
    for validator in all_validators:
      self.assertEquals(gpu_test_base.ValidatorBase.ValidateAndMeasurePage,
                        validator.ValidateAndMeasurePage,
                        'Class %s should not override ValidateAndMeasurePage'
                        % validator.__name__)

  def testPageBase(self):
    all_pages = classes_util.DiscoverClasses(
      _GetGpuDir(), _GetGpuDir(), gpu_test_base.PageBase)
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

#
# Tests verifying interactions between Telemetry and GpuTestBase.
#

class FakeValidator(gpu_test_base.ValidatorBase):
  def __init__(self):
    super(FakeValidator, self).__init__()
    self.ValidateAndMeasurePageInner = mock.Mock()


class FakePage(gpu_test_base.PageBase):
  def __init__(self, page_set, name, expectations=None):
    super(FakePage, self).__init__(
      name=name,
      url='http://nonexistentserver.com/' + name,
      page_set=page_set,
      shared_page_state_class=fakes.FakeSharedPageState,
      expectations=expectations)
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
  def __init__(self, page_set, name, expectations=None):
    super(FailingPage, self).__init__(page_set, name, expectations=expectations)
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
    page = FakePage(test.GetFakeStorySet(), 'page1')
    page.RunNavigateStepsInner = manager.page.RunNavigateStepsInner
    page.RunPageInteractionsInner = manager.page.RunPageInteractionsInner
    test.validator.ValidateAndMeasurePageInner = (
      manager.validator.ValidateAndMeasurePageInner)
    test.AddFakePage(page)
    self.assertEqual(test.Run(finder_options), 0,
                     'Test should run with no errors')
    expected = [mock.call.page.RunNavigateStepsInner(mock.ANY),
                mock.call.page.RunPageInteractionsInner(mock.ANY),
                mock.call.validator.ValidateAndMeasurePageInner(
                  page, mock.ANY, mock.ANY)]
    self.assertTrue(manager.mock_calls == expected)

  def testFailingPage(self):
    test, finder_options = self.setupTest()
    page = FailingPage(test.GetFakeStorySet(), 'page1')
    test.AddFakePage(page)
    self.assertNotEqual(test.Run(finder_options), 0, 'Test should fail')

  def testExpectedFailure(self):
    test, finder_options = self.setupTest()
    page = FailingPage(test.GetFakeStorySet(), 'page1',
                       expectations=test.GetExpectations())
    test.AddFakePage(page)
    test.GetExpectations().Fail('page1')
    self.assertEqual(test.Run(finder_options), 0,
                     'Test should run with no errors')
    self.assertFalse(page.RunPageInteractionsInner.called)
    self.assertFalse(test.validator.ValidateAndMeasurePageInner.called)

  def testPassAfterExpectedFailure(self):
    test, finder_options = self.setupTest()
    manager = mock.Mock()
    page = FailingPage(test.GetFakeStorySet(), 'page1',
                       expectations=test.GetExpectations())
    test.AddFakePage(page)
    test.GetExpectations().Fail('page1')
    page = FakePage(test.GetFakeStorySet(), 'page2')
    page.RunNavigateStepsInner = manager.page.RunNavigateStepsInner
    page.RunPageInteractionsInner = manager.page.RunPageInteractionsInner
    test.validator.ValidateAndMeasurePageInner = (
      manager.validator.ValidateAndMeasurePageInner)
    test.AddFakePage(page)
    self.assertEqual(test.Run(finder_options), 0,
                     'Test should run with no errors')
    expected = [mock.call.page.RunNavigateStepsInner(mock.ANY),
                mock.call.page.RunPageInteractionsInner(mock.ANY),
                mock.call.validator.ValidateAndMeasurePageInner(
                  page, mock.ANY, mock.ANY)]
    self.assertTrue(manager.mock_calls == expected)
