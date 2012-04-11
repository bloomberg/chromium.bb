#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import shutil

import pyauto_functional  # Must be imported before pyauto
import pyauto
import test_utils
from selenium.webdriver.common.keys import Keys


class FullscreenMouselockTest(pyauto.PyUITest):
  """TestCase for Fullscreen and Mouse Lock."""

  def ExtraChromeFlags(self):
    """Ensures Chrome is launched with custom flags.

    Returns:
      A list of extra flags to pass to Chrome when it is launched.
    """
    # Extra flag needed by scroll performance tests.
    return super(FullscreenMouselockTest,
                 self).ExtraChromeFlags() + ['--enable-pointer-lock']

  def testFullScreenMouseLockHooks(self):
    """Verify fullscreen and mouse lock automation hooks work."""

    from webdriver_pages import settings
    from webdriver_pages.settings import Behaviors, ContentTypes
    driver = self.NewWebDriver()
    self.NavigateToURL(self.GetHttpURLForDataPath(
        'fullscreen_mouselock', 'fullscreen_mouselock.html'))

    # Starting off we shouldn't be fullscreen
    self.assertFalse(self.IsFullscreenForBrowser())
    self.assertFalse(self.IsFullscreenForTab())

    # Go fullscreen
    driver.find_element_by_id('enterFullscreen').click()
    self.assertTrue(self.WaitUntil(self.IsFullscreenForTab))

    # Bubble should be up prompting to allow fullscreen
    self.assertTrue(self.IsFullscreenBubbleDisplayed())
    self.assertTrue(self.IsFullscreenBubbleDisplayingButtons())
    self.assertTrue(self.IsFullscreenPermissionRequested())

    # Accept bubble, it should go away.
    self.AcceptCurrentFullscreenOrMouseLockRequest()
    self.assertTrue(self.WaitUntil(
        lambda: not self.IsFullscreenBubbleDisplayingButtons()))

    # Try to lock mouse, it won't lock yet but permision will be requested.
    self.assertFalse(self.IsMouseLocked())
    driver.find_element_by_id('lockMouse1').click()
    self.assertTrue(self.WaitUntil(self.IsMouseLockPermissionRequested))
    self.assertFalse(self.IsMouseLocked())

    # Deny mouse lock.
    self.DenyCurrentFullscreenOrMouseLockRequest()
    self.assertTrue(self.WaitUntil(
        lambda: not self.IsFullscreenBubbleDisplayingButtons()))
    self.assertFalse(self.IsMouseLocked())

    # Try mouse lock again, and accept it.
    driver.find_element_by_id('lockMouse1').click()
    self.assertTrue(self.WaitUntil(self.IsMouseLockPermissionRequested))
    self.AcceptCurrentFullscreenOrMouseLockRequest()
    self.assertTrue(self.WaitUntil(self.IsMouseLocked))

    # The following doesn't work - as sending the key to the input field isn't
    # picked up by the browser. :( Need an alternative way.
    #
    # # Ideally we wouldn't target a specific element, we'd just send keys to
    # # whatever the current keyboard focus was.
    # keys_target = driver.find_element_by_id('sendKeysTarget')
    #
    # # ESC key should exit fullscreen and mouse lock.
    #
    # print "# ESC key should exit fullscreen and mouse lock."
    # keys_target.send_keys(Keys.ESCAPE)
    # self.assertTrue(self.WaitUntil(lambda: not self.IsFullscreenForBrowser()))
    # self.assertTrue(self.WaitUntil(lambda: not self.IsFullscreenForTab()))
    # self.assertTrue(self.WaitUntil(lambda: not self.IsMouseLocked()))
    #
    # # Check we can go browser fullscreen
    # print "# Check we can go browser fullscreen"
    # keys_target.send_keys(Keys.F11)
    # self.assertTrue(self.WaitUntil(self.IsFullscreenForBrowser))


if __name__ == '__main__':
  pyauto_functional.Main()
