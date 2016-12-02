# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from gpu_tests import gpu_test_base
from gpu_tests import trace_test_expectations
import page_sets

from telemetry.page import legacy_page_test
from telemetry.timeline import model as model_module
from telemetry.timeline import tracing_config

TOPLEVEL_GL_CATEGORY = 'gpu_toplevel'
TOPLEVEL_SERVICE_CATEGORY = 'disabled-by-default-gpu.service'
TOPLEVEL_DEVICE_CATEGORY = 'disabled-by-default-gpu.device'
TOPLEVEL_CATEGORIES = [TOPLEVEL_SERVICE_CATEGORY, TOPLEVEL_DEVICE_CATEGORY]

test_harness_script = r"""
  var domAutomationController = {};

  domAutomationController._finished = false;

  domAutomationController.setAutomationId = function(id) {}

  domAutomationController.send = function(msg) {
    // Issue a read pixel to synchronize the gpu process to ensure
    // the asynchronous category enabling is finished.
    var temp_canvas = document.createElement("canvas")
    temp_canvas.width = 1;
    temp_canvas.height = 1;
    var temp_gl = temp_canvas.getContext("experimental-webgl") ||
                  temp_canvas.getContext("webgl");
    if (temp_gl) {
      temp_gl.clear(temp_gl.COLOR_BUFFER_BIT);
      var id = new Uint8Array(4);
      temp_gl.readPixels(0, 0, 1, 1, temp_gl.RGBA, temp_gl.UNSIGNED_BYTE, id);
    } else {
      console.log('Failed to get WebGL context.');
    }

    domAutomationController._finished = true;
  }

  window.domAutomationController = domAutomationController;
"""


class TraceValidatorBase(gpu_test_base.ValidatorBase):
  def GetCategoryName(self):
    raise NotImplementedError("GetCategoryName() Not implemented!")

  def ValidateAndMeasurePage(self, page, tab, results):
    timeline_data = tab.browser.platform.tracing_controller.StopTracing()
    timeline_model = model_module.TimelineModel(timeline_data)

    category_name = self.GetCategoryName()
    event_iter = timeline_model.IterAllEvents(
        event_type_predicate=model_module.IsSliceOrAsyncSlice)
    for event in event_iter:
      if (event.args.get('gl_category', None) == TOPLEVEL_GL_CATEGORY and
          event.category == category_name):
        break
    else:
      raise legacy_page_test.Failure(self._FormatException(category_name))

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--enable-logging')
    options.AppendExtraBrowserArgs('--enable-experimental-canvas-features')

  def WillNavigateToPage(self, page, tab):
    config = tracing_config.TracingConfig()
    config.chrome_trace_config.category_filter.AddExcludedCategory('*')
    for cat in TOPLEVEL_CATEGORIES:
      config.chrome_trace_config.category_filter.AddDisabledByDefault(
          cat)
    config.enable_chrome_trace = True
    tab.browser.platform.tracing_controller.StartTracing(config, 60)

  def _FormatException(self, category):
    return 'Trace markers for GPU category was not found: %s' % category


class _TraceValidator(TraceValidatorBase):
  def GetCategoryName(self):
    return TOPLEVEL_SERVICE_CATEGORY


class _DeviceTraceValidator(TraceValidatorBase):
  def GetCategoryName(self):
    return TOPLEVEL_DEVICE_CATEGORY


class TraceTestBase(gpu_test_base.TestBase):
  """Base class for the trace tests."""
  def CreateStorySet(self, options):
    # Utilize pixel tests page set as a set of simple pages to load.
    story_set = page_sets.PixelTestsStorySet(self.GetExpectations(),
                                             base_name=self.Name())
    for story in story_set:
      story.script_to_evaluate_on_commit = test_harness_script
    return story_set


class TraceTest(TraceTestBase):
  """Tests GPU traces are plumbed through properly."""
  test = _TraceValidator
  name = 'TraceTest'

  @classmethod
  def Name(cls):
    return 'trace_test'

  def _CreateExpectations(self):
    return trace_test_expectations.TraceTestExpectations()


class DeviceTraceTest(TraceTestBase):
  """Tests GPU Device traces show up on devices that support it."""
  test = _DeviceTraceValidator
  name = 'DeviceTraceTest'

  @classmethod
  def Name(cls):
    return 'device_trace_test'

  def _CreateExpectations(self):
    return trace_test_expectations.DeviceTraceTestExpectations()
