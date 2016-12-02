# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from telemetry import benchmark as benchmark_module
from telemetry.page import page as page_module
from telemetry.page import legacy_page_test
from telemetry.page import shared_page_state as shared_page_state_module
from telemetry.testing import fakes

from gpu_tests import exception_formatter
from gpu_tests import gpu_test_expectations

class TestBase(benchmark_module.Benchmark):
  """Base classes for all GPU tests in this directory. Implements
  support for per-page test expectations."""

  def __init__(self, max_failures=None):
    super(TestBase, self).__init__(max_failures=max_failures)
    self._cached_expectations = None

  def GetExpectations(self):
    """Returns the expectations that apply to this test."""
    if not self._cached_expectations:
      self._cached_expectations = self._CreateExpectations()
    if not isinstance(self._cached_expectations,
                      gpu_test_expectations.GpuTestExpectations):
      raise Exception('gpu_test_base requires use of GpuTestExpectations')
    return self._cached_expectations

  def _CreateExpectations(self):
    # By default, creates an empty GpuTestExpectations object. Override
    # this in subclasses to set up test-specific expectations. Must
    # return an instance of GpuTestExpectations or a subclass.
    #
    # Do not call this directly. Call GetExpectations where necessary.
    return gpu_test_expectations.GpuTestExpectations()


# Provides a single subclass of PageTest in case it's useful in the
# future.
class ValidatorBase(legacy_page_test.LegacyPageTest):
  def __init__(self,
               needs_browser_restart_after_each_page=False,
               clear_cache_before_each_run=False):
    super(ValidatorBase, self).__init__(
      needs_browser_restart_after_each_page=\
        needs_browser_restart_after_each_page,
      clear_cache_before_each_run=clear_cache_before_each_run)

  def ValidateAndMeasurePage(self, page, tab, result):
    pass

def _CanRunOnBrowser(browser_info, page):
  expectations = page.GetExpectations()
  return expectations.GetExpectationForPage(
    browser_info.browser, page) != 'skip'

def RunStoryWithRetries(cls, shared_page_state, results):
  page = shared_page_state.current_page
  expectations = page.GetExpectations()
  expectation = 'pass'
  if expectations:
    expectation = expectations.GetExpectationForPage(
      shared_page_state.browser, page)
  if expectation == 'skip':
    raise Exception(
      'Skip expectations should have been handled in CanRunOnBrowser')
  try:
    super(cls, shared_page_state).RunStory(results)
  except Exception:
    if expectation == 'pass':
      raise
    elif expectation == 'fail':
      msg = 'Expected exception while running %s' % page.display_name
      exception_formatter.PrintFormattedException(msg=msg)
      return
    if expectation != 'flaky':
      logging.warning(
        'Unknown expectation %s while handling exception for %s',
        expectation, page.display_name)
      raise
    # Flaky tests are handled here.
    num_retries = expectations.GetFlakyRetriesForPage(
      shared_page_state.browser, page)
    if not num_retries:
      # Re-raise the exception.
      raise
    # Re-run the test up to |num_retries| times.
    for ii in xrange(0, num_retries):
      print 'FLAKY TEST FAILURE, retrying: ' + page.display_name
      try:
        super(cls, shared_page_state).RunStory(results)
        break
      except Exception:
        # Squelch any exceptions from any but the last retry.
        if ii == num_retries - 1:
          raise
  else:
    if expectation == 'fail':
      logging.warning(
          '%s was expected to fail, but passed.\n', page.display_name)

class GpuSharedPageState(shared_page_state_module.SharedPageState):
  def CanRunOnBrowser(self, browser_info, page):
    return _CanRunOnBrowser(browser_info, page)

  def RunStory(self, results):
    RunStoryWithRetries(GpuSharedPageState, self, results)


# TODO(kbr): re-evaluate the need for this SharedPageState
# subclass. It's only used by the WebGL conformance suite.
class DesktopGpuSharedPageState(
    shared_page_state_module.SharedDesktopPageState):
  def CanRunOnBrowser(self, browser_info, page):
    return _CanRunOnBrowser(browser_info, page)

  def RunStory(self, results):
    RunStoryWithRetries(DesktopGpuSharedPageState, self, results)


class FakeGpuSharedPageState(fakes.FakeSharedPageState):
  def CanRunOnBrowser(self, browser_info, page):
    return _CanRunOnBrowser(browser_info, page)

  def RunStory(self, results):
    RunStoryWithRetries(FakeGpuSharedPageState, self, results)


class PageBase(page_module.Page):
  # The convention is that pages subclassing this class must be
  # configured with the test expectations.
  def __init__(self, url, page_set=None, base_dir=None, name='',
               shared_page_state_class=GpuSharedPageState,
               make_javascript_deterministic=True,
               expectations=None):
    super(PageBase, self).__init__(
      url=url, page_set=page_set, base_dir=base_dir, name=name,
      shared_page_state_class=shared_page_state_class,
      make_javascript_deterministic=make_javascript_deterministic)
    # Disable automatic garbage collection to reduce the test's cycle time.
    self._collect_garbage_before_run = False

    # TODO(kbr): this is fragile -- if someone needs to independently
    # derive a new independent SharedPageState subclass which handles
    # skip expectations, this code will have to be updated.
    assert issubclass(
      shared_page_state_class,
      (GpuSharedPageState,
       DesktopGpuSharedPageState,
       FakeGpuSharedPageState)), \
      'shared_page_state_class must be one which handles skip expectations'


    # TODO(kbr): this is fragile -- if someone changes the
    # shared_page_state_class to something that doesn't handle skip
    # expectations, then they'll hit the exception in
    # RunStoryWithRetries, above. Need to rethink.
    self._expectations = expectations

  def GetExpectations(self):
    return self._expectations
