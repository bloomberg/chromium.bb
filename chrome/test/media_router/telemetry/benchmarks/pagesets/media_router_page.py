# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import time

from telemetry import page
from telemetry import story
from telemetry.core import exceptions


class CastPage(page.Page):
  """Abstract Cast page for Media Router Telemetry tests."""

  def ChooseSink(self, tab, sink_name):
    """Chooses a specific sink in the list."""

    tab.ExecuteJavaScript(
      'var sinks = window.document.getElementById("media-router-container").'
      '  shadowRoot.getElementById("sink-list").getElementsByTagName("span");'
      'for (var i=0; i<sinks.length; i++) {'
      '  if(sinks[i].textContent.trim() == "%s") {'
      '    sinks[i].click();'
      '    break;'
      '}}' % sink_name);

  def CloseDialog(self, tab):
    """Closes media router dialog."""

    try:
      tab.ExecuteJavaScript(
          'window.document.getElementById("media-router-container").' +
          'shadowRoot.getElementById("container-header").shadowRoot.' +
          'getElementById("close-button").click();')
    except exceptions.DevtoolsTargetCrashException:
      # Ignore the crash exception, this exception is caused by the js
      # code which closes the dialog, it is expected.
      pass

  def ExecuteAsyncJavaScript(self, action_runner, script, verify_func,
                             error_message, timeout=5):
    """Executes async javascript function and waits until it finishes."""

    action_runner.ExecuteJavaScript(script)
    self._WaitForResult(action_runner, verify_func, error_message,
                        timeout=timeout)

  def _WaitForResult(self, action_runner, verify_func, error_message,
                     timeout=5):
    """Waits until the function finishes or timeout."""

    start_time = time.time()
    while (not verify_func() and
           time.time() - start_time < timeout):
      action_runner.Wait(1)
    if not verify_func():
      raise page.page_test.Failure(error_message)

  def _GetDeviceName(self):
    """Gets device name from environment variable RECEIVER_NAME."""
    if 'RECEIVER_NAME' not in os.environ or not os.environ.get('RECEIVER_NAME'):
      raise page.page_test.Failure(
          'Your test machine is not set up correctly, '
          'RECEIVER_NAME enviroment variable is missing.')
    return os.environ.get('RECEIVER_NAME')
