# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry import page
from telemetry import story
from benchmarks.pagesets import media_router_page
from telemetry.core import exceptions
from telemetry.page import shared_page_state


class SharedState(shared_page_state.SharedPageState):
  """Shared state that restarts the browser for every single story."""

  def __init__(self, test, finder_options, story_set):
    super(SharedState, self).__init__(
        test, finder_options, story_set)

  def DidRunStory(self, results):
    super(SharedState, self).DidRunStory(results)
    self._StopBrowser()


class CastDialogPage(media_router_page.CastPage):
  """Cast page to open a cast-enabled page and open media router dialog."""

  def __init__(self, page_set, url='file://basic_test.html',
               shared_page_state_class=shared_page_state.SharedPageState):
    super(CastDialogPage, self).__init__(
        url=url, page_set=page_set,
        shared_page_state_class=shared_page_state_class)

  def RunPageInteractions(self, action_runner):
     # Wait for 5s after Chrome is opened in order to get consistent results.
    action_runner.Wait(5)
    with action_runner.CreateInteraction('LaunchDialog'):
      action_runner.ExecuteJavaScript('collectPerfData();')
      self._WaitForResult(
          action_runner,
          lambda: action_runner.EvaluateJavaScript('initialized'),
          'Failed to initialize')

      action_runner.TapElement(selector='#start_session_button')
      action_runner.Wait(5)
      for tab in action_runner.tab.browser.tabs:
        # Close media router dialog
        if tab.url == 'chrome://media-router/':
          self.CloseDialog(tab)


class CastIdlePage(CastDialogPage):
  """Cast page to open a cast-enabled page and do nothing."""

  def __init__(self, page_set):
    super(CastIdlePage, self).__init__(
        page_set=page_set,
        url='file://basic_test.html',
        shared_page_state_class=SharedState)

  def RunPageInteractions(self, action_runner):
    super(CastIdlePage, self).RunPageInteractions(action_runner)
    action_runner.Wait(15)


class CastFlingingPage(media_router_page.CastPage):
  """Cast page to fling a video to Chromecast device."""

  def __init__(self, page_set):
    super(CastFlingingPage, self).__init__(
        page_set=page_set,
        url='file://basic_test.html#flinging',
        shared_page_state_class=SharedState)

  def RunPageInteractions(self, action_runner):
     # Wait for 5s after Chrome is opened in order to get consistent results.
    action_runner.Wait(5)
    with action_runner.CreateInteraction('flinging'):
      action_runner.ExecuteJavaScript('collectPerfData();')

      self._WaitForResult(
          action_runner,
          lambda: action_runner.EvaluateJavaScript('initialized'),
          'Failed to initialize')

      # Start session
      action_runner.TapElement(selector='#start_session_button')
      self._WaitForResult(
          action_runner,
          lambda: len(action_runner.tab.browser.tabs) >= 2,
          'MR dialog never showed up.')

      # Wait for 2s to make sure the dialog is fully loaded.
      action_runner.Wait(2)
      for tab in action_runner.tab.browser.tabs:
        # Choose sink
        if tab.url == 'chrome://media-router/':
          self.ChooseSink(tab, self._GetDeviceName())

      self._WaitForResult(
        action_runner,
        lambda: action_runner.EvaluateJavaScript('currentSession'),
         'Failed to start session',
         timeout=10)

      # Load Media
      # TODO: use local video instead of the public one.
      self.ExecuteAsyncJavaScript(
          action_runner,
          'loadMedia("http://easyhtml5video.com/images/happyfit2.mp4");',
          lambda: action_runner.EvaluateJavaScript('currentMedia'),
          'Failed to load media')

      action_runner.Wait(20)
      # Stop session
      self.ExecuteAsyncJavaScript(
          action_runner,
          'stopSession();',
          lambda: not action_runner.EvaluateJavaScript('currentSession'),
          'Failed to stop session')


class MediaRouterDialogPageSet(story.StorySet):
  """Pageset for media router dialog latency tests."""

  def __init__(self):
    super(MediaRouterDailogPageSet, self).__init__(
        cloud_storage_bucket=story.PARTNER_BUCKET)
    self.AddStory(CastDialogPage(self))


class MediaRouterCPUMemoryPageSet(story.StorySet):
  """Pageset for media router CPU/memory usage tests."""

  def __init__(self):
    super(MediaRouterCPUMemoryPageSet, self).__init__(
        cloud_storage_bucket=story.PARTNER_BUCKET)
    self.AddStory(CastIdlePage(self))
    self.AddStory(CastFlingingPage(self))
