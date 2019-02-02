# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import sys

from gpu_tests import gpu_integration_test
from gpu_tests import path_util
from gpu_tests import pixel_test_pages
from gpu_tests import trace_test_expectations

from telemetry.timeline import model as model_module
from telemetry.timeline import tracing_config

gpu_relative_path = "content/test/data/gpu/"

data_paths = [os.path.join(
                  path_util.GetChromiumSrcDir(), gpu_relative_path),
              os.path.join(
                  path_util.GetChromiumSrcDir(), 'media', 'test', 'data')]

webgl_test_harness_script = r"""
  var domAutomationController = {};

  domAutomationController._finished = false;

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

basic_test_harness_script = r"""
  var domAutomationController = {};

  domAutomationController._proceed = false;

  domAutomationController._readyForActions = false;
  domAutomationController._succeeded = false;
  domAutomationController._finished = false;

  domAutomationController.send = function(msg) {
    domAutomationController._proceed = true;
    let lmsg = msg.toLowerCase();
    if (lmsg == "ready") {
      domAutomationController._readyForActions = true;
    } else {
      domAutomationController._finished = true;
      if (lmsg == "success") {
        domAutomationController._succeeded = true;
      } else {
        domAutomationController._succeeded = false;
      }
    }
  }

  window.domAutomationController = domAutomationController;
"""

# Presentation mode enums match DXGI_FRAME_PRESENTATION_MODE
_SWAP_CHAIN_PRESENTATION_MODE_COMPOSED = 0
_SWAP_CHAIN_PRESENTATION_MODE_OVERLAY = 1
_SWAP_CHAIN_PRESENTATION_MODE_NONE = 2
_SWAP_CHAIN_PRESENTATION_MODE_COMPOSITION_FAILURE = 3

# Pixel format enums match OverlayFormat in config/gpu/gpu_info.h
_SWAP_CHAIN_PIXEL_FORMAT_BGRA = 0
_SWAP_CHAIN_PIXEL_FORMAT_YUY2 = 1
_SWAP_CHAIN_PIXEL_FORMAT_NV12 = 2

_TEST_MAX_REPEATS = 30

_TEST_DONE = 0 # Test finished, either failed or succeeded
_TEST_REPEAT = 1 # Test failed, but it's flaky and should run again.

class TraceIntegrationTest(gpu_integration_test.GpuIntegrationTest):
  """Tests GPU traces are plumbed through properly.

  Also tests that GPU Device traces show up on devices that support them."""

  @classmethod
  def Name(cls):
    return 'trace_test'

  @classmethod
  def GenerateGpuTests(cls, options):
    # Include the device level trace tests, even though they're
    # currently skipped on all platforms, to give a hint that they
    # should perhaps be enabled in the future.
    for p in pixel_test_pages.DefaultPages('TraceTest'):
      yield (p.name, gpu_relative_path + p.url,
             {'browser_args': [],
              'category': cls._DisabledByDefaultTraceCategory('gpu.service'),
              'test_harness_script': webgl_test_harness_script,
              'finish_js_condition': 'domAutomationController._finished',
              'success_eval_func': 'CheckGLCategory'})
    for p in pixel_test_pages.DefaultPages('DeviceTraceTest'):
      yield (p.name, gpu_relative_path + p.url,
             {'browser_args': [],
              'category': cls._DisabledByDefaultTraceCategory('gpu.device'),
              'test_harness_script': webgl_test_harness_script,
              'finish_js_condition': 'domAutomationController._finished',
              'success_eval_func': 'CheckGLCategory'})
    for p in pixel_test_pages.DirectCompositionPages('VideoTraceTest'):
      success_eval_func = 'CheckVideoMode'
      if (p.other_args is not None and
          p.other_args.get('video_is_rotated', False)):
        # On several Intel GPUs we tested that support hardware overlays,
        # none of them promote a swap chain to hardware overlay if there is
        # rotation.
        success_eval_func = 'CheckVideoModeNoOverlay'
      yield (p.name, gpu_relative_path + p.url,
             {'browser_args': p.browser_args,
              'category': cls._DisabledByDefaultTraceCategory('gpu.service'),
              'test_harness_script': basic_test_harness_script,
              'finish_js_condition': 'domAutomationController._finished',
              'success_eval_func': success_eval_func})

  def RunActualGpuTest(self, test_path, *args):
    test_params = args[0]
    assert 'browser_args' in test_params
    assert 'category' in test_params
    assert 'test_harness_script' in test_params
    assert 'finish_js_condition' in test_params
    browser_args = test_params['browser_args']
    category = test_params['category']
    test_harness_script = test_params['test_harness_script']
    finish_js_condition = test_params['finish_js_condition']
    success_eval_func = test_params['success_eval_func']

    # Maximum repeat a flaky test 30 times
    for ii in range(_TEST_MAX_REPEATS):
      if ii > 0:
        logging.info('Try the test again: #%d', ii + 1)
      # The version of this test in the old GPU test harness restarted
      # the browser after each test, so continue to do that to match its
      # behavior.
      self.RestartBrowserWithArgs(self._AddDefaultArgs(browser_args))

      # Set up tracing.
      config = tracing_config.TracingConfig()
      config.chrome_trace_config.category_filter.AddExcludedCategory('*')
      config.chrome_trace_config.category_filter.AddDisabledByDefault(category)
      config.enable_chrome_trace = True
      tab = self.tab
      tab.browser.platform.tracing_controller.StartTracing(config, 60)

      # Perform page navigation.
      url = self.UrlOfStaticFilePath(test_path)
      tab.Navigate(url, script_to_evaluate_on_commit=test_harness_script)
      tab.action_runner.WaitForJavaScriptCondition(
          finish_js_condition, timeout=30)

      # Stop tracing.
      timeline_data = tab.browser.platform.tracing_controller.StopTracing()[0]

      # Evaluate success.
      timeline_model = model_module.TimelineModel(timeline_data)
      event_iter = timeline_model.IterAllEvents(
          event_type_predicate=timeline_model.IsSliceOrAsyncSlice)
      test_result = _TEST_DONE
      if success_eval_func:
        prefixed_func_name = '_EvaluateSuccess_' + success_eval_func
        test_result = getattr(self, prefixed_func_name)(category, event_iter)
        assert test_result in [_TEST_DONE, _TEST_REPEAT]
      if test_result == _TEST_DONE:
        break
    else:
      self.fail('Test failed all %d tries' % _TEST_MAX_REPEATS)

  @classmethod
  def _CreateExpectations(cls):
    return trace_test_expectations.TraceTestExpectations()

  @classmethod
  def SetUpProcess(cls):
    super(TraceIntegrationTest, cls).SetUpProcess()
    path_util.SetupTelemetryPaths()
    cls.CustomizeBrowserArgs(cls._AddDefaultArgs([]))
    cls.StartBrowser()
    cls.SetStaticServerDirs(data_paths)

  @staticmethod
  def _AddDefaultArgs(browser_args):
    # All tests receive the following options.
    return [
      '--enable-logging',
      '--enable-experimental-web-platform-features'] + browser_args

  def _GetOverlayBotConfigHelper(self):
    system_info = self.browser.GetSystemInfo()
    if not system_info:
      raise Exception("Browser doesn't support GetSystemInfo")
    gpu = system_info.gpu.devices[0]
    if not gpu:
      raise Exception("System Info doesn't have a gpu")
    os_version_name = self.browser.platform.GetOSVersionName()
    return self.GetOverlayBotConfig(
        os_version_name, gpu.vendor_id, gpu.device_id)

  @staticmethod
  def _SwapChainPixelFormatToStr(pixel_format):
    if pixel_format == _SWAP_CHAIN_PIXEL_FORMAT_BGRA:
      return 'BGRA'
    if pixel_format == _SWAP_CHAIN_PIXEL_FORMAT_YUY2:
      return 'YUY2'
    if pixel_format == _SWAP_CHAIN_PIXEL_FORMAT_NV12:
      return 'NV12'
    return str(pixel_format)

  @staticmethod
  def _SwapChainPixelFormatListToStr(pixel_format_list):
    assert len(pixel_format_list) > 0
    list_str = None
    for pixel_format in pixel_format_list:
      format_str = TraceIntegrationTest._SwapChainPixelFormatToStr(pixel_format)
      if list_str is not None:
        list_str = '%s, %s' % (list_str, format_str)
      else:
        list_str = format_str
    return '[%s]' % list_str

  @staticmethod
  def _SwapChainPresentationModeToStr(presentation_mode):
    if presentation_mode == _SWAP_CHAIN_PRESENTATION_MODE_COMPOSED:
      return 'COMPOSED'
    if presentation_mode == _SWAP_CHAIN_PRESENTATION_MODE_OVERLAY:
      return 'OVERLAY'
    if presentation_mode == _SWAP_CHAIN_PRESENTATION_MODE_NONE:
      return 'NONE'
    if presentation_mode == _SWAP_CHAIN_PRESENTATION_MODE_COMPOSITION_FAILURE:
      return 'COMPOSITION_FAILURE'
    return str(presentation_mode)

  @staticmethod
  def _SwapChainPresentationModeListToStr(presentation_mode_list):
    assert len(presentation_mode_list) > 0
    list_str = None
    for mode in presentation_mode_list:
      mode_str = TraceIntegrationTest._SwapChainPresentationModeToStr(mode)
      if list_str is not None:
        list_str = '%s, %s' % (list_str, mode_str)
      else:
        list_str = mode_str
    return '[%s]' % list_str

  @staticmethod
  def _DisabledByDefaultTraceCategory(category):
    return 'disabled-by-default-%s' % category

  #########################################
  # The test success evaluation functions

  def _EvaluateSuccess_CheckGLCategory(self, category, event_iterator):
    for event in event_iterator:
      if (event.category == category and
          event.args.get('gl_category', None) == 'gpu_toplevel'):
        break
    else:
      self.fail('Trace markers for GPU category %s were not found' % category)
    return _TEST_DONE

  def _EvaluateSuccess_CheckVideoModeNoOverlay(self, category, event_iterator):
    return self._CheckVideoModeHelper(category, event_iterator, no_overlay=True)

  def _EvaluateSuccess_CheckVideoMode(self, category, event_iterator):
    return self._CheckVideoModeHelper(category, event_iterator,
                                      no_overlay=False)

  def _CheckVideoModeHelper(self, category, event_iterator, no_overlay):
    os_name = self.browser.platform.GetOSName()
    assert os_name and os_name.lower() == 'win'

    overlay_bot_config = self.GetOverlayBotConfig()
    if overlay_bot_config is None:
      self.fail('Overlay bot config can not be determined')
    assert overlay_bot_config.get('direct_composition', False)

    expected_pixel_format = _SWAP_CHAIN_PIXEL_FORMAT_NV12
    expected_presentation_mode = _SWAP_CHAIN_PRESENTATION_MODE_COMPOSED
    if overlay_bot_config.get('supports_overlays', False):
      supports_yuy2 = False
      supports_nv12 = False
      if overlay_bot_config.get('overlay_cap_yuy2', 'NONE') != 'NONE':
        supports_yuy2 = True
      if overlay_bot_config.get('overlay_cap_nv12', 'NONE') != 'NONE':
        supports_nv12 = True
      assert supports_yuy2 or supports_nv12
      if not no_overlay:
        expected_presentation_mode = _SWAP_CHAIN_PRESENTATION_MODE_OVERLAY
      if not supports_nv12:
        expected_pixel_format = _SWAP_CHAIN_PIXEL_FORMAT_YUY2

    pixel_format_history = []
    presentation_mode_history = []
    previous_time = None
    invalid_info_encountered = False
    for event in event_iterator:
      if event.category != category:
        continue
      if event.name == 'SwapChainFrameInfoInvalid':
        error_code = event.args.get('ErrorCode', None)
        if error_code is None:
          self.fail('ErrorCode is missing from SwapChainFrameInfoInvalid event')
        invalid_info_encountered = True
        logging.info('Swap chain presentation stats collection failed: %s',
                     hex(error_code))
        continue
      if event.name != 'SwapChainFrameInfo':
        continue
      if previous_time is not None:
        # Sanity check that events are chronically sorted
        assert previous_time < event.start
      pixel_format = event.args.get('SwapChain.PixelFormat', None)
      presentation_mode = event.args.get('SwapChain.PresentationMode', None)
      if pixel_format is None or presentation_mode is None:
        self.fail('PixelFormat or PresentationMode is missing from event')
      pixel_format_history.append(pixel_format)
      presentation_mode_history.append(presentation_mode)
      previous_time = event.start

    if len(pixel_format_history) == 0 or len(presentation_mode_history) == 0:
      # In theory 'supports_overlays' needs to be true to trigger a fail, but
      # all DirectComposition test pages run with commandline switch
      # --enable-direct-composition-layers.
      if invalid_info_encountered:
        return _TEST_REPEAT
      self.fail('Trace markers of name SwapChainFrameInfo were not found')

    # The last relevant event is selected.
    if expected_pixel_format != pixel_format_history[-1]:
      self.fail('SwapChain pixel format mismatch, expected %s got %s' %
          (TraceIntegrationTest._SwapChainPixelFormatToStr(
               expected_pixel_format),
           TraceIntegrationTest._SwapChainPixelFormatListToStr(
               pixel_format_history)))
    if expected_presentation_mode != presentation_mode_history[-1]:
      self.fail('SwapChain presentation mode mismatch, expected %s got %s' %
          (TraceIntegrationTest._SwapChainPresentationModeToStr(
               expected_presentation_mode),
           TraceIntegrationTest._SwapChainPresentationModeListToStr(
               presentation_mode_history)))
    return _TEST_DONE


def load_tests(loader, tests, pattern):
  del loader, tests, pattern  # Unused.
  return gpu_integration_test.LoadAllTestsInModule(sys.modules[__name__])
