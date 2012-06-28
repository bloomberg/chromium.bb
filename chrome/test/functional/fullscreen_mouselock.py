#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import re
import shutil
import time

import pyauto_functional  # Must be imported before pyauto
import pyauto
import test_utils
from selenium.webdriver.common.action_chains import ActionChains
from selenium.common.exceptions import WebDriverException
from selenium.webdriver.common.keys import Keys
from webdriver_pages import settings


class FullscreenMouselockTest(pyauto.PyUITest):
  """TestCase for Fullscreen and Mouse Lock."""

  def setUp(self):
    pyauto.PyUITest.setUp(self)
    self._driver = self.NewWebDriver()
    # Get the hostname pattern (e.g. http://127.0.0.1:57622).
    self._hostname_pattern = (
        re.sub('/files/$', '', self.GetHttpURLForDataPath('')))

  def Debug(self):
    """Test method for experimentation.

    This method will not run automatically.
    """
    page = settings.ContentSettingsPage.FromNavigation(self._driver)
    import pdb
    pdb.set_trace()

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
    self.NavigateToURL(self.GetHttpURLForDataPath(
        'fullscreen_mouselock', 'fullscreen_mouselock.html'))

    # Starting off we shouldn't be fullscreen
    self.assertFalse(self.IsFullscreenForBrowser())
    self.assertFalse(self.IsFullscreenForTab())

    # Go fullscreen
    self._driver.find_element_by_id('enterFullscreen').click()
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
    self._driver.find_element_by_id('lockMouse1').click()
    self.assertTrue(self.WaitUntil(self.IsMouseLockPermissionRequested))
    self.assertFalse(self.IsMouseLocked())

    # Deny mouse lock.
    self.DenyCurrentFullscreenOrMouseLockRequest()
    self.assertTrue(self.WaitUntil(
        lambda: not self.IsFullscreenBubbleDisplayingButtons()))
    self.assertFalse(self.IsMouseLocked())

    # Try mouse lock again, and accept it.
    self._driver.find_element_by_id('lockMouse1').click()
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

  def _LaunchFSAndExpectPrompt(self, button_action='enterFullscreen'):
    """Helper function to launch fullscreen and expect a prompt.

    Fullscreen is initiated and a bubble prompt appears asking to allow or
    cancel from fullscreen mode. The actual fullscreen mode doesn't take place
    until after approving the prompt.

    If the helper is not successful then the test will fail.

    Args:
      button_action: The button id to click to initiate an action. Default is to
          click enterFullscreen.
    """
    self.NavigateToURL(self.GetHttpURLForDataPath(
        'fullscreen_mouselock', 'fullscreen_mouselock.html'))
    # Should not be in fullscreen mode during initial launch.
    self.assertFalse(self.IsFullscreenForBrowser())
    self.assertFalse(self.IsFullscreenForTab())
    # Go into fullscreen mode.
    self._driver.find_element_by_id(button_action).click()
    self.assertTrue(self.WaitUntil(self.IsFullscreenForTab))
    # Bubble should display prompting to allow fullscreen.
    self.assertTrue(self.IsFullscreenPermissionRequested())

  def _InitiateBrowserFullscreen(self):
    """Helper function that initiates browser fullscreen."""
    self.NavigateToURL(self.GetHttpURLForDataPath(
        'fullscreen_mouselock', 'fullscreen_mouselock.html'))
    # Should not be in fullscreen mode during initial launch.
    self.assertFalse(self.IsFullscreenForBrowser())
    self.assertFalse(self.IsFullscreenForTab())
    # Initiate browser fullscreen.
    self.ApplyAccelerator(pyauto.IDC_FULLSCREEN)
    self.assertTrue(self.WaitUntil(self.IsFullscreenForBrowser))
    self.assertTrue(self.WaitUntil(lambda: not self.IsFullscreenForTab()))
    self.assertTrue(self.WaitUntil(lambda: not self.IsMouseLocked()))

  def _InitiateTabFullscreen(self):
    """Helper function that initiates tab fullscreen."""
    self.NavigateToURL(self.GetHttpURLForDataPath(
        'fullscreen_mouselock', 'fullscreen_mouselock.html'))
    # Initiate tab fullscreen.
    self._driver.find_element_by_id('enterFullscreen').click()
    self.assertTrue(self.WaitUntil(self.IsFullscreenForTab))

  def _AcceptFullscreenOrMouseLockRequest(self):
    """Helper function to accept fullscreen or mouse lock request."""
    self.AcceptCurrentFullscreenOrMouseLockRequest()
    self.assertTrue(self.WaitUntil(
        lambda: not self.IsFullscreenBubbleDisplayingButtons()))

  def _EnableFullscreenAndMouseLockMode(self):
    """Helper function to enable fullscreen and mouse lock mode."""
    self._LaunchFSAndExpectPrompt(button_action='enterFullscreenAndLockMouse1')
    # Allow fullscreen.
    self.AcceptCurrentFullscreenOrMouseLockRequest()
    # The wait is needed due to crbug.com/123396. Should be able to click the
    # fullscreen and mouselock button and be both accepted in a single action.
    self.assertTrue(self.WaitUntil(self.IsMouseLockPermissionRequested))
    # Allow mouse lock.
    self.AcceptCurrentFullscreenOrMouseLockRequest()
    self.assertTrue(self.WaitUntil(self.IsMouseLocked))

  def _EnableMouseLockMode(self, button_action='lockMouse1'):
    """Helper function to enable mouse lock mode.

    Args:
      button_action: The button id to click to initiate an action. Default is to
          click lockMouse1.
    """
    self._driver.find_element_by_id(button_action).click()
    self.assertTrue(self.WaitUntil(self.IsMouseLockPermissionRequested))
    self.AcceptCurrentFullscreenOrMouseLockRequest()
    self.assertTrue(self.IsMouseLocked())

  def _EnableAndReturnLockMouseResult(self):
    """Helper function to enable and return mouse lock result."""
    self.NavigateToURL(self.GetHttpURLForDataPath(
        'fullscreen_mouselock', 'fullscreen_mouselock.html'))
    self._driver.find_element_by_id('lockMouse2').click()
    self.assertTrue(
        self.WaitUntil(self.IsMouseLockPermissionRequested))
    self.AcceptCurrentFullscreenOrMouseLockRequest()
    # Waits until lock_result gets 'success' or 'failure'.
    return self._driver.execute_script('return lock_result')

  def _ClickAnchorLink(self):
    """Clicks the anchor link until it's successfully clicked.

    Clicks on the anchor link and compares the js |clicked_elem_ID| variabled
    with the anchor id. Returns True if the link is clicked.
    """
    element_id = 'anchor'
    # Catch WebDriverException: u'Element is not clickable at point (185.5,
    # 669.5). Instead another element would receive the click.
    try:
      self._driver.find_element_by_id(element_id).click()
    except WebDriverException:
      return False
    return self._driver.execute_script('return clicked_elem_ID') == element_id

  def testPrefsForFullscreenAllowed(self):
    """Verify prefs when fullscreen is allowed."""
    self._LaunchFSAndExpectPrompt()
    self._AcceptFullscreenOrMouseLockRequest()
    content_settings = (
        self.GetPrefsInfo().Prefs()['profile']['content_settings'])
    self.assertEqual(
        {self._hostname_pattern + ',*': {'fullscreen': 1}},  # Allow hostname.
        content_settings['pattern_pairs'],
        msg='Saved hostname pattern does not match expected pattern.')

  def testPrefsForFullscreenExit(self):
    """Verify prefs is empty when exit fullscreen mode before allowing."""
    self._LaunchFSAndExpectPrompt()
    self._driver.find_element_by_id('exitFullscreen').click()
    # Verify exit from fullscreen mode.
    self.assertTrue(self.WaitUntil(lambda: not self.IsFullscreenForTab()))
    content_settings = (
        self.GetPrefsInfo().Prefs()['profile']['content_settings'])
    self.assertEqual(
        {}, content_settings['pattern_pairs'],
        msg='Patterns saved when there should be none.')

  def testPatternsForFSAndML(self):
    """Verify hostname pattern and behavior for allowed mouse cursor lock.

    To lock the mouse, the browser needs to be in fullscreen mode.
    """
    self._EnableFullscreenAndMouseLockMode()
    self._EnableMouseLockMode()
    expected_pattern = (
        {self._hostname_pattern + ',*': {'fullscreen': 1, 'mouselock': 1}})
    content_settings = (
        self.GetPrefsInfo().Prefs()['profile']['content_settings'])
    self.assertEqual(
        expected_pattern, content_settings['pattern_pairs'],
        msg='Saved hostname and behavior patterns do not match expected.')

  def testPatternsForAllowMouseLock(self):
    """Verify hostname pattern and behavior for allowed mouse cursor lock.

    Enable fullscreen mode and enable mouse lock separately.
    """
    self._LaunchFSAndExpectPrompt()
    self.AcceptCurrentFullscreenOrMouseLockRequest()
    self._EnableMouseLockMode()
    expected_pattern = (
        {self._hostname_pattern + ',*': {'fullscreen': 1, 'mouselock': 1}})
    content_settings = (
        self.GetPrefsInfo().Prefs()['profile']['content_settings'])
    self.assertEqual(
        expected_pattern, content_settings['pattern_pairs'],
        msg='Saved hostname and behavior patterns do not match expected.')

  def testNoMouseLockRequest(self):
    """Verify mouse lock request does not appear.

    When allowing all sites to disable the mouse cursor, the mouse lock request
    bubble should not show. The mouse cursor should be automatically disabled
    when clicking on a disable mouse button.
    """
    # Allow all sites to disable mouse cursor.
    self.SetPrefs(pyauto.kDefaultContentSettings, {u'mouselock': 1})
    self._LaunchFSAndExpectPrompt()
    # Allow for fullscreen mode.
    self._AcceptFullscreenOrMouseLockRequest()
    self._driver.set_script_timeout(2)
    # Receive callback status (success or failure) from javascript that the
    # click has registered and the mouse lock status has changed.
    lock_result = self._driver.execute_async_script(
        'lockMouse1(arguments[arguments.length - 1])')
    self.assertEqual(lock_result, 'success', msg='Mouse lock unsuccessful.')
    self.assertTrue(self.WaitUntil(
        lambda: not self.IsMouseLockPermissionRequested()))
    self.assertTrue(self.IsMouseLocked())

  def testUnableToLockMouse(self):
    """Verify mouse lock is disabled.

    When not allowing any site to disable the mouse cursor, the mouse lock
    request bubble should not show and the mouse cursor should not be disabled.
    """
    # Do not allow any site to disable mouse cursor.
    self.SetPrefs(pyauto.kDefaultContentSettings, {u'mouselock': 2})
    self._LaunchFSAndExpectPrompt()
    # Allow for fullscreen mode.
    self._AcceptFullscreenOrMouseLockRequest()
    self._driver.set_script_timeout(2)
    # Receive callback status (success or failure) from javascript that the
    # click has registered and the mouse lock status has changed.
    lock_result = self._driver.execute_async_script(
        'lockMouse1(arguments[arguments.length - 1])')
    self.assertEqual(lock_result, 'failure', msg='Mouse locked unexpectedly.')
    self.assertTrue(self.WaitUntil(
        lambda: not self.IsMouseLockPermissionRequested()))
    self.assertTrue(self.WaitUntil(lambda: not self.IsMouseLocked()))

  def testEnterTabFSWhileInBrowserFS(self):
    """Verify able to enter into tab fullscreen while in browser fullscreen."""
    self._InitiateBrowserFullscreen()
    # Initiate tab fullscreen.
    self._driver.find_element_by_id('enterFullscreen').click()
    self.assertTrue(self.WaitUntil(lambda: self.IsFullscreenForTab()))
    self.assertTrue(self.WaitUntil(lambda: not self.IsMouseLocked()))

  def testMouseLockInBrowserFS(self):
    """Verify mouse lock in browser fullscreen requires allow prompt."""
    self._InitiateBrowserFullscreen()
    self._driver.set_script_timeout(2)
    self._driver.execute_script('lockMouse1AndSetLockResult()')
    # Bubble should display prompting to allow mouselock.
    self.assertTrue(self.WaitUntil(self.IsMouseLockPermissionRequested))
    self.AcceptCurrentFullscreenOrMouseLockRequest()
    # Waits until lock_result gets 'success' or 'failure'.
    lock_result = self._driver.execute_script('return lock_result')
    self.assertEqual(lock_result, 'success',
        msg='Mouse was not locked in browser fullscreen.')

  def testNoMouseLockWhenCancelFS(self):
    """Verify mouse lock breaks when canceling tab fullscreen.

    This test uses javascript to initiate exit of tab fullscreen after mouse
    lock success callback.
    """
    self._LaunchFSAndExpectPrompt()
    self._driver.set_script_timeout(2)
    lock_result = self._driver.execute_script('lockMouse1AndSetLockResult()')
    self.assertTrue(
        self.WaitUntil(lambda: self.IsMouseLockPermissionRequested()))
    self.AcceptCurrentFullscreenOrMouseLockRequest()
    # Waits until lock_result gets 'success' or 'failure'.
    lock_result = self._driver.execute_script('return lock_result')
    self.assertEqual(
        lock_result, 'success', msg='Mouse is not locked.')
    self._driver.execute_script('document.webkitCancelFullScreen()')
    self.assertTrue(self.WaitUntil(lambda: not self.IsFullscreenForTab()),
                    msg='Tab is still in fullscreen.')
    self.assertTrue(self.WaitUntil(lambda: not self.IsMouseLocked()),
                    msg='Mouse is still locked after exiting fullscreen.')

  def testNoTabFSExitWhenJSExitMouseLock(self):
    """Verify tab fullscreen does not exit when javascript init mouse lock exit.

    This test uses javascript to initiate exit of mouse lock after mouse
    lock success callback.
    """
    self._LaunchFSAndExpectPrompt()
    self._EnableMouseLockMode()
    self._driver.execute_script('navigator.webkitPointer.unlock()')
    self.WaitUntil(lambda: not self.IsMouseLocked())
    self.assertTrue(self.IsFullscreenForTab(), msg='Tab fullscreen was lost.')

  def testMouseLockExitWhenAlertDialogShow(self):
    """Verify mouse lock breaks when alert dialog appears."""
    self._LaunchFSAndExpectPrompt()
    self._EnableMouseLockMode()
    # Need to catch the exception here since the alert dialog raises
    # a WebDriverException due to a modal dialog.
    from selenium.common.exceptions import WebDriverException
    try:
      self._driver.execute_script('alert("A modal dialog")')
    except WebDriverException:
      pass

    self.assertTrue(self.WaitUntil(lambda: self.IsFullscreenForTab()),
                    msg='Tab fullscreen was lost.')
    self.assertTrue(self.WaitUntil(lambda: not self.IsMouseLocked()),
                    msg='Mouse is still locked')

  def testMouseLockExitWhenBrowserLoseFocus(self):
    """Verify mouse lock breaks when browser loses focus.

    Mouse lock breaks when the focus is placed on another new window.
    """
    self._LaunchFSAndExpectPrompt()
    self.AcceptCurrentFullscreenOrMouseLockRequest()
    # Open a new window to shift focus away.
    self.OpenNewBrowserWindow(True)
    self.assertTrue(self.WaitUntil(lambda: self.IsFullscreenForTab()))
    self.assertTrue(self.WaitUntil(lambda: not self.IsMouseLocked()),
                    msg='Mouse lock did not break when browser lost focus.')

  def testMouseLockLostOnReload(self):
    """Verify mouse lock is lost on page reload."""
    self.NavigateToURL(self.GetHttpURLForDataPath(
        'fullscreen_mouselock', 'fullscreen_mouselock.html'))
    self._EnableMouseLockMode()
    self.ReloadActiveTab()
    self.assertTrue(self.WaitUntil(lambda: not self.IsMouseLocked()),
                    msg='Mouse lock did not break when page is reloaded.')

  def testNoMLBubbleWhenTabLoseFocus(self):
    """Verify mouse lock bubble goes away when tab loses focus."""
    self.NavigateToURL(self.GetHttpURLForDataPath(
        'fullscreen_mouselock', 'fullscreen_mouselock.html'))
    self._driver.find_element_by_id('lockMouse1').click()
    self.assertTrue(self.WaitUntil(self.IsMouseLockPermissionRequested))
    self.AppendTab(pyauto.GURL('chrome://newtab'))
    self.assertTrue(self.WaitUntil(
        lambda: not self.IsFullscreenBubbleDisplayingButtons()),
                    msg='Mouse lock bubble did not clear when tab lost focus.')

  def testTabFSExitWhenNavBackToPrevPage(self):
    """Verify tab fullscreen exit when navigating back to previous page.

    This test navigates to a new page while in tab fullscreen mode by using
    GoBack() to navigate to the previous google.html page.
    """
    self.NavigateToURL(self.GetHttpURLForDataPath('google', 'google.html'))
    self._InitiateTabFullscreen()
    self.GetBrowserWindow().GetTab().GoBack()
    self.assertFalse(
        self.IsFullscreenForTab(),
        msg='Tab fullscreen did not exit when navigating to a new page.')

  def testTabFSExitWhenNavToNewPage(self):
    """Verify tab fullscreen exit when navigating to a new website.

    This test navigates to a new website while in tab fullscreen.
    """
    self._InitiateTabFullscreen()
    self.NavigateToURL(self.GetHttpURLForDataPath('google', 'google.html'))
    self.assertFalse(
        self.IsFullscreenForTab(),
        msg='Tab fullscreen did not exit when navigating to a new website.')

  def testTabFSDoesNotExitForAnchorLinks(self):
    """Verify tab fullscreen does not exit for anchor links.

    Tab fullscreen should not exit when following a link to the same page such
    as example.html#anchor.
    """
    self._InitiateTabFullscreen()
    self.assertTrue(self.WaitUntil(self._ClickAnchorLink))
    self.assertTrue(
        self.WaitUntil(self.IsFullscreenForTab),
        msg='Tab fullscreen should not exit when clicking on an anchor link.')

  def testMLExitWhenNavBackToPrevPage(self):
    """Verify mouse lock exit when navigating back to previous page.

    This test navigates to a new page while mouse lock is activated by using
    GoBack() to navigate to the previous google.html page.
    """
    self.NavigateToURL(self.GetHttpURLForDataPath('google', 'google.html'))
    lock_result = self._EnableAndReturnLockMouseResult()
    self.assertEqual(
        lock_result, 'success', msg='Mouse is not locked.')
    self.GetBrowserWindow().GetTab().GoBack()
    self.assertFalse(
        self.IsMouseLocked(),
        msg='Mouse lock did not exit when navigating to the prev page.')

  def testMLExitWhenNavToNewPage(self):
    """Verify mouse lock exit when navigating to a new website."""
    lock_result = self._EnableAndReturnLockMouseResult()
    self.assertEqual(
        lock_result, 'success', msg='Mouse is not locked.')
    self.NavigateToURL(self.GetHttpURLForDataPath('google', 'google.html'))
    self.assertFalse(
        self.IsMouseLocked(),
        msg='Mouse lock did not exit when navigating to a new website.')

  def testMLDoesNotExitForAnchorLinks(self):
    """Verify mouse lock does not exit for anchor links.

    Mouse lock should not exist when following a link to the same page such as
    example.html#anchor.
    """
    lock_result = self._EnableAndReturnLockMouseResult()
    self.assertEqual(
        lock_result, 'success', msg='Mouse is not locked.')
    ActionChains(self._driver).move_to_element(
        self._driver.find_element_by_id('anchor')).click().perform()
    self.assertTrue(self.WaitUntil(self.IsMouseLocked),
                    msg='Mouse lock broke when clicking on an anchor link.')

  def ExitTabFSToBrowserFS(self):
    """Verify exiting tab fullscreen leaves browser in browser fullscreen.

    This test is semi-automated.

    The browser initiates browser fullscreen, then initiates tab fullscreen. The
    test verifies that existing tab fullscreen by simulating ESC key press or
    clicking the js function to exitFullscreen() will exit the tab fullscreen
    leaving browser fullscreen intact.
    """
    self._InitiateBrowserFullscreen()
    # Initiate tab fullscreen.
    self._driver.find_element_by_id('enterFullscreen').click()
    self.assertTrue(self.WaitUntil(lambda: self.IsFullscreenForTab()))
    # Require manual intervention to send ESC key due to crbug.com/123930.
    # TODO(dyu): Update to a full test once associated bug is fixed.
    logging.info('Press ESC key to exit tab fullscreen.')
    time.sleep(5)
    self.assertTrue(self.WaitUntil(lambda: not self.IsFullscreenForTab()))
    self.assertTrue(self.WaitUntil(lambda: self.IsFullscreenForBrowser()),
                    msg='Not in browser fullscreen mode.')

    self._driver.find_element_by_id('enterFullscreen').click()
    self.assertTrue(self.WaitUntil(lambda: self.IsFullscreenForTab()))
    # Exit tab fullscreen by clicking button exitFullscreen().
    self._driver.find_element_by_id('exitFullscreen').click()
    self.assertTrue(self.WaitUntil(lambda: not self.IsFullscreenForTab()))
    self.assertTrue(self.WaitUntil(lambda: self.IsFullscreenForBrowser()),
                    msg='Not in browser fullscreen mode.')

  def F11KeyExitsTabAndBrowserFS(self):
    """Verify existing tab fullscreen exits all fullscreen modes.

    This test is semi-automated.

    The browser initiates browser fullscreen, then initiates tab fullscreen. The
    test verifies that existing tab fullscreen by simulating F11 key press or
    CMD + SHIFT + F keys on the Mac will exit the tab fullscreen and the
    browser fullscreen.
    """
    self._InitiateBrowserFullscreen()
    # Initiate tab fullscreen.
    self._driver.find_element_by_id('enterFullscreen').click()
    self.assertTrue(self.WaitUntil(lambda: self.IsFullscreenForTab()))
    # Require manual intervention to send F11 key due to crbug.com/123930.
    # TODO(dyu): Update to a full test once associated bug is fixed.
    logging.info('Press F11 key to exit tab fullscreen.')
    time.sleep(5)
    self.assertTrue(self.WaitUntil(lambda: not self.IsFullscreenForTab()))
    self.assertTrue(self.WaitUntil(lambda: not self.IsFullscreenForBrowser()),
                    msg='Browser is in fullscreen mode.')

  def SearchForTextOutsideOfContainer(self):
    """Verify text outside of container is not visible when fullscreen.

    This test is semi-automated.

    Verify this test manually until there is a way to find text on screen
    without using FindInPage().

    The text that is outside of the fullscreen container should only be visible
    when fullscreen is off. The text should not be visible while in fullscreen
    mode.
    """
    self.NavigateToURL(self.GetHttpURLForDataPath(
        'fullscreen_mouselock', 'fullscreen_mouselock.html'))
    # Should not be in fullscreen mode during initial launch.
    self.assertFalse(self.IsFullscreenForBrowser())
    self.assertFalse(self.IsFullscreenForTab())
    self.assertTrue(
        self.WaitUntil(lambda: self.FindInPage(
            'This text is outside of the container')['match_count'],
                       expect_retval=1))
    # Go into fullscreen mode.
    self._driver.find_element_by_id('enterFullscreen').click()
    self.assertTrue(self.WaitUntil(self.IsFullscreenForTab))
    time.sleep(5)
    # TODO(dyu): find a way to verify on screen text instead of using
    #            FindInPage() which searches for text in the HTML.

  def SameMouseLockMovement(self):
    """Verify the correct feel of mouse movement data when mouse is locked.

    This test is semi-automated.

    This test loads the same web page in two different tabs while in mouse lock
    mode. Each tab loads the web page from a different URL (e.g. by loading it
    from a localhost server and a file url). The test verifies
    that the mouse lock movements work the same in both
    tabs.
    """
    url1 = self.GetHttpURLForDataPath(
        'fullscreen_mouselock', 'fullscreen_mouselock.html')
    url2 = self.GetFileURLForDataPath(
        'fullscreen_mouselock', 'fullscreen_mouselock.html')
    tab2 = 'f1-4'
    self.NavigateToURL(url1)
    self.RunCommand(pyauto.IDC_NEW_TAB)  # Open new tab.
    self.NavigateToURL(url2, 0, 1)
    self._driver.switch_to_window(tab2)
    self._EnableMouseLockMode()  # Lock mouse in tab 2.
    raw_input('Manually move the mouse cursor on the page in tab 2. Shift+Tab \
              into tab 1, click on lockMouse1() button, and move the mouse \
              cursor on the page in tab 1. Verify mouse movement is smooth.')

  def MouseEventsIndependentOfExitBubble(self):
    """Verify mouse events are independent of the exit FS exit bubble for ML.

    Mouse movement events should work immediately when mouse lock is activated.
    The events should not be blocked waiting for the exit instruction bubble to
    clear.
    """
    self.NavigateToURL(self.GetHttpURLForDataPath(
        'fullscreen_mouselock', 'fullscreen_mouselock.html'))
    # Should not be in fullscreen mode during initial launch.
    self.assertFalse(self.IsFullscreenForBrowser())
    self.assertFalse(self.IsFullscreenForTab())
    # Go into fullscreen mode.
    self._driver.find_element_by_id('enterFullscreen').click()
    self.assertTrue(self.WaitUntil(self.IsFullscreenForTab))
    self._EnableMouseLockMode()
    raw_input(
        '1. Move the mouse, see movement data being received by the page.\
        2. Press ESC key.\
        3. Lock the mouse without going fullscreen. Click lockMouse1() button.\
        Verify: The mouse movement events should work immediately.')

if __name__ == '__main__':
  pyauto_functional.Main()
