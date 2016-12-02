# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import sys
from telemetry.story import story_set as story_set_module
from telemetry.page import legacy_page_test

from gpu_tests import gpu_test_base

class GpuProcessSharedPageState(gpu_test_base.GpuSharedPageState):

  gpu_switches = ['--gpu-no-complete-info-collection',
                  '--gpu-testing-os-version',
                  '--gpu-testing-vendor-id',
                  '--gpu-testing-device-id',
                  '--gpu-testing-secondary-vendor-ids',
                  '--gpu-testing-secondary-device-ids',
                  '--gpu-testing-driver-date',
                  '--gpu-testing-gl-vendor',
                  '--gpu-testing-gl-renderer',
                  '--gpu-testing-gl-version']

  def __init__(self, test, finder_options, story_set):
    super(GpuProcessSharedPageState, self).__init__(
      test, finder_options, story_set)
    options = finder_options.browser_options.extra_browser_args

    # Clear all existing gpu testing switches.
    old_gpu_switches = []
    for opt in options:
      for gpu_switch in self.gpu_switches:
        if opt.startswith(gpu_switch):
          old_gpu_switches.append(opt)
    options.difference_update(old_gpu_switches)


class IdentifyActiveGpuPageBase(gpu_test_base.PageBase):

  def __init__(self, name=None, page_set=None, shared_page_state_class=None,
               expectations=None, active_gpu=None, inactive_gpus=None):
    super(IdentifyActiveGpuPageBase, self).__init__(
      url='chrome:gpu',
      name=name,
      page_set=page_set,
      shared_page_state_class=shared_page_state_class,
      expectations=expectations)
    self.active_gpu = active_gpu
    self.inactive_gpus = inactive_gpus

  def Validate(self, tab, results):
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

    if active_gpu != self.active_gpu:
      raise legacy_page_test.Failure(
          'Active GPU field is wrong %s' % active_gpu)

    if inactive_gpus != self.inactive_gpus:
      raise legacy_page_test.Failure(
          'Inactive GPU field is wrong %s' % inactive_gpus)


class DriverBugWorkaroundsTestsPage(gpu_test_base.PageBase):
  def __init__(self, page_set=None, name='',
               shared_page_state_class=None,
               expectations=None,
               expected_workaround=None,
               unexpected_workaround=None):
    super(DriverBugWorkaroundsTestsPage, self).__init__(
        url='chrome:gpu',
        page_set=page_set,
        name=name,
        shared_page_state_class=shared_page_state_class,
        expectations=expectations)
    self.expected_workaround = expected_workaround
    self.unexpected_workaround = unexpected_workaround

  def _Validate(self, tab, process_kind, is_expected, workaround_name):
    if process_kind == "browser_process":
      gpu_driver_bug_workarounds = tab.EvaluateJavaScript( \
        'GetDriverBugWorkarounds()')
    elif process_kind == "gpu_process":
      gpu_driver_bug_workarounds = tab.EvaluateJavaScript( \
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
      raise legacy_page_test.Failure('%s %s in Browser process workarounds: %s'
        % (workaround_name, error_message, gpu_driver_bug_workarounds))

  def Validate(self, tab, results):
    if not self.expected_workaround and not self.unexpected_workaround:
      return

    if self.expected_workaround:
      self._Validate(tab, "browser_process", True, self.expected_workaround)
      self._Validate(tab, "gpu_process", True, self.expected_workaround)

    if self.unexpected_workaround:
      self._Validate(tab, "browser_process", False, self.unexpected_workaround)
      self._Validate(tab, "gpu_process", False, self.unexpected_workaround)


class EqualBugWorkaroundsBasePage(gpu_test_base.PageBase):
  def __init__(self, name=None, page_set=None, shared_page_state_class=None,
               expectations=None):
    super(EqualBugWorkaroundsBasePage, self).__init__(
      url='chrome:gpu',
      name=name,
      page_set=page_set,
      shared_page_state_class=shared_page_state_class,
      expectations=expectations)

  def Validate(self, tab, results):
    has_gpu_process_js = 'chrome.gpuBenchmarking.hasGpuProcess()'
    if not tab.EvaluateJavaScript(has_gpu_process_js):
      raise legacy_page_test.Failure('No GPU process detected')

    has_gpu_channel_js = 'chrome.gpuBenchmarking.hasGpuChannel()'
    if not tab.EvaluateJavaScript(has_gpu_channel_js):
      raise legacy_page_test.Failure('No GPU channel detected')

    browser_list = tab.EvaluateJavaScript('GetDriverBugWorkarounds()')
    gpu_list = tab.EvaluateJavaScript( \
      'chrome.gpuBenchmarking.getGpuDriverBugWorkarounds()')

    diff = set(browser_list).symmetric_difference(set(gpu_list))
    if len(diff) > 0:
      print 'Test failed. Printing page contents:'
      print tab.EvaluateJavaScript('document.body.innerHTML')
      raise legacy_page_test.Failure(
        'Browser and GPU process list of driver bug'
        'workarounds are not equal: %s != %s, diff: %s' %
        (browser_list, gpu_list, list(diff)))

    basic_infos = tab.EvaluateJavaScript('browserBridge.gpuInfo.basic_info')
    disabled_gl_extensions = None
    for info in basic_infos:
      if info['description'].startswith('Disabled Extensions'):
        disabled_gl_extensions = info['value']
        break

    return gpu_list, disabled_gl_extensions


class GpuProcessTestsPage(gpu_test_base.PageBase):
  def __init__(self, url, name, story_set, expectations):
    super(GpuProcessTestsPage, self).__init__(url=url,
        shared_page_state_class=gpu_test_base.GpuSharedPageState,
        page_set=story_set,
        name=name,
        expectations=expectations)


class FunctionalVideoPage(GpuProcessTestsPage):

  def __init__(self, story_set, expectations):
    super(FunctionalVideoPage, self).__init__(
      url='file://../../data/gpu/functional_video.html',
      name='GpuProcess.video',
      story_set=story_set,
      expectations=expectations)

  def RunNavigateSteps(self, action_runner):
    super(FunctionalVideoPage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'domAutomationController._finished', timeout_in_seconds=30)


class GpuInfoCompletePage(GpuProcessTestsPage):

  def __init__(self, story_set, expectations):
    super(GpuInfoCompletePage, self).__init__(
      url='file://../../data/gpu/functional_3d_css.html',
      name='GpuProcess.gpu_info_complete',
      story_set=story_set,
      expectations=expectations)

  def Validate(self, tab, results):
    # Regression test for crbug.com/454906
    if not tab.browser.supports_system_info:
      raise legacy_page_test.Failure('Browser must support system info')
    system_info = tab.browser.GetSystemInfo()
    if not system_info.gpu:
      raise legacy_page_test.Failure('Target machine must have a GPU')
    if not system_info.gpu.aux_attributes:
      raise legacy_page_test.Failure('Browser must support GPU aux attributes')
    if not 'gl_renderer' in system_info.gpu.aux_attributes:
      raise legacy_page_test.Failure(
          'Browser must have gl_renderer in aux attribs')
    if len(system_info.gpu.aux_attributes['gl_renderer']) <= 0:
      raise legacy_page_test.Failure(
          'Must have a non-empty gl_renderer string')


class NoGpuProcessSharedPageState(GpuProcessSharedPageState):
  def __init__(self, test, finder_options, story_set):
    super(NoGpuProcessSharedPageState, self).__init__(
      test, finder_options, story_set)
    options = finder_options.browser_options

    if options.browser_type.startswith('android'):
      # Android doesn't support starting up the browser without any
      # GPU process. This test is skipped on Android in
      # gpu_process_expectations.py, but we must at least be able to
      # bring up the browser in order to detect that the test
      # shouldn't run. Faking a vendor and device ID can get the
      # browser into a state where it won't launch.
      pass
    elif sys.platform in ('cygwin', 'win32'):
      # Hit id 34 from kSoftwareRenderingListJson.
      options.AppendExtraBrowserArgs('--gpu-testing-vendor-id=0x5333')
      options.AppendExtraBrowserArgs('--gpu-testing-device-id=0x8811')
    elif sys.platform.startswith('linux'):
      # Hit id 50 from kSoftwareRenderingListJson.
      options.AppendExtraBrowserArgs('--gpu-no-complete-info-collection')
      options.AppendExtraBrowserArgs('--gpu-testing-vendor-id=0x10de')
      options.AppendExtraBrowserArgs('--gpu-testing-device-id=0x0de1')
      options.AppendExtraBrowserArgs('--gpu-testing-gl-vendor=VMware')
      options.AppendExtraBrowserArgs('--gpu-testing-gl-renderer=softpipe')
      options.AppendExtraBrowserArgs('--gpu-testing-gl-version="2.1 Mesa 10.1"')
    elif sys.platform == 'darwin':
      # Hit id 81 from kSoftwareRenderingListJson.
      options.AppendExtraBrowserArgs('--gpu-testing-os-version=10.7')
      options.AppendExtraBrowserArgs('--gpu-testing-vendor-id=0x15ad')
      options.AppendExtraBrowserArgs('--gpu-testing-device-id=0x0393')


class NoGpuProcessPage(gpu_test_base.PageBase):

  def __init__(self, story_set, expectations):
    super(NoGpuProcessPage, self).__init__(
      url='about:blank',
      name='GpuProcess.no_gpu_process',
      page_set=story_set,
      shared_page_state_class=NoGpuProcessSharedPageState,
      expectations=expectations)

  def Validate(self, tab, results):
    has_gpu_process_js = 'chrome.gpuBenchmarking.hasGpuProcess()'
    has_gpu_process = tab.EvaluateJavaScript(has_gpu_process_js)
    if has_gpu_process:
      raise legacy_page_test.Failure('GPU process detected')


class SoftwareGpuProcessSharedPageState(GpuProcessSharedPageState):
  def __init__(self, test, finder_options, story_set):
    super(SoftwareGpuProcessSharedPageState, self).__init__(
      test, finder_options, story_set)
    options = finder_options.browser_options

    # Hit exception from id 50 from kSoftwareRenderingListJson.
    options.AppendExtraBrowserArgs('--gpu-testing-vendor-id=0x10de')
    options.AppendExtraBrowserArgs('--gpu-testing-device-id=0x0de1')
    options.AppendExtraBrowserArgs('--gpu-testing-gl-vendor=VMware')
    options.AppendExtraBrowserArgs('--gpu-testing-gl-renderer=SVGA3D')
    options.AppendExtraBrowserArgs('--gpu-testing-gl-version=2.1 Mesa 10.1')


class SoftwareGpuProcessPage(gpu_test_base.PageBase):

  def __init__(self, story_set, expectations):
    super(SoftwareGpuProcessPage, self).__init__(
      url='about:blank',
      name='GpuProcess.software_gpu_process',
      page_set=story_set,
      shared_page_state_class=SoftwareGpuProcessSharedPageState,
      expectations=expectations)


class SkipGpuProcessSharedPageState(GpuProcessSharedPageState):
  def __init__(self, test, finder_options, story_set):
    super(SkipGpuProcessSharedPageState, self).__init__(
      test, finder_options, story_set)
    options = finder_options.browser_options

    options.AppendExtraBrowserArgs('--disable-gpu')
    options.AppendExtraBrowserArgs('--skip-gpu-data-loading')


class SkipGpuProcessPage(gpu_test_base.PageBase):

  def __init__(self, story_set, expectations):
    super(SkipGpuProcessPage, self).__init__(
      url='chrome:gpu',
      name='GpuProcess.skip_gpu_process',
      page_set=story_set,
      shared_page_state_class=SkipGpuProcessSharedPageState,
      expectations=expectations)

  def Validate(self, tab, results):
    has_gpu_process_js = 'chrome.gpuBenchmarking.hasGpuProcess()'
    has_gpu_process = tab.EvaluateJavaScript(has_gpu_process_js)
    if has_gpu_process:
      raise legacy_page_test.Failure('GPU process detected')


class DriverBugWorkaroundsShared(GpuProcessSharedPageState):
  def __init__(self, test, finder_options, story_set):
    super(DriverBugWorkaroundsShared, self).__init__(
      test, finder_options, story_set)
    options = finder_options.browser_options
    options.AppendExtraBrowserArgs('--use_gpu_driver_workaround_for_testing')


class DriverBugWorkaroundsInGpuProcessPage(DriverBugWorkaroundsTestsPage):
  def __init__(self, story_set, expectations):
    super(DriverBugWorkaroundsInGpuProcessPage, self).__init__(
      name='GpuProcess.driver_bug_workarounds_in_gpu_process',
      page_set=story_set,
      shared_page_state_class=DriverBugWorkaroundsShared,
      expectations=expectations,
      expected_workaround='use_gpu_driver_workaround_for_testing')

  def Validate(self, tab, results):
    super(DriverBugWorkaroundsInGpuProcessPage, self).Validate(tab, results)


class DriverBugWorkaroundsUponGLRendererShared(GpuProcessSharedPageState):
  def __init__(self, test, finder_options, story_set):
    super(DriverBugWorkaroundsUponGLRendererShared, self).__init__(
      test, finder_options, story_set)
    options = finder_options.browser_options
    if options.browser_type.startswith('android'):
      # Hit id 108 from kGpuDriverBugListJson.
      options.AppendExtraBrowserArgs('--gpu-testing-gl-vendor=' \
                                     'NVIDIA Corporation')
      options.AppendExtraBrowserArgs('--gpu-testing-gl-renderer=NVIDIA Tegra')
      options.AppendExtraBrowserArgs('--gpu-testing-gl-version=' \
                                     'OpenGL ES 3.1 NVIDIA 343.00')
    elif sys.platform in ('cygwin', 'win32'):
      # Hit id 51 and 87 from kGpuDriverBugListJson.
      options.AppendExtraBrowserArgs('--gpu-testing-vendor-id=0x1002')
      options.AppendExtraBrowserArgs('--gpu-testing-device-id=0x6779')
      options.AppendExtraBrowserArgs('--gpu-testing-driver-date=11-20-2014')
      options.AppendExtraBrowserArgs('--gpu-testing-gl-vendor=Google Inc.')
      options.AppendExtraBrowserArgs('--gpu-testing-gl-renderer=ANGLE ' \
        '(AMD Radeon HD 6450 Direct3D11 vs_5_0 ps_5_0)')
      options.AppendExtraBrowserArgs('--gpu-testing-gl-version=OpenGL ES 2.0 ' \
        '(ANGLE 2.1.0.0c0d8006a9dd)')
    elif sys.platform.startswith('linux'):
      # Hit id 40 from kGpuDriverBugListJson.
      options.AppendExtraBrowserArgs('--gpu-testing-vendor-id=0x0101')
      options.AppendExtraBrowserArgs('--gpu-testing-device-id=0x0102')
      options.AppendExtraBrowserArgs('--gpu-testing-gl-vendor=ARM')
      options.AppendExtraBrowserArgs('--gpu-testing-gl-renderer=Mali-400 MP')
    elif sys.platform == 'darwin':
      # Currently on osx no workaround relies on gl-renderer.
      pass


class DriverBugWorkaroundsUponGLRendererPage(DriverBugWorkaroundsTestsPage):
  def __init__(self, story_set, expectations, is_platform_android):
    self.expected_workaround = None
    self.unexpected_workaround = None

    if is_platform_android:
      self.expected_workaround = \
          "unpack_overlapping_rows_separately_unpack_buffer"
    elif sys.platform in ('cygwin', 'win32'):
      self.expected_workaround = "texsubimage_faster_than_teximage"
      self.unexpected_workaround = "disable_d3d11"
    elif sys.platform.startswith('linux'):
      self.expected_workaround = "disable_discard_framebuffer"
    elif sys.platform == 'darwin':
      pass
    super(DriverBugWorkaroundsUponGLRendererPage, self).__init__(
      name='GpuProcess.driver_bug_workarounds_upon_gl_renderer',
      page_set=story_set,
      shared_page_state_class=DriverBugWorkaroundsUponGLRendererShared,
      expectations=expectations,
      expected_workaround=self.expected_workaround,
      unexpected_workaround=self.unexpected_workaround)

  def Validate(self, tab, results):
    super(DriverBugWorkaroundsUponGLRendererPage, self).Validate(tab, results)


class IdentifyActiveGpuSharedPageState1(GpuProcessSharedPageState):
  def __init__(self, test, finder_options, story_set):
    super(IdentifyActiveGpuSharedPageState1, self).__init__(
      test, finder_options, story_set)
    opts = finder_options.browser_options

    opts.AppendExtraBrowserArgs('--gpu-testing-vendor-id=0x8086')
    opts.AppendExtraBrowserArgs('--gpu-testing-device-id=0x040a')
    opts.AppendExtraBrowserArgs('--gpu-testing-secondary-vendor-ids=0x10de')
    opts.AppendExtraBrowserArgs('--gpu-testing-secondary-device-ids=0x0de1')
    opts.AppendExtraBrowserArgs('--gpu-testing-gl-vendor=nouveau')


class IdentifyActiveGpuPage1(IdentifyActiveGpuPageBase):
  def __init__(self, story_set, expectations):
    active_gpu = ['VENDOR = 0x10de, DEVICE= 0x0de1 *ACTIVE*']
    inactive_gpus = ['VENDOR = 0x8086, DEVICE= 0x040a']

    super(IdentifyActiveGpuPage1, self).__init__(
      name='GpuProcess.identify_active_gpu1',
      page_set=story_set,
      shared_page_state_class=IdentifyActiveGpuSharedPageState1,
      expectations=expectations,
      active_gpu=active_gpu,
      inactive_gpus=inactive_gpus)

  def Validate(self, tab, results):
    super(IdentifyActiveGpuPage1, self).Validate(tab, results)


class IdentifyActiveGpuSharedPageState2(GpuProcessSharedPageState):
  def __init__(self, test, finder_options, story_set):
    super(IdentifyActiveGpuSharedPageState2, self).__init__(
      test, finder_options, story_set)
    opts = finder_options.browser_options

    opts.AppendExtraBrowserArgs('--gpu-testing-vendor-id=0x8086')
    opts.AppendExtraBrowserArgs('--gpu-testing-device-id=0x040a')
    opts.AppendExtraBrowserArgs('--gpu-testing-secondary-vendor-ids=0x10de')
    opts.AppendExtraBrowserArgs('--gpu-testing-secondary-device-ids=0x0de1')
    opts.AppendExtraBrowserArgs('--gpu-testing-gl-vendor=Intel')


class IdentifyActiveGpuPage2(IdentifyActiveGpuPageBase):
  def __init__(self, story_set, expectations):
    active_gpu = ['VENDOR = 0x8086, DEVICE= 0x040a *ACTIVE*']
    inactive_gpus = ['VENDOR = 0x10de, DEVICE= 0x0de1']

    super(IdentifyActiveGpuPage2, self).__init__(
      name='GpuProcess.identify_active_gpu2',
      page_set=story_set,
      shared_page_state_class=IdentifyActiveGpuSharedPageState2,
      expectations=expectations,
      active_gpu=active_gpu,
      inactive_gpus=inactive_gpus)

  def Validate(self, tab, results):
    super(IdentifyActiveGpuPage2, self).Validate(tab, results)


class IdentifyActiveGpuSharedPageState3(GpuProcessSharedPageState):
  def __init__(self, test, finder_options, story_set):
    super(IdentifyActiveGpuSharedPageState3, self).__init__(
      test, finder_options, story_set)
    opts = finder_options.browser_options

    opts.AppendExtraBrowserArgs('--gpu-testing-vendor-id=0x8086')
    opts.AppendExtraBrowserArgs('--gpu-testing-device-id=0x040a')
    opts.AppendExtraBrowserArgs('--gpu-testing-secondary-vendor-ids= \
                                0x10de;0x1002')
    opts.AppendExtraBrowserArgs('--gpu-testing-secondary-device-ids= \
                                0x0de1;0x6779')
    opts.AppendExtraBrowserArgs('--gpu-testing-gl-vendor=X.Org')
    opts.AppendExtraBrowserArgs('--gpu-testing-gl-renderer=AMD R600')


class IdentifyActiveGpuPage3(IdentifyActiveGpuPageBase):
  def __init__(self, story_set, expectations):
    active_gpu = ['VENDOR = 0x1002, DEVICE= 0x6779 *ACTIVE*']
    inactive_gpus = ['VENDOR = 0x8086, DEVICE= 0x040a', \
                     'VENDOR = 0x10de, DEVICE= 0x0de1']

    super(IdentifyActiveGpuPage3, self).__init__(
      name='GpuProcess.identify_active_gpu3',
      page_set=story_set,
      shared_page_state_class=IdentifyActiveGpuSharedPageState3,
      expectations=expectations,
      active_gpu=active_gpu,
      inactive_gpus=inactive_gpus)

  def Validate(self, tab, results):
    super(IdentifyActiveGpuPage3, self).Validate(tab, results)


class IdentifyActiveGpuSharedPageState4(GpuProcessSharedPageState):
  def __init__(self, test, finder_options, story_set):
    super(IdentifyActiveGpuSharedPageState4, self).__init__(
      test, finder_options, story_set)
    opts = finder_options.browser_options

    opts.AppendExtraBrowserArgs('--gpu-testing-vendor-id=0x10de')
    opts.AppendExtraBrowserArgs('--gpu-testing-device-id=0x0de1')
    opts.AppendExtraBrowserArgs('--gpu-testing-secondary-vendor-ids=')
    opts.AppendExtraBrowserArgs('--gpu-testing-secondary-device-ids=')
    opts.AppendExtraBrowserArgs('--gpu-testing-gl-vendor=nouveau')


class IdentifyActiveGpuPage4(IdentifyActiveGpuPageBase):
  def __init__(self, story_set, expectations):
    active_gpu = ['VENDOR = 0x10de, DEVICE= 0x0de1 *ACTIVE*']
    inactive_gpus = []

    super(IdentifyActiveGpuPage4, self).__init__(
      name='GpuProcess.identify_active_gpu4',
      page_set=story_set,
      shared_page_state_class=IdentifyActiveGpuSharedPageState4,
      expectations=expectations,
      active_gpu=active_gpu,
      inactive_gpus=inactive_gpus)

  def Validate(self, tab, results):
    super(IdentifyActiveGpuPage4, self).Validate(tab, results)


class ReadbackWebGLGpuProcessSharedPageState(GpuProcessSharedPageState):
  def __init__(self, test, finder_options, story_set):
    super(ReadbackWebGLGpuProcessSharedPageState, self).__init__(
      test, finder_options, story_set)
    options = finder_options.browser_options
    is_platform_android = options.browser_type.startswith('android')

    if sys.platform.startswith('linux') and not is_platform_android:
      # Hit id 110 from kSoftwareRenderingListJson.
      options.AppendExtraBrowserArgs('--gpu-testing-vendor-id=0x10de')
      options.AppendExtraBrowserArgs('--gpu-testing-device-id=0x0de1')
      options.AppendExtraBrowserArgs('--gpu-testing-gl-vendor=VMware')
      options.AppendExtraBrowserArgs('--gpu-testing-gl-renderer=Gallium 0.4 ' \
        'on llvmpipe (LLVM 3.4, 256 bits)')
      options.AppendExtraBrowserArgs('--gpu-testing-gl-version="3.0 Mesa 11.2"')

class ReadbackWebGLGpuProcessPage(gpu_test_base.PageBase):
  def __init__(self, story_set, expectations, is_platform_android):
    super(ReadbackWebGLGpuProcessPage, self).__init__(
      url='chrome:gpu',
      name='GpuProcess.readback_webgl_gpu_process',
      page_set=story_set,
      shared_page_state_class=ReadbackWebGLGpuProcessSharedPageState,
      expectations=expectations)
    self.is_platform_android = is_platform_android

  def Validate(self, tab, results):
    if sys.platform.startswith('linux') and not self.is_platform_android:
      feature_status_js = 'browserBridge.gpuInfo.featureStatus.featureStatus'
      feature_status_list = tab.EvaluateJavaScript(feature_status_js)
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
        raise legacy_page_test.Failure('WebGL readback setup failed: %s' \
          % feature_status_list)


class HasTransparentVisualsShared(GpuProcessSharedPageState):
  def __init__(self, test, finder_options, story_set):
    super(HasTransparentVisualsShared, self).__init__(
      test, finder_options, story_set)
    options = finder_options.browser_options
    if sys.platform.startswith('linux'):
      # Hit id 173 from kGpuDriverBugListJson.
      options.AppendExtraBrowserArgs('--gpu-testing-gl-version=3.0 Mesa ' \
                                     '12.1')

class HasTransparentVisualsGpuProcessPage(DriverBugWorkaroundsTestsPage):
  def __init__(self, story_set, expectations):
    super(HasTransparentVisualsGpuProcessPage, self).__init__(
      name='GpuProcess.has_transparent_visuals_gpu_process',
      page_set=story_set,
      shared_page_state_class=HasTransparentVisualsShared,
      expectations=expectations,
      expected_workaround=None,
      unexpected_workaround=None)

  def Validate(self, tab, results):
    if sys.platform.startswith('linux'):
      super(HasTransparentVisualsGpuProcessPage, self).Validate(tab, results)


class NoTransparentVisualsShared(GpuProcessSharedPageState):
  def __init__(self, test, finder_options, story_set):
    super(NoTransparentVisualsShared, self).__init__(
      test, finder_options, story_set)

class NoTransparentVisualsGpuProcessPage(DriverBugWorkaroundsTestsPage):
  def __init__(self, story_set, expectations):
    super(NoTransparentVisualsGpuProcessPage, self).__init__(
      name='GpuProcess.no_transparent_visuals_gpu_process',
      page_set=story_set,
      shared_page_state_class=NoTransparentVisualsShared,
      expectations=expectations,
      expected_workaround=None,
      unexpected_workaround=None)

  def Validate(self, tab, results):
    if sys.platform.startswith('linux'):
      super(NoTransparentVisualsGpuProcessPage, self).Validate(tab, results)


class TransferWorkaroundSharedPageState(GpuProcessSharedPageState):
  def __init__(self, test, finder_options, story_set):
    super(TransferWorkaroundSharedPageState, self).__init__(
      test, finder_options, story_set)

  # Extra browser args need to be added here. Adding them in RunStory would be
  # too late.
  def WillRunStory(self, results):
    if self.WarmUpDone():
      options = self._finder_options.browser_options
      options.AppendExtraBrowserArgs('--use_gpu_driver_workaround_for_testing')
      options.AppendExtraBrowserArgs('--disable-gpu-driver-bug-workarounds')

      # Inject some info to make sure the flag above is effective.
      if sys.platform == 'darwin':
        # Hit id 33 from kGpuDriverBugListJson.
        options.AppendExtraBrowserArgs('--gpu-testing-gl-vendor=Imagination')
      else:
        # Hit id 5 from kGpuDriverBugListJson.
        options.AppendExtraBrowserArgs('--gpu-testing-vendor-id=0x10de')
        options.AppendExtraBrowserArgs('--gpu-testing-device-id=0x0001')
        # no multi gpu on Android.
        if not options.browser_type.startswith('android'):
          options.AppendExtraBrowserArgs('--gpu-testing-secondary-vendor-ids=')
          options.AppendExtraBrowserArgs('--gpu-testing-secondary-device-ids=')

      for workaround in self.RecordedWorkarounds():
        options.AppendExtraBrowserArgs('--' + workaround)
      options.AppendExtraBrowserArgs('--disable-gl-extensions=' + \
                                     self.DisabledGLExts())
    super(TransferWorkaroundSharedPageState, self).WillRunStory(results)

  # self._current_page is None in WillRunStory so do the transfer here.
  def RunStory(self, results):
    if self.WarmUpDone():
      self._current_page.expected_workarounds = self.RecordedWorkarounds()
      self._current_page.expected_disabled_exts = self.DisabledGLExts()
      self.AddTestingWorkaround(self._current_page.expected_workarounds)
    super(TransferWorkaroundSharedPageState, self).RunStory(results)

  def WarmUpDone(self):
    return self._previous_page is not None and \
           self.RecordedWorkarounds() is not None

  def RecordedWorkarounds(self):
    return self._previous_page.recorded_workarounds

  def DisabledGLExts(self):
    return self._previous_page.recorded_disabled_exts

  def AddTestingWorkaround(self, workarounds):
    if workarounds is not None:
      workarounds.append('use_gpu_driver_workaround_for_testing')


class EqualBugWorkaroundsPage(EqualBugWorkaroundsBasePage):
  def __init__(self, story_set, expectations):
    super(EqualBugWorkaroundsPage, self).__init__(
      name='GpuProcess.equal_bug_workarounds_in_browser_and_gpu_process',
      page_set=story_set,
      shared_page_state_class=TransferWorkaroundSharedPageState,
      expectations=expectations)
    self.recorded_workarounds = None
    self.recorded_disabled_exts = None

  def Validate(self, tab, results):
    recorded_info = super(EqualBugWorkaroundsPage, self).Validate(tab, results)
    gpu_list, disabled_gl_extensions = recorded_info

    self.recorded_workarounds = gpu_list
    self.recorded_disabled_exts = disabled_gl_extensions


class OnlyOneWorkaroundPage(EqualBugWorkaroundsBasePage):
  def __init__(self, story_set, expectations):
    super(OnlyOneWorkaroundPage, self).__init__(
      name='GpuProcess.only_one_workaround',
      page_set=story_set,
      shared_page_state_class=TransferWorkaroundSharedPageState,
      expectations=expectations)
    self.expected_workarounds = None
    self.expected_disabled_exts = None

  def Validate(self, tab, results):
    # Requires EqualBugWorkaroundsPage to succeed. If it has failed then just
    # pass to not overload the logs.
    if self.expected_workarounds is None:
      return

    recorded_info = super(OnlyOneWorkaroundPage, self).Validate(tab, results)
    gpu_list, disabled_gl_extensions = recorded_info

    diff = set(self.expected_workarounds).symmetric_difference(set(gpu_list))
    if len(diff) > 0:
      print 'Test failed. Printing page contents:'
      print tab.EvaluateJavaScript('document.body.innerHTML')
      raise legacy_page_test.Failure(
        'GPU process and expected list of driver bug'
        'workarounds are not equal: %s != %s, diff: %s' %
        (self.expected_workarounds, gpu_list, list(diff)))

    if self.expected_disabled_exts != disabled_gl_extensions:
      print 'Test failed. Printing page contents:'
      print tab.EvaluateJavaScript('document.body.innerHTML')
      raise legacy_page_test.Failure(
        'The expected disabled gl extensions are '
        'incorrect: %s != %s:' %
        (self.expected_disabled_exts, disabled_gl_extensions))


class GpuProcessTestsStorySet(story_set_module.StorySet):

  """ Tests that accelerated content triggers the creation of a GPU process """

  def __init__(self, expectations, is_platform_android):
    super(GpuProcessTestsStorySet, self).__init__(
      serving_dirs=set(['../../../../content/test/data']))

    urls_and_names_list = [
      ('file://../../data/gpu/functional_canvas_demo.html',
       'GpuProcess.canvas2d'),
      ('file://../../data/gpu/functional_3d_css.html',
       'GpuProcess.css3d'),
      ('file://../../data/gpu/functional_webgl.html',
       'GpuProcess.webgl')
    ]

    for url, name in urls_and_names_list:
      self.AddStory(GpuProcessTestsPage(url, name, self, expectations))

    self.AddStory(FunctionalVideoPage(self, expectations))
    self.AddStory(GpuInfoCompletePage(self, expectations))
    self.AddStory(NoGpuProcessPage(self, expectations))
    self.AddStory(DriverBugWorkaroundsInGpuProcessPage(self, expectations))
    self.AddStory(ReadbackWebGLGpuProcessPage(self, expectations,
                                              is_platform_android))
    self.AddStory(DriverBugWorkaroundsUponGLRendererPage(self, expectations,
                                                         is_platform_android))
    self.AddStory(EqualBugWorkaroundsPage(self, expectations))
    self.AddStory(OnlyOneWorkaroundPage(self, expectations))

    if not is_platform_android:
      self.AddStory(SkipGpuProcessPage(self, expectations))
      self.AddStory(HasTransparentVisualsGpuProcessPage(self, expectations))
      self.AddStory(NoTransparentVisualsGpuProcessPage(self, expectations))

      # There is no Android multi-gpu configuration and the helper
      # gpu_info_collector.cc::IdentifyActiveGPU is not even called.
      self.AddStory(IdentifyActiveGpuPage1(self, expectations))
      self.AddStory(IdentifyActiveGpuPage2(self, expectations))
      self.AddStory(IdentifyActiveGpuPage3(self, expectations))
      self.AddStory(IdentifyActiveGpuPage4(self, expectations))

      # There is currently no entry in kSoftwareRenderingListJson that enables
      # a software GL driver on Android.
      self.AddStory(SoftwareGpuProcessPage(self, expectations))

  @property
  def allow_mixed_story_states(self):
    # Return True here in order to be able to run pages with different browser
    # command line arguments.
    return True
