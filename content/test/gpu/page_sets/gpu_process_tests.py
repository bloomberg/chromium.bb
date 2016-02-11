# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import sys
from telemetry.story import story_set as story_set_module
from telemetry.page import page_test

from gpu_tests import gpu_test_base

class GpuProcessSharedPageState(gpu_test_base.GpuSharedPageState):

  gpu_switches = ['--gpu-no-complete-info-collection',
                  '--gpu-testing-os-version',
                  '--gpu-testing-vendor-id',
                  '--gpu-testing-device-id',
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
      raise page_test.Failure('Browser must support system info')
    system_info = tab.browser.GetSystemInfo()
    if not system_info.gpu:
      raise page_test.Failure('Target machine must have a GPU')
    if not system_info.gpu.aux_attributes:
      raise page_test.Failure('Browser must support GPU aux attributes')
    if not 'gl_renderer' in system_info.gpu.aux_attributes:
      raise page_test.Failure('Browser must have gl_renderer in aux attribs')
    if len(system_info.gpu.aux_attributes['gl_renderer']) <= 0:
      raise page_test.Failure('Must have a non-empty gl_renderer string')


class NoGpuProcessSharedPageState(GpuProcessSharedPageState):
  def __init__(self, test, finder_options, story_set):
    super(NoGpuProcessSharedPageState, self).__init__(
      test, finder_options, story_set)
    options = finder_options.browser_options

    if sys.platform in ('cygwin', 'win32'):
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
      raise page_test.Failure('GPU process detected')


class SoftwareGpuProcessSharedPageState(GpuProcessSharedPageState):
  def __init__(self, test, finder_options, story_set):
    super(SoftwareGpuProcessSharedPageState, self).__init__(
      test, finder_options, story_set)
    options = finder_options.browser_options
    options.AppendExtraBrowserArgs('--gpu-testing-vendor-id=0x10de')
    options.AppendExtraBrowserArgs('--gpu-testing-device-id=0x0de1')
    options.AppendExtraBrowserArgs('--gpu-testing-gl-vendor=VMware')
    options.AppendExtraBrowserArgs('--gpu-testing-gl-renderer=SVGA3D')
    options.AppendExtraBrowserArgs('--gpu-testing-gl-version="2.1 Mesa 10.1"')


class SoftwareGpuProcessPage(gpu_test_base.PageBase):

  def __init__(self, story_set, expectations):
    super(SoftwareGpuProcessPage, self).__init__(
      url='about:blank',
      name='GpuProcess.software_gpu_process',
      page_set=story_set,
      shared_page_state_class=SoftwareGpuProcessSharedPageState,
      expectations=expectations)


class GpuProcessTestsStorySet(story_set_module.StorySet):

  """ Tests that accelerated content triggers the creation of a GPU process """

  def __init__(self, expectations):
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
    self.AddStory(SoftwareGpuProcessPage(self, expectations))

  @property
  def allow_mixed_story_states(self):
    # Return True here in order to be able to run pages with different browser
    # command line arguments.
    return True
