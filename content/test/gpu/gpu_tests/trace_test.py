# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import trace_test_expectations
import page_sets

from telemetry import benchmark
from telemetry.page import page_test
from telemetry.core.platform import tracing_category_filter
from telemetry.core.platform import tracing_options
from telemetry.timeline import model as model_module

TOPLEVEL_GL_CATEGORY = 'gpu_toplevel'
TOPLEVEL_SERVICE_CATEGORY = 'disabled-by-default-gpu.service'
TOPLEVEL_DEVICE_CATEGORY = 'disabled-by-default-gpu.device'
TOPLEVEL_CATEGORIES = [TOPLEVEL_SERVICE_CATEGORY, TOPLEVEL_DEVICE_CATEGORY]

test_harness_script = r"""
  var domAutomationController = {};

  domAutomationController._finished = false;

  domAutomationController.setAutomationId = function(id) {}

  domAutomationController.send = function(msg) {
    domAutomationController._finished = true;
  }

  window.domAutomationController = domAutomationController;
"""


class _TraceValidatorBase(page_test.PageTest):
  def GetCategoryName(self):
    raise NotImplementedError("GetCategoryName() Not implemented!")

  def ValidateAndMeasurePage(self, page, tab, results):
    timeline_data = tab.browser.platform.tracing_controller.Stop()
    timeline_model = model_module.TimelineModel(timeline_data)

    category_name = self.GetCategoryName()
    event_iter = timeline_model.IterAllEvents(
        event_type_predicate=model_module.IsSliceOrAsyncSlice)
    for event in event_iter:
      if (event.args.get('gl_category', None) == TOPLEVEL_GL_CATEGORY and
          event.category == category_name):
        break
    else:
      raise page_test.Failure(self._FormatException(category_name))

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--enable-logging')

  def WillNavigateToPage(self, page, tab):
    cat_string = ','.join(TOPLEVEL_CATEGORIES)
    cat_filter = tracing_category_filter.TracingCategoryFilter(cat_string)
    options = tracing_options.TracingOptions()
    options.enable_chrome_trace = True
    tab.browser.platform.tracing_controller.Start(options, cat_filter, 60)

  def _FormatException(self, category):
    return 'Trace markers for GPU category was not found: %s' % category


class _TraceValidator(_TraceValidatorBase):
  def GetCategoryName(self):
    return TOPLEVEL_SERVICE_CATEGORY


class _DeviceTraceValidator(_TraceValidatorBase):
  def GetCategoryName(self):
    return TOPLEVEL_DEVICE_CATEGORY


class _TraceTestBase(benchmark.Benchmark):
  """Base class for the trace tests."""
  def CreatePageSet(self, options):
    # Utilize pixel tests page set as a set of simple pages to load.
    page_set = page_sets.PixelTestsPageSet(base_name=self.name)
    for page in page_set.pages:
      page.script_to_evaluate_on_commit = test_harness_script
    return page_set


class TraceTest(_TraceTestBase):
  """Tests GPU traces are plumbed through properly."""
  test = _TraceValidator
  name = 'TraceTest'

  @classmethod
  def Name(cls):
    return 'trace_test'

  def CreateExpectations(self):
    return trace_test_expectations.TraceTestExpectations()


class DeviceTraceTest(_TraceTestBase):
  """Tests GPU Device traces show up on devices that support it."""
  test = _DeviceTraceValidator
  name = 'DeviceTraceTest'

  @classmethod
  def Name(cls):
    return 'device_trace_test'

  def CreateExpectations(self):
    return trace_test_expectations.DeviceTraceTestExpectations()
