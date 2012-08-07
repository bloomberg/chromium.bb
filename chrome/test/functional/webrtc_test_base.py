#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pyauto


class WebrtcTestBase(pyauto.PyUITest):
  """This base class provides helpers for getUserMedia calls."""

  def GetUserMedia(self, tab_index, action='allow'):
    """Acquires webcam or mic for one tab and returns the result.

    Args:
      tab_index: The tab to request user media on.
      action: The action to take on the info bar. Can be 'allow', 'deny' or
          'dismiss'.

    Returns:
      A string as specified by the getUserMedia javascript function.
    """
    self.assertEquals('ok-requested', self.ExecuteJavascript(
        'getUserMedia(true, true)', tab_index=tab_index))

    self.WaitForInfobarCount(1, tab_index=tab_index)
    self.PerformActionOnInfobar(action, infobar_index=0, tab_index=tab_index)
    self.WaitForGetUserMediaResult(tab_index=0)

    result = self.GetUserMediaResult(tab_index=0)
    self.AssertNoFailures(tab_index)
    return result

  def WaitForGetUserMediaResult(self, tab_index):
    """Waits until WebRTC has responded to a getUserMedia query.

    Fails an assert if WebRTC doesn't respond within the default timeout.

    Args:
      tab_index: the tab to query.
    """
    def HasResult():
      return self.GetUserMediaResult(tab_index) != 'not-called-yet'
    self.assertTrue(self.WaitUntil(HasResult),
                    msg='Timed out while waiting for getUserMedia callback.')

  def GetUserMediaResult(self, tab_index):
    """Retrieves WebRTC's answer to a user media query.

    Args:
      tab_index: the tab to query.

    Returns:
      Specified in obtainGetUserMediaResult() in getusermedia.js.
    """
    return self.ExecuteJavascript(
        'obtainGetUserMediaResult()', tab_index=tab_index)

  def AssertNoFailures(self, tab_index):
    """Ensures the javascript hasn't registered any asynchronous errors.

    Args:
      tab_index: The tab to check.
    """
    self.assertEquals('ok-no-errors', self.ExecuteJavascript(
        'getAnyTestFailures()', tab_index=tab_index))
