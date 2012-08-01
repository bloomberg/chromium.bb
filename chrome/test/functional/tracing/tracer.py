# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pyauto_tracing
import pyauto
import tab_tracker
from timeline_model import TimelineModel

import os


class Tracer(object):
  """Controlls chrome and system tracing, returning a TimelineModel."""

  def __init__(self, browser, tab_tracker):
    """
    Args:
      browser: an instance of a PyUITest
      tab_tracker: an instance of a TabTracker
    """
    self._browser = browser
    self._tab_tracker = tab_tracker
    self._tab_uuid = tab_tracker.CreateTab('chrome://tracing')

    # TODO(eatnumber): Find a way to import timeline_model_shim.js from within
    # TimelineModelProxy
    # I'm not happy with importing timeline_model_shim.js
    # here. I'd rather pull it in from within TimelineModelProxy.
    # tracing_test.js depends on timeline_model_shim.js however.
    files = ['timeline_model_shim.js', 'tracer.js']
    for fileName in files:
      with open(os.path.join(os.path.dirname(__file__), fileName), 'r') as fd:
        self._ExecuteJavascript(fd.read())

  def __del__(self):
    self._tab_tracker.ReleaseTab(self._tab_uuid)

  def _ExecuteJavascript(self, js):
    return self._browser.ExecuteJavascript(
        js = js,
        windex = self._tab_tracker.GetWindowIndex(),
        tab_index = self._tab_tracker.GetTabIndex(self._tab_uuid)
    )

  def BeginTracing(self, system_tracing=True):
    """Start tracing.

    Args:
      system_tracing: whether or not to gather system traces along with chrome
      traces.
    """
    self._ExecuteJavascript("""
        window.domAutomationController.send(
            window.pyautoRecordTrace(%s)
        );
    """ % ('true' if system_tracing else 'false'))

  def EndTracing(self):
    """End tracing and return a TimelineModel.

    Returns:
      an instance of a TimelineModel which contains the results of the trace
    """
    return TimelineModel(
        js_executor = self._ExecuteJavascript.__get__(self, Tracer),
        shim_id = self._ExecuteJavascript('tracingController.endTracing();')
    )


class TracerFactory(object):
  """A TracerFactory is used to produce a Tracer.

  It's recommended to use the same TracerFactory to produce all Tracers so that
  the same TabTracker can be used throughout

  Args:
    browser: an instance of a PyUITest
  """
  def __init__(self, browser):
    self._tab_tracker = tab_tracker.TabTracker(browser)
    self._browser = browser

  def Produce(self):
    """Produce an instance of a Tracer"""
    return Tracer(self._browser, self._tab_tracker)
