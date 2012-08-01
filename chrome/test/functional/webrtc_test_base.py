#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pyauto


class WebrtcTestBase(pyauto.PyUITest):
  """This base class provides helpers for getUserMedia calls."""

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