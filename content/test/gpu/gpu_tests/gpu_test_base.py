# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from telemetry import benchmark as benchmark_module
from telemetry.core import exceptions
from telemetry.page import page as page_module
from telemetry.page import page_test
from telemetry.page import shared_page_state
from telemetry.value import skip

import exception_formatter
import gpu_test_expectations

"""Base classes for all GPU tests in this directory. Implements
support for per-page test expectations."""

def _PageOperationWrapper(page, tab, expectations,
                          show_expected_failure, inner_func,
                          results=None):
  """Wraps an operation to be done against the page. If an error has
  occurred in an earlier step, skips this entirely."""
  if page.HadError():
    return
  expectation = 'pass'
  if expectations:
    expectation = expectations.GetExpectationForPage(tab.browser, page)
  if expectation == 'skip':
    raise Exception(
      'Skip expectations should have been handled at a higher level')
  try:
    inner_func()
  except Exception:
    page.SetHadError()
    if expectation == 'pass':
      raise
    elif expectation == 'fail':
      msg = 'Expected exception while running %s' % page.display_name
      exception_formatter.PrintFormattedException(msg=msg)
    elif expectation == 'flaky':
      if not page.GetSuppressFlakyFailures():
        raise
    else:
      logging.warning(
          'Unknown expectation %s while handling exception for %s' %
          (expectation, page.display_name))
      raise
  else:
    if show_expected_failure and expectation == 'fail':
      logging.warning(
          '%s was expected to fail, but passed.\n', page.display_name)


class TestBase(benchmark_module.Benchmark):
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


class ValidatorBase(page_test.PageTest):
  def __init__(self,
               needs_browser_restart_after_each_page=False,
               discard_first_result=False,
               clear_cache_before_each_run=False):
    super(ValidatorBase, self).__init__(
      needs_browser_restart_after_each_page=\
        needs_browser_restart_after_each_page,
      discard_first_result=discard_first_result,
      clear_cache_before_each_run=clear_cache_before_each_run)

  def WillNavigateToPage(self, page, tab):
    """Does operations before the page is navigated. Do not override this
    method. Override WillNavigateToPageInner, below."""
    _PageOperationWrapper(
      page, tab, page.GetExpectations(), False,
      lambda: self.WillNavigateToPageInner(page, tab))

  def DidNavigateToPage(self, page, tab):
    """Does operations right after the page is navigated and after all
    waiting for completion has occurred. Do not override this method.
    Override DidNavigateToPageInner, below."""
    _PageOperationWrapper(
      page, tab, page.GetExpectations(), False,
      lambda: self.DidNavigateToPageInner(page, tab))

  def ValidateAndMeasurePage(self, page, tab, results):
    """Validates and measures the page, taking into account test
    expectations. Do not override this method. Override
    ValidateAndMeasurePageInner, below."""

    # First figure out whether this test is marked flaky in the test
    # expectations.
    num_retries = page.GetExpectations().GetFlakyRetriesForPage(
      tab.browser, page)

    try:
      _PageOperationWrapper(
        page, tab, page.GetExpectations(), True,
        lambda: self.ValidateAndMeasurePageInner(page, tab, results),
        results=results)
    except Exception:
      # If we're going to re-execute a flaky test in the finally
      # clause, then we have to squelch this exception.
      if num_retries == 0:
        raise
    finally:
      if page.HadError() and num_retries:
        # Emulate the logic that's in shared_page_state to re-run the
        # test up to |num_retries| times. It would be ideal if it
        # could be reused (by calling SharedPageState.RunStory), but
        # unfortunately it's not possible to fetch the active
        # SharedPageState on demand.
        for ii in xrange(0, num_retries):
          print 'FLAKY TEST FAILURE, retrying: ' + page.display_name
          page.ClearHadError()
          page.SetSuppressFlakyFailures(False)
          try:
            self.WillNavigateToPage(page, tab)
            page.RunNavigateSteps(page.cached_action_runner)
            self.DidNavigateToPage(page, tab)
            page.RunPageInteractions(page.cached_action_runner)
            # Must not re-enter ourselves!
            _PageOperationWrapper(
              page, tab, page.GetExpectations(), True,
              lambda: self.ValidateAndMeasurePageInner(page, tab, results),
              results=results)
          except Exception:
            # Squelch any exceptions from any but the last retry.
            if ii == num_retries - 1:
              raise
          finally:
            # If the retry succeeded, stop.
            if not page.HadError():
              break
            # Otherwise, clear the error state and retry.
            page.ClearHadError()
      # Clear the error state of the page at this point so that if
      # --page-repeat or --pageset-repeat are used, the subsequent
      # iterations don't turn into no-ops.
      page.ClearHadError()

  def WillNavigateToPageInner(self, page, tab):
    pass

  def DidNavigateToPageInner(self, page, tab):
    pass

  def ValidateAndMeasurePageInner(self, page, tab, results):
    pass


# NOTE: if you change this logic you must change the logic in
# FakeGpuSharedPageState (gpu_test_base_unittest.py) as well.
def _CanRunOnBrowser(browser_info, page):
  expectations = page.GetExpectations()
  return expectations.GetExpectationForPage(
    browser_info.browser, page) != 'skip'


class GpuSharedPageState(shared_page_state.SharedPageState):
  def CanRunOnBrowser(self, browser_info, page):
    return _CanRunOnBrowser(browser_info, page)


# TODO(kbr): re-evaluate the need for this SharedPageState
# subclass. It's only used by the WebGL conformance suite.
class DesktopGpuSharedPageState(
    shared_page_state.SharedDesktopPageState):
  def CanRunOnBrowser(self, browser_info, page):
    return _CanRunOnBrowser(browser_info, page)


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
    # TODO(kbr): this is fragile -- if someone changes the
    # shared_page_state_class to something that doesn't handle skip
    # expectations, then they'll hit the exception in
    # _PageOperationWrapper, above. Need to rethink.
    self._expectations = expectations
    self._had_error = False
    self._cached_action_runner = None
    self._suppress_flaky_failures = True

  def GetExpectations(self):
    return self._expectations

  def HadError(self):
    return self._had_error

  def SetHadError(self):
    self._had_error = True

  def ClearHadError(self):
    self._had_error = False

  def SetSuppressFlakyFailures(self, value):
    self._suppress_flaky_failures = value

  def GetSuppressFlakyFailures(self):
    return self._suppress_flaky_failures

  @property
  def cached_action_runner(self):
    return self._cached_action_runner

  def RunNavigateSteps(self, action_runner):
    """Runs navigation steps, taking into account test expectations.
    Do not override this method. Override RunNavigateStepsInner, below."""
    self._cached_action_runner = action_runner
    def Functor():
      self.RunDefaultNavigateSteps(action_runner)
      self.RunNavigateStepsInner(action_runner)
    _PageOperationWrapper(self, action_runner.tab, self.GetExpectations(),
                          False, Functor)

  def RunDefaultNavigateSteps(self, action_runner):
    """Runs the default set of navigation steps inherited from page.Page."""
    super(PageBase, self).RunNavigateSteps(action_runner)

  def RunNavigateStepsInner(self, action_runner):
    pass

  def RunPageInteractions(self, action_runner):
    """Runs page interactions, taking into account test expectations. Do not
    override this method. Override RunPageInteractionsInner, below."""
    def Functor():
      super(PageBase, self).RunPageInteractions(action_runner)
      self.RunPageInteractionsInner(action_runner)
    _PageOperationWrapper(self, action_runner.tab, self.GetExpectations(),
                          False, Functor)

  def RunPageInteractionsInner(self, action_runner):
    pass
