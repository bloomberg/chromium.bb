# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.story import story_set as story_set_module

import sys

from gpu_tests import gpu_test_base


class PixelTestsPage(gpu_test_base.PageBase):

  def __init__(self, url, name, test_rect, revision, story_set,
               shared_page_state_class, expectations, expected_colors=None):
    super(PixelTestsPage, self).__init__(
      url=url, page_set=story_set, name=name,
      shared_page_state_class=shared_page_state_class,
      expectations=expectations)
    self.test_rect = test_rect
    self.revision = revision
    if expected_colors:
      self.expected_colors = expected_colors

  def RunNavigateSteps(self, action_runner):
    super(PixelTestsPage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'domAutomationController._finished', timeout_in_seconds=30)


class PixelTestsES3SharedPageState(gpu_test_base.GpuSharedPageState):
  def __init__(self, test, finder_options, story_set):
    super(PixelTestsES3SharedPageState, self).__init__(
      test, finder_options, story_set)
    finder_options.browser_options.AppendExtraBrowserArgs(
      ['--enable-unsafe-es3-apis'])


class PixelTestsStorySet(story_set_module.StorySet):

  """ Some basic test cases for GPU. """
  def __init__(self, expectations, base_name='Pixel', try_es3=False):
    super(PixelTestsStorySet, self).__init__()
    self._AddAllPages(expectations, base_name, False)
    # Would be better to fetch this from Telemetry.
    # TODO(kbr): enable this on all platforms. Don't know what will
    # happen on Android right now.
    if try_es3 and sys.platform.startswith('darwin'):
      # Add all the tests again, this time with the
      # --enable-unsafe-es3-apis command line argument. This has the
      # side-effect of enabling the Core Profile rendering path on Mac
      # OS.
      self._AddAllPages(expectations, base_name, True)

  def _AddAllPages(self, expectations, base_name, use_es3):
    if use_es3:
      es3_suffix = 'ES3'
      shared_page_state_class = PixelTestsES3SharedPageState
    else:
      es3_suffix = ''
      shared_page_state_class = gpu_test_base.GpuSharedPageState

    self.AddStory(PixelTestsPage(
      url='file://../../data/gpu/pixel_canvas2d.html',
      name=base_name + '.Canvas2DRedBox' + es3_suffix,
      test_rect=[0, 0, 300, 300],
      revision=7,
      story_set=self,
      shared_page_state_class=shared_page_state_class,
      expectations=expectations))

    self.AddStory(PixelTestsPage(
      url='file://../../data/gpu/pixel_css3d.html',
      name=base_name + '.CSS3DBlueBox' + es3_suffix,
      test_rect=[0, 0, 300, 300],
      revision=15,
      story_set=self,
      shared_page_state_class=shared_page_state_class,
      expectations=expectations))

    self.AddStory(PixelTestsPage(
      url='file://../../data/gpu/pixel_webgl.html',
      name=base_name + '.WebGLGreenTriangle' + es3_suffix,
      test_rect=[0, 0, 300, 300],
      revision=12,
      story_set=self,
      shared_page_state_class=shared_page_state_class,
      expectations=expectations))

    self.AddStory(PixelTestsPage(
      url='file://../../data/gpu/pixel_scissor.html',
      name=base_name + '.ScissorTestWithPreserveDrawingBuffer' + es3_suffix,
      test_rect=[0, 0, 300, 300],
      revision=0, # This is not used.
      story_set=self,
      shared_page_state_class=shared_page_state_class,
      expectations=expectations,
      expected_colors='../../data/gpu/pixel_scissor_expectations.json'))

  @property
  def allow_mixed_story_states(self):
    # Return True here in order to be able to add the same tests with
    # a different SharedPageState on Mac which tests them with the
    # Core Profile rendering path.
    return True
