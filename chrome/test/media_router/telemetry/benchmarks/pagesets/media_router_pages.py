# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry import page
from telemetry import story
from telemetry.page import shared_page_state

class SharedState(shared_page_state.SharedPageState):
  """Shared state that restarts the browser for every single story."""

  def __init__(self, test, finder_options, story_set):
    super(SharedState, self).__init__(
        test, finder_options, story_set)

  def DidRunStory(self, results):
    super(SharedState, self).DidRunStory(results)
    self._StopBrowser()

class CastPage(page.Page):

  def __init__(self, page_set):
    super(CastPage, self).__init__(
        url='file://basic_test.html',
        page_set=page_set,
        shared_page_state_class=SharedState)

  def RunPageInteractions(self, action_runner):
    with action_runner.CreateInteraction('LaunchDialog'):
      action_runner.TapElement(selector='#start_session_button')
      action_runner.Wait(5)


class MediaRouterPageSet(story.StorySet):

  def __init__(self):
    super(MediaRouterPageSet, self).__init__(
        cloud_storage_bucket=story.PARTNER_BUCKET)
    self.AddStory(CastPage(self))
