# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry import page
from telemetry import story
from telemetry.page import shared_page_state
from telemetry.core import exceptions


class CastPage(page.Page):

  def __init__(self, page_set):
    super(CastPage, self).__init__(
        url='file://basic_test.html',
        page_set=page_set)

  def RunPageInteractions(self, action_runner):
    with action_runner.CreateInteraction('LaunchDialog'):
      # Wait for 5s after Chrome is opened in order to get consistent results.
      action_runner.Wait(5)
      action_runner.TapElement(selector='#start_session_button')
      action_runner.Wait(5)
      for tab in action_runner.tab.browser.tabs:
        # Close media router dialog
        if tab.url == 'chrome://media-router/':
          try:
            tab.ExecuteJavaScript(
                'window.document.getElementById("media-router-container").' +
                'shadowRoot.getElementById("container-header").shadowRoot.' +
                'getElementById("close-button").click();')
          except exceptions.DevtoolsTargetCrashException:
            # Ignore the crash exception, this exception is caused by the js
            # code which closes the dialog, it is expected.
            pass


class MediaRouterPageSet(story.StorySet):

  def __init__(self):
    super(MediaRouterPageSet, self).__init__(
        cloud_storage_bucket=story.PARTNER_BUCKET)
    self.AddStory(CastPage(self))
