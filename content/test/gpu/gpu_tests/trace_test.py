# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import trace_test_expectations
import page_sets

from telemetry import benchmark
from telemetry.page import page_test
from telemetry.core.platform import tracing_category_filter
from telemetry.core.platform import tracing_options
from telemetry.timeline import model

TOPLEVEL_GL_CATEGORY = 'gpu_toplevel'
TOPLEVEL_CATEGORIES = ['disabled-by-default-gpu.device',
                       'disabled-by-default-gpu.service']

test_harness_script = r"""
  var domAutomationController = {};

  domAutomationController._finished = false;

  domAutomationController.setAutomationId = function(id) {}

  domAutomationController.send = function(msg) {
    domAutomationController._finished = true;
  }

  window.domAutomationController = domAutomationController;
"""

class _TraceValidator(page_test.PageTest):
  def ValidateAndMeasurePage(self, page, tab, results):
    timeline_data = tab.browser.platform.tracing_controller.Stop()
    timeline_model = model.TimelineModel(timeline_data)

    categories_set = set(TOPLEVEL_CATEGORIES)
    for event in timeline_model.IterAllEvents():
      if event.args.get('gl_category', None) == TOPLEVEL_GL_CATEGORY:
        categories_set.discard(event.category)
      if not categories_set:
        break
    else:
      raise page_test.Failure(self._FormatException(sorted(categories_set)))

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--enable-logging')

  def WillNavigateToPage(self, page, tab):
    cat_string = ','.join(TOPLEVEL_CATEGORIES)
    cat_filter = tracing_category_filter.TracingCategoryFilter(cat_string)
    options = tracing_options.TracingOptions()
    options.enable_chrome_trace = True
    tab.browser.platform.tracing_controller.Start(options, cat_filter, 60)

  def _FormatException(self, categories):
    return 'Trace markers for GPU categories were not found: %s' % categories

class TraceTest(benchmark.Benchmark):
  """Tests GPU traces"""
  test = _TraceValidator

  def CreateExpectations(self):
    return trace_test_expectations.TraceTestExpectations()

  def CreatePageSet(self, options):
    # Utilize pixel tests page set as a set of simple pages to load.
    page_set = page_sets.PixelTestsPageSet()
    for page in page_set.pages:
      page.script_to_evaluate_on_commit = test_harness_script
    return page_set
