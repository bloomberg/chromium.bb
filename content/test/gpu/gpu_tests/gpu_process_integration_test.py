# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import sys

from gpu_tests import gpu_integration_test
from gpu_tests import gpu_process_expectations
from gpu_tests import path_util

data_path = os.path.join(
    path_util.GetChromiumSrcDir(), 'content', 'test', 'data')

test_harness_script = r"""
  var domAutomationController = {};
  domAutomationController._finished = false;
  domAutomationController.setAutomationId = function(id) {}
  domAutomationController.send = function(msg) {
    domAutomationController._finished = true;
  }

  window.domAutomationController = domAutomationController;

  function GetDriverBugWorkarounds() {
    var query_result = document.querySelector('.workarounds-list');
    var browser_list = []
    for (var i=0; i < query_result.childElementCount; i++)
      browser_list.push(query_result.children[i].textContent);
    return browser_list;
  };
"""

class GpuProcessIntegrationTest(gpu_integration_test.GpuIntegrationTest):
  # We store a deep copy of the original browser finder options in
  # order to be able to restart the browser multiple times, with a
  # different set of command line arguments each time.
  _original_finder_options = None

  # We keep track of the set of command line arguments used to launch
  # the browser most recently in order to figure out whether we need
  # to relaunch it, if a new pixel test requires a different set of
  # arguments.
  _last_launched_browser_args = set()

  @classmethod
  def Name(cls):
    """The name by which this test is invoked on the command line."""
    return 'gpu_process'

  @classmethod
  def SetUpProcess(cls):
    super(cls, GpuProcessIntegrationTest).SetUpProcess()
    cls._original_finder_options = cls._finder_options.Copy()
    cls.CustomizeBrowserArgs([])
    cls.StartBrowser()
    cls.SetStaticServerDirs([data_path])

  @classmethod
  def CustomizeBrowserArgs(cls, browser_args):
    if not browser_args:
      browser_args = []
    cls._finder_options = cls._original_finder_options.Copy()
    browser_options = cls._finder_options.browser_options
    # All tests receive the following options. They aren't recorded in
    # the _last_launched_browser_args.
    browser_options.AppendExtraBrowserArgs([
      '--enable-gpu-benchmarking',
      # TODO(kbr): figure out why the following option seems to be
      # needed on Android for robustness.
      # https://github.com/catapult-project/catapult/issues/3122
      '--no-first-run'])
    # Append the new arguments.
    browser_options.AppendExtraBrowserArgs(browser_args)
    cls._last_launched_browser_args = set(browser_args)
    cls.SetBrowserOptions(cls._finder_options)

  @classmethod
  def RestartBrowserIfNecessaryWithArgs(cls, browser_args):
    if not browser_args:
      browser_args = []
    if set(browser_args) != cls._last_launched_browser_args:
      logging.info('Restarting browser with arguments: ' + str(browser_args))
      cls.StopBrowser()
      cls.CustomizeBrowserArgs(browser_args)
      cls.StartBrowser()

  @classmethod
  def _CreateExpectations(cls):
    return gpu_process_expectations.GpuProcessExpectations()

  @classmethod
  def GenerateGpuTests(cls, options):
    # The browser test runner synthesizes methods with the exact name
    # given in GenerateGpuTests, so in order to hand-write our tests but
    # also go through the _RunGpuTest trampoline, the test needs to be
    # slightly differently named.

    # Also note that since functional_video.html refers to files in
    # ../media/ , the serving dir must be the common parent directory.
    tests = (('GpuProcess_canvas2d', 'gpu/functional_canvas_demo.html'),
             ('GpuProcess_css3d', 'gpu/functional_3d_css.html'),
             ('GpuProcess_webgl', 'gpu/functional_webgl.html'),
             ('GpuProcess_video', 'gpu/functional_video.html'),
             ('GpuProcess_gpu_info_complete', 'gpu/functional_3d_css.html'),
             ('GpuProcess_no_gpu_process', 'about:blank'),
             ('GpuProcess_driver_bug_workarounds_in_gpu_process', 'chrome:gpu'),
             ('GpuProcess_readback_webgl_gpu_process', 'chrome:gpu'),
             ('GpuProcess_driver_bug_workarounds_upon_gl_renderer',
              'chrome:gpu'),
             ('GpuProcess_only_one_workaround', 'chrome:gpu'),
             ('GpuProcess_skip_gpu_process', 'chrome:gpu'),
             ('GpuProcess_identify_active_gpu1', 'chrome:gpu'),
             ('GpuProcess_identify_active_gpu2', 'chrome:gpu'),
             ('GpuProcess_identify_active_gpu3', 'chrome:gpu'),
             ('GpuProcess_identify_active_gpu4', 'chrome:gpu'),
             ('GpuProcess_disabling_workarounds_works', 'chrome:gpu'))

    # The earlier has_transparent_visuals_gpu_process and
    # no_transparent_visuals_gpu_process tests became no-ops in
    # http://crrev.com/2347383002 and were deleted.

    for t in tests:
      yield (t[0], t[1], ('_' + t[0]))

  def RunActualGpuTest(self, test_path, *args):
    test_name = args[0]
    getattr(self, test_name)(test_path)

  ######################################
  # Helper functions for the tests below

  def _Navigate(self, test_path):
    url = self.UrlOfStaticFilePath(test_path)
    # It's crucial to use the action_runner, rather than the tab's
    # Navigate method directly. It waits for the document ready state
    # to become interactive or better, avoiding critical race
    # conditions.
    self.tab.action_runner.Navigate(
      url, script_to_evaluate_on_commit=test_harness_script)

  def _NavigateAndWait(self, test_path):
    self._Navigate(test_path)
    self.tab.action_runner.WaitForJavaScriptCondition(
      'window.domAutomationController._finished', timeout=10)

  def _VerifyGpuProcessPresent(self):
    tab = self.tab
    if not tab.EvaluateJavaScript('chrome.gpuBenchmarking.hasGpuChannel()'):
      self.fail('No GPU channel detected')

  def _ValidateDriverBugWorkaroundsImpl(self, process_kind, is_expected,
                                    workaround_name):
    tab = self.tab
    if process_kind == "browser_process":
      gpu_driver_bug_workarounds = tab.EvaluateJavaScript(
        'GetDriverBugWorkarounds()')
    elif process_kind == "gpu_process":
      gpu_driver_bug_workarounds = tab.EvaluateJavaScript(
        'chrome.gpuBenchmarking.getGpuDriverBugWorkarounds()')

    is_present = workaround_name in gpu_driver_bug_workarounds
    failure = False
    if is_expected and not is_present:
      failure = True
      error_message = "is missing"
    elif not is_expected and is_present:
      failure = True
      error_message = "is not expected"

    if failure:
      print 'Test failed. Printing page contents:'
      print tab.EvaluateJavaScript('document.body.innerHTML')
      self.fail('%s %s in %s workarounds: %s'
                % (workaround_name, error_message, process_kind,
                   gpu_driver_bug_workarounds))

  def _ValidateDriverBugWorkarounds(self, expected_workaround,
                                    unexpected_workaround):
    if not expected_workaround and not unexpected_workaround:
      return
    if expected_workaround:
      self._ValidateDriverBugWorkaroundsImpl(
        "browser_process", True, expected_workaround)
      self._ValidateDriverBugWorkaroundsImpl(
        "gpu_process", True, expected_workaround)
    if unexpected_workaround:
      self._ValidateDriverBugWorkaroundsImpl(
        "browser_process", False, unexpected_workaround)
      self._ValidateDriverBugWorkaroundsImpl(
        "gpu_process", False, unexpected_workaround)

  # This can only be called from one of the tests, i.e., after the
  # browser's been brought up once.
  def _RunningOnAndroid(self):
    options = self.__class__._original_finder_options.browser_options
    return options.browser_type.startswith('android')

  def _CompareAndCaptureDriverBugWorkarounds(self):
    tab = self.tab
    if not tab.EvaluateJavaScript('chrome.gpuBenchmarking.hasGpuProcess()'):
      self.fail('No GPU process detected')

    if not tab.EvaluateJavaScript('chrome.gpuBenchmarking.hasGpuChannel()'):
      self.fail('No GPU channel detected')

    browser_list = tab.EvaluateJavaScript('GetDriverBugWorkarounds()')
    gpu_list = tab.EvaluateJavaScript(
      'chrome.gpuBenchmarking.getGpuDriverBugWorkarounds()')

    diff = set(browser_list).symmetric_difference(set(gpu_list))
    if len(diff) > 0:
      print 'Test failed. Printing page contents:'
      print tab.EvaluateJavaScript('document.body.innerHTML')
      self.fail('Browser and GPU process list of driver bug'
                'workarounds are not equal: %s != %s, diff: %s' %
                (browser_list, gpu_list, list(diff)))

    basic_infos = tab.EvaluateJavaScript('browserBridge.gpuInfo.basic_info')
    disabled_gl_extensions = None
    for info in basic_infos:
      if info['description'].startswith('Disabled Extensions'):
        disabled_gl_extensions = info['value']
        break

    return gpu_list, disabled_gl_extensions

  def _VerifyActiveAndInactiveGPUs(
      self, expected_active_gpu, expected_inactive_gpus):
    tab = self.tab
    basic_infos = tab.EvaluateJavaScript('browserBridge.gpuInfo.basic_info')
    active_gpu = []
    inactive_gpus = []
    index = 0
    for info in basic_infos:
      description = info['description']
      value = info['value']
      if description.startswith('GPU%d' % index) and value.startswith('VENDOR'):
        if value.endswith('*ACTIVE*'):
          active_gpu.append(value)
        else:
          inactive_gpus.append(value)
        index += 1
    if active_gpu != expected_active_gpu:
      self.fail('Active GPU field is wrong %s' % active_gpu)
    if inactive_gpus != expected_inactive_gpus:
      self.fail('Inactive GPU field is wrong %s' % inactive_gpus)

  ######################################
  # The actual tests

  def _GpuProcess_canvas2d(self, test_path):
    self.RestartBrowserIfNecessaryWithArgs([])
    self._NavigateAndWait(test_path)
    self._VerifyGpuProcessPresent()

  def _GpuProcess_css3d(self, test_path):
    self.RestartBrowserIfNecessaryWithArgs([])
    self._NavigateAndWait(test_path)
    self._VerifyGpuProcessPresent()

  def _GpuProcess_webgl(self, test_path):
    self.RestartBrowserIfNecessaryWithArgs([])
    self._NavigateAndWait(test_path)
    self._VerifyGpuProcessPresent()

  def _GpuProcess_video(self, test_path):
    self.RestartBrowserIfNecessaryWithArgs([])
    self._NavigateAndWait(test_path)
    self._VerifyGpuProcessPresent()

  def _GpuProcess_gpu_info_complete(self, test_path):
    # Regression test for crbug.com/454906
    self.RestartBrowserIfNecessaryWithArgs([])
    self._NavigateAndWait(test_path)
    tab = self.tab
    if not tab.browser.supports_system_info:
      self.fail('Browser must support system info')
    system_info = tab.browser.GetSystemInfo()
    if not system_info.gpu:
      self.fail('Target machine must have a GPU')
    if not system_info.gpu.aux_attributes:
      self.fail('Browser must support GPU aux attributes')
    if not 'gl_renderer' in system_info.gpu.aux_attributes:
      self.fail('Browser must have gl_renderer in aux attribs')
    if len(system_info.gpu.aux_attributes['gl_renderer']) <= 0:
      self.fail('Must have a non-empty gl_renderer string')

  def _GpuProcess_no_gpu_process(self, test_path):
    options = self.__class__._original_finder_options.browser_options
    if options.browser_type.startswith('android'):
      # Android doesn't support starting up the browser without any
      # GPU process. This test is skipped on Android in
      # gpu_process_expectations.py, but we must at least be able to
      # bring up the browser in order to detect that the test
      # shouldn't run. Faking a vendor and device ID can get the
      # browser into a state where it won't launch.
      return
    elif sys.platform in ('cygwin', 'win32'):
      # Hit id 34 from kSoftwareRenderingListJson.
      self.RestartBrowserIfNecessaryWithArgs([
        '--gpu-testing-vendor-id=0x5333',
        '--gpu-testing-device-id=0x8811'])
    elif sys.platform.startswith('linux'):
      # Hit id 50 from kSoftwareRenderingListJson.
      self.RestartBrowserIfNecessaryWithArgs([
        '--gpu-no-complete-info-collection',
        '--gpu-testing-vendor-id=0x10de',
        '--gpu-testing-device-id=0x0de1',
        '--gpu-testing-gl-vendor=VMware',
        '--gpu-testing-gl-renderer=softpipe',
        '--gpu-testing-gl-version="2.1 Mesa 10.1"'])
    elif sys.platform == 'darwin':
      # Hit id 112 from kSoftwareRenderingListJson.
      self.RestartBrowserIfNecessaryWithArgs([
        '--gpu-testing-vendor-id=0x8086',
        '--gpu-testing-device-id=0x0116'])
    self._Navigate(test_path)
    if self.tab.EvaluateJavaScript('chrome.gpuBenchmarking.hasGpuProcess()'):
      self.fail('GPU process detected')

  def _GpuProcess_driver_bug_workarounds_in_gpu_process(self, test_path):
    self.RestartBrowserIfNecessaryWithArgs([
      '--use_gpu_driver_workaround_for_testing'])
    self._Navigate(test_path)
    self._ValidateDriverBugWorkarounds(
      'use_gpu_driver_workaround_for_testing', None)

  def _GpuProcess_readback_webgl_gpu_process(self, test_path):
    # This test was designed to only run on desktop Linux.
    options = self.__class__._original_finder_options.browser_options
    is_platform_android = options.browser_type.startswith('android')
    if sys.platform.startswith('linux') and not is_platform_android:
      # Hit id 110 from kSoftwareRenderingListJson.
      self.RestartBrowserIfNecessaryWithArgs([
        '--gpu-testing-vendor-id=0x10de',
        '--gpu-testing-device-id=0x0de1',
        '--gpu-testing-gl-vendor=VMware',
        '--gpu-testing-gl-renderer=Gallium 0.4 ' \
        'on llvmpipe (LLVM 3.4, 256 bits)',
        '--gpu-testing-gl-version="3.0 Mesa 11.2"'])
      self._Navigate(test_path)
      feature_status_list = self.tab.EvaluateJavaScript(
          'browserBridge.gpuInfo.featureStatus.featureStatus')
      result = True
      for name, status in feature_status_list.items():
        if name == 'multiple_raster_threads':
          result = result and status == 'enabled_on'
        elif name == 'native_gpu_memory_buffers':
          result = result and status == 'disabled_software'
        elif name == 'webgl':
          result = result and status == 'enabled_readback'
        elif name == 'webgl2':
          result = result and status == 'unavailable_off'
        else:
          result = result and status == 'unavailable_software'
      if not result:
        self.fail('WebGL readback setup failed: %s' % feature_status_list)

  def _GpuProcess_driver_bug_workarounds_upon_gl_renderer(self, test_path):
    is_platform_android = self._RunningOnAndroid()
    if is_platform_android:
      # Hit id 108 from kGpuDriverBugListJson.
      self.RestartBrowserIfNecessaryWithArgs([
        '--gpu-testing-gl-vendor=NVIDIA Corporation',
        '--gpu-testing-gl-renderer=NVIDIA Tegra',
        '--gpu-testing-gl-version=OpenGL ES 3.1 NVIDIA 343.00'])
    elif sys.platform in ('cygwin', 'win32'):
      # Hit id 51 and 87 from kGpuDriverBugListJson.
      self.RestartBrowserIfNecessaryWithArgs([
        '--gpu-testing-vendor-id=0x1002',
        '--gpu-testing-device-id=0x6779',
        '--gpu-testing-driver-date=11-20-2014',
        '--gpu-testing-gl-vendor=Google Inc.',
        '--gpu-testing-gl-renderer=ANGLE ' \
        '(AMD Radeon HD 6450 Direct3D11 vs_5_0 ps_5_0)',
        '--gpu-testing-gl-version=OpenGL ES 2.0 (ANGLE 2.1.0.0c0d8006a9dd)'])
    elif sys.platform.startswith('linux'):
      # Hit id 40 from kGpuDriverBugListJson.
      self.RestartBrowserIfNecessaryWithArgs([
        '--gpu-testing-vendor-id=0x0101',
        '--gpu-testing-device-id=0x0102',
        '--gpu-testing-gl-vendor=ARM',
        '--gpu-testing-gl-renderer=Mali-400 MP'])
    elif sys.platform == 'darwin':
      # Currently on osx no workaround relies on gl-renderer.
      return
    self._Navigate(test_path)
    if is_platform_android:
      self._ValidateDriverBugWorkarounds(
        'unpack_overlapping_rows_separately_unpack_buffer', None)
    elif sys.platform in ('cygwin', 'win32'):
      self._ValidateDriverBugWorkarounds(
        'texsubimage_faster_than_teximage', 'disable_d3d11')
    elif sys.platform.startswith('linux'):
      self._ValidateDriverBugWorkarounds('disable_discard_framebuffer', None)
    else:
      self.fail('Unexpected platform ' + sys.platform)

  def _GpuProcess_only_one_workaround(self, test_path):
    # Start this test by launching the browser with no command line
    # arguments.
    self.RestartBrowserIfNecessaryWithArgs([])
    self._Navigate(test_path)
    self._VerifyGpuProcessPresent()
    recorded_workarounds, recorded_disabled_gl_extensions = (
      self._CompareAndCaptureDriverBugWorkarounds())
    # Add the testing workaround to the recorded workarounds.
    recorded_workarounds.append('use_gpu_driver_workaround_for_testing')
    # Relaunch the browser with OS-specific command line arguments.
    browser_args = ['--use_gpu_driver_workaround_for_testing',
                    '--disable-gpu-driver-bug-workarounds']
    # Inject some info to make sure the flags above are effective.
    if sys.platform == 'darwin':
      # Hit id 33 from kGpuDriverBugListJson.
      browser_args.extend(['--gpu-testing-gl-vendor=Imagination'])
    else:
      # Hit id 5 from kGpuDriverBugListJson.
      browser_args.extend(['--gpu-testing-vendor-id=0x10de',
                           '--gpu-testing-device-id=0x0001'])
      # no multi gpu on Android.
      if not self._RunningOnAndroid():
        browser_args.extend(['--gpu-testing-secondary-vendor-ids=',
                             '--gpu-testing-secondary-device-ids='])
    for workaround in recorded_workarounds:
      browser_args.append('--' + workaround)
    browser_args.append('--disable-gl-extensions=' +
                        recorded_disabled_gl_extensions)
    self.RestartBrowserIfNecessaryWithArgs(browser_args)
    self._Navigate(test_path)
    self._VerifyGpuProcessPresent()
    new_workarounds, new_disabled_gl_extensions = (
      self._CompareAndCaptureDriverBugWorkarounds())
    diff = set(recorded_workarounds).symmetric_difference(new_workarounds)
    tab = self.tab
    if len(diff) > 0:
      print 'Test failed. Printing page contents:'
      print tab.EvaluateJavaScript('document.body.innerHTML')
      self.fail(
        'GPU process and expected list of driver bug '
        'workarounds are not equal: %s != %s, diff: %s' %
        (recorded_workarounds, new_workarounds, list(diff)))
    if recorded_disabled_gl_extensions != new_disabled_gl_extensions:
      print 'Test failed. Printing page contents:'
      print tab.EvaluateJavaScript('document.body.innerHTML')
      self.fail(
        'The expected disabled gl extensions are '
        'incorrect: %s != %s:' %
        (recorded_disabled_gl_extensions, new_disabled_gl_extensions))

  def _GpuProcess_skip_gpu_process(self, test_path):
    self.RestartBrowserIfNecessaryWithArgs([
      '--disable-gpu',
      '--skip-gpu-data-loading'])
    self._Navigate(test_path)
    if self.tab.EvaluateJavaScript('chrome.gpuBenchmarking.hasGpuProcess()'):
      self.fail('GPU process detected')

  def _GpuProcess_identify_active_gpu1(self, test_path):
    self.RestartBrowserIfNecessaryWithArgs([
      '--gpu-testing-vendor-id=0x8086',
      '--gpu-testing-device-id=0x040a',
      '--gpu-testing-secondary-vendor-ids=0x10de',
      '--gpu-testing-secondary-device-ids=0x0de1',
      '--gpu-testing-gl-vendor=nouveau'])
    self._Navigate(test_path)
    self._VerifyActiveAndInactiveGPUs(
      ['VENDOR = 0x10de, DEVICE= 0x0de1 *ACTIVE*'],
      ['VENDOR = 0x8086, DEVICE= 0x040a'])

  def _GpuProcess_identify_active_gpu2(self, test_path):
    self.RestartBrowserIfNecessaryWithArgs([
      '--gpu-testing-vendor-id=0x8086',
      '--gpu-testing-device-id=0x040a',
      '--gpu-testing-secondary-vendor-ids=0x10de',
      '--gpu-testing-secondary-device-ids=0x0de1',
      '--gpu-testing-gl-vendor=Intel'])
    self._Navigate(test_path)
    self._VerifyActiveAndInactiveGPUs(
      ['VENDOR = 0x8086, DEVICE= 0x040a *ACTIVE*'],
      ['VENDOR = 0x10de, DEVICE= 0x0de1'])

  def _GpuProcess_identify_active_gpu3(self, test_path):
    self.RestartBrowserIfNecessaryWithArgs([
      '--gpu-testing-vendor-id=0x8086',
      '--gpu-testing-device-id=0x040a',
      '--gpu-testing-secondary-vendor-ids=0x10de;0x1002',
      '--gpu-testing-secondary-device-ids=0x0de1;0x6779',
      '--gpu-testing-gl-vendor=X.Org',
      '--gpu-testing-gl-renderer=AMD R600'])
    self._Navigate(test_path)
    self._VerifyActiveAndInactiveGPUs(
      ['VENDOR = 0x1002, DEVICE= 0x6779 *ACTIVE*'],
      ['VENDOR = 0x8086, DEVICE= 0x040a',
       'VENDOR = 0x10de, DEVICE= 0x0de1'])

  def _GpuProcess_identify_active_gpu4(self, test_path):
    self.RestartBrowserIfNecessaryWithArgs([
      '--gpu-testing-vendor-id=0x10de',
      '--gpu-testing-device-id=0x0de1',
      '--gpu-testing-secondary-vendor-ids=',
      '--gpu-testing-secondary-device-ids=',
      '--gpu-testing-gl-vendor=nouveau'])
    self._Navigate(test_path)
    self._VerifyActiveAndInactiveGPUs(
      ['VENDOR = 0x10de, DEVICE= 0x0de1 *ACTIVE*'],
      [])

  def _GpuProcess_disabling_workarounds_works(self, test_path):
    # Hit exception from id 215 from kGpuDriverBugListJson.
    self.RestartBrowserIfNecessaryWithArgs([
      '--gpu-testing-vendor-id=0xbad9',
      '--gpu-testing-device-id=0xbad9',
      '--gpu-testing-secondary-vendor-ids=',
      '--gpu-testing-secondary-device-ids=',
      '--gpu-testing-gl-vendor=FakeVendor',
      '--gpu-testing-gl-renderer=FakeRenderer',
      '--use_gpu_driver_workaround_for_testing=0'])
    self._Navigate(test_path)
    workarounds, _ = (
      self._CompareAndCaptureDriverBugWorkarounds())
    if 'use_gpu_driver_workaround_for_testing' in workarounds:
      self.fail('use_gpu_driver_workaround_for_testing erroneously present')

def load_tests(loader, tests, pattern):
  del loader, tests, pattern  # Unused.
  return gpu_integration_test.LoadAllTestsInModule(sys.modules[__name__])
