#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import urllib

import pyauto_functional
import pyauto


class NotificationsTest(pyauto.PyUITest):
  """Test of HTML5 desktop notifications."""
  def __init__(self, methodName='runTest'):
    super(NotificationsTest, self).__init__(methodName)
    self.EMPTY_PAGE_URL = self.GetHttpURLForDataPath('empty.html')
    # Content settings for default notification permission.
    self.ALLOW_ALL_SETTING = 1
    self.DENY_ALL_SETTING = 2
    self.ASK_SETTING = 3

    # HTML page used for notification testing.
    self.TEST_PAGE_URL = self.GetFileURLForDataPath(
        os.path.join('notifications', 'notification_tester.html'))

  def Debug(self):
    """Test method for experimentation.

    This method will not run automatically.
    """
    while True:
      raw_input('Interact with the browser and hit <enter> to dump notification'
                'state...')
      print '*' * 20
      self.pprint(self.GetActiveNotifications())
      self.pprint(self._GetDefaultPermissionSetting())

  def _SetDefaultPermissionSetting(self, setting):
    """Sets the default setting for whether sites are allowed to create
    notifications.
    """
    self.SetPrefs(pyauto.kDefaultContentSettings, {u'notifications': setting})

  def _GetDefaultPermissionSetting(self):
    """Gets the default setting for whether sites are allowed to create
    notifications.
    """
    return self.GetPrefsInfo().Prefs(
        pyauto.kDefaultContentSettings)[u'notifications']

  def _GetDeniedOrigins(self):
    """Gets the list of origins that are explicitly denied to create
    notifications.
    """
    return (self.GetPrefsInfo().Prefs(pyauto.kDesktopNotificationDeniedOrigins)
            or [])

  def _GetAllowedOrigins(self):
    """Gets the list of origins that are explicitly allowed to create
    notifications.
    """
    return (self.GetPrefsInfo().Prefs(pyauto.kDesktopNotificationAllowedOrigins)
            or [])

  def _SetAllowedOrigins(self, origins):
    """Sets the list of allowed origins to the given list.

    None of the items in the list should be explicitly denied.
    """
    return self.SetPrefs(pyauto.kDesktopNotificationAllowedOrigins, origins)

  def _SetDeniedOrigins(self, origins):
    """Sets the list of denied origins to the given list.

    None of the items in the list should be explicitly allowed.
    """
    return self.SetPrefs(pyauto.kDesktopNotificationDeniedOrigins, origins)

  def _DenyOrigin(self, new_origin):
    """Denies the given origin to create notifications.

    If it was explicitly allowed, that preference is dropped.
    """
    self._DropOriginPreference(new_origin)
    denied = self._GetDeniedOrigins()
    if new_origin not in denied:
      self._SetDeniedOrigins(denied + [new_origin])

  def _AllowOrigin(self, new_origin):
    """Allows the given origin to create notifications. If it was explicitly
    denied, that preference is dropped.
    """
    self._DropOriginPreference(new_origin)
    allowed = self._GetAllowedOrigins()
    if new_origin not in allowed:
      self._SetAllowedOrigins(allowed + [new_origin])

  def _DropOriginPreference(self, new_origin):
    """Drops the preference as to whether this origin should be allowed to
    create notifications. If it was explicitly allowed or explicitly denied,
    that preference is removed.
    """
    allowed = self._GetAllowedOrigins()
    if allowed and new_origin in allowed:
      allowed.remove(new_origin)
      self._SetAllowedOrigins(allowed)
    denied = self._GetDeniedOrigins()
    if denied and new_origin in denied:
      denied.remove(new_origin)
      self._SetDeniedOrigins(denied)

  def _AllowAllOrigins(self):
    """Allows any origin to create notifications."""
    self._SetDefaultPermissionSetting(self.ALLOW_ALL_SETTING)
    self._SetDeniedOrigins([])

  def _VerifyInfobar(self, origin, tab_index=0, windex=0):
    """Helper to verify the notification infobar contents are correct.

    Defaults to first tab in first window.

    Args:
      origin: origin of the notification, e.g., www.gmail.com
      tab_index: index of the tab within the given window
      windex: index of the window
    """
    tab_info = self.GetBrowserInfo()['windows'][windex]['tabs'][tab_index]
    self.assertEquals(1, len(tab_info['infobars']))
    infobar = tab_info['infobars'][0]
    text = 'Allow %s to show desktop notifications?' % origin
    self.assertEqual(text, infobar['text'])
    self.assertEqual(2, len(infobar['buttons']))
    self.assertEqual('Allow', infobar['buttons'][0])
    self.assertEqual('Deny', infobar['buttons'][1])

  def _CreateSimpleNotification(self, img_url, title, text,
                                replace_id='', tab_index=0, windex=0):
    """Creates a simple notification.

    Returns the id of the notification, which can be used to cancel it later.

    This executes a script in the page which shows a notification.
    This will only work if the page is navigated to |TEST_PAGE_URL|.
    The page must also have permission to show notifications.

    Args:
      img_url: url of a image to use; can be a data url
      title: title of the notification
      text: text in the notification
      replace_id: id string to be used for this notification. If another
                  notification is shown with the same replace_id, the former
                  will be replaced.
      tab_index: index of the tab within the given window
      windex: index of the window
    """
    return self.CallJavascriptFunc('createNotification',
                                   [img_url, title, text, replace_id],
                                   tab_index,
                                   windex);

  def _CreateHTMLNotification(self, content_url, replace_id='',
                              wait_for_display=True, tab_index=0, windex=0):
    """Creates an HTML notification.

    Returns the id of the notification, which can be used to cancel it later.

    This executes a script in the page which shows a notification.
    This will only work if the page is navigated to |TEST_PAGE_URL|.
    The page must also have permission to show notifications.

    Args:
      content_url: url of the page to show in the notification
      replace_id: id string to be used for this notification. If another
                  notification is shown with the same replace_id, the former
                  will be replaced.
      wait_for_display: whether we should wait for the notification to display
      tab_index: index of the tab within the given window
      windex: index of the window
    """
    return self.CallJavascriptFunc('createHTMLNotification',
                                   [content_url, replace_id, wait_for_display],
                                   tab_index,
                                   windex)

  def _RequestPermission(self, tab_index=0, windex=0):
    """Requests permission to create notifications.

    This will only work if the current page is navigated to |TEST_PAGE_URL|.

    Args:
      tab_index: index of the tab within the given window
      windex: index of the window
    """
    self.CallJavascriptFunc('requestPermission', [], tab_index, windex)

  def _CancelNotification(self, notification_id, tab_index=0, windex=0):
    """Cancels a notification with the given id.

    This canceling is done in the page that showed that notification and so
    follows a different path than closing a notification via the UI.

    This function should NOT be called until |WaitForNotificationCount| has been
    used to verify the notification is showing. This function cannot be used to
    cancel a notification that is in the display queue.

    This will only work if the page is navigated to |TEST_PAGE_URL|.

    Args:
      notification_id: id of the notification to cancel
      tab_index: index of the tab within the given window that created the
          notification
      windex: index of the window
    """
    msg = self.CallJavascriptFunc(
        'cancelNotification', [notification_id], tab_index, windex)
    # '1' signifies success.
    self.assertEquals('1', msg)

  def testCreateSimpleNotification(self):
    """Creates a simple notification."""
    self._AllowAllOrigins()
    self.NavigateToURL(self.TEST_PAGE_URL)
    self._CreateSimpleNotification('no_such_file.png', 'My Title', 'My Body')
    self.assertEquals(1, len(self.GetActiveNotifications()))
    notification = self.GetActiveNotifications()[0]
    html_data = urllib.unquote(notification['content_url'])
    self.assertTrue('no_such_file.png' in html_data)
    self.assertTrue('My Title' in html_data)
    self.assertTrue('My Body' in html_data)

  def testCreateHTMLNotification(self):
    """Creates an HTML notification using a fake url."""
    self._AllowAllOrigins()
    self.NavigateToURL(self.TEST_PAGE_URL)
    self._CreateHTMLNotification(self.EMPTY_PAGE_URL)
    self.assertEquals(1, len(self.GetActiveNotifications()))
    notification = self.GetActiveNotifications()[0]
    self.assertEquals(self.EMPTY_PAGE_URL, notification['content_url'])
    self.assertEquals('', notification['display_source'])
    self.assertEquals('file:///', notification['origin_url'])

  def testCloseNotification(self):
    """Creates a notification and closes it."""
    self._AllowAllOrigins()
    self.NavigateToURL(self.TEST_PAGE_URL)
    self._CreateHTMLNotification(self.EMPTY_PAGE_URL)
    self.CloseNotification(0)
    self.assertFalse(self.GetActiveNotifications())

  def testCancelNotification(self):
    """Creates a notification and cancels it in the origin page."""
    self._AllowAllOrigins()
    self.NavigateToURL(self.TEST_PAGE_URL)
    note_id = self._CreateHTMLNotification(self.EMPTY_PAGE_URL)
    self.assertNotEquals(-1, note_id)
    self.WaitForNotificationCount(1)
    self._CancelNotification(note_id)
    self.assertFalse(self.GetActiveNotifications())

  def testPermissionInfobarAppears(self):
    """Requests notification privileges and verifies the infobar appears."""
    self.NavigateToURL(self.TEST_PAGE_URL)
    self._RequestPermission()
    self.assertTrue(self.WaitForInfobarCount(1))
    self.assertFalse(self.GetActiveNotifications())
    self._VerifyInfobar('')  # file:/// origins are blank

  def testAllowOnPermissionInfobar(self):
    """Tries to create a notification and clicks allow on the infobar."""
    self.NavigateToURL(self.TEST_PAGE_URL)
    # This notification should not be shown because we do not have permission.
    self._CreateHTMLNotification(self.EMPTY_PAGE_URL)
    self.assertFalse(self.GetActiveNotifications())

    self._RequestPermission()
    self.assertTrue(self.WaitForInfobarCount(1))
    self.PerformActionOnInfobar('accept', 0)
    self._CreateHTMLNotification(self.EMPTY_PAGE_URL)
    self.WaitForNotificationCount(1)

  def testOriginPreferencesBasic(self):
    """Tests that we can allow and deny origins."""
    altavista = 'http://www.altavista.com'
    gmail = 'http://www.gmail.com'
    yahoo = 'http://www.yahoo.com'
    self._SetDeniedOrigins([altavista, gmail])
    self.assertEquals(altavista, self._GetDeniedOrigins()[0])
    self.assertEquals(gmail, self._GetDeniedOrigins()[1])
    self._DenyOrigin(yahoo)
    self.assertEquals(yahoo, self._GetDeniedOrigins()[2])
    self.assertEquals(3, len(self._GetDeniedOrigins()))
    self._DropOriginPreference(gmail)
    self.assertEquals(2, len(self._GetDeniedOrigins()))
    self.assertFalse(gmail in self._GetDeniedOrigins())

    self._AllowOrigin(yahoo)
    self.assertEquals(1, len(self._GetDeniedOrigins()))
    self.assertFalse(yahoo in self._GetDeniedOrigins())
    self.assertTrue(yahoo in self._GetAllowedOrigins())

    self._SetAllowedOrigins([altavista, gmail])
    self._SetDeniedOrigins([])
    self.assertEquals(altavista, self._GetAllowedOrigins()[0])
    self.assertEquals(gmail, self._GetAllowedOrigins()[1])
    self._AllowOrigin(yahoo)
    self.assertEquals(yahoo, self._GetAllowedOrigins()[2])
    self.assertEquals(3, len(self._GetAllowedOrigins()))
    self._DropOriginPreference(gmail)
    self.assertEquals(2, len(self._GetAllowedOrigins()))
    self.assertFalse(gmail in self._GetAllowedOrigins())

    self._DenyOrigin(yahoo)
    self.assertEquals(1, len(self._GetAllowedOrigins()))
    self.assertTrue(yahoo in self._GetDeniedOrigins())
    self.assertFalse(yahoo in self._GetAllowedOrigins())

  def testDenyOnPermissionInfobar (self):
    """Test that no notification is created when Deny is chosen
    from permission infobar."""
    self.NavigateToURL(self.TEST_PAGE_URL)
    self._RequestPermission()
    self.assertTrue(self.WaitForInfobarCount(1))
    self.PerformActionOnInfobar('cancel', 0)
    self._CreateHTMLNotification(self.EMPTY_PAGE_URL)
    self.assertFalse(self.GetActiveNotifications())
    self.assertEquals(['file:///'], self._GetDeniedOrigins())

  def testClosePermissionInfobar(self):
    """Test that no notification is created when permission
    infobar is dismissed."""
    self.NavigateToURL(self.TEST_PAGE_URL)
    self._RequestPermission()
    self.assertTrue(self.WaitForInfobarCount(1))
    self.PerformActionOnInfobar('dismiss', 0)
    self._CreateHTMLNotification(self.EMPTY_PAGE_URL)
    self.assertFalse(self.GetActiveNotifications())
    self.assertFalse(self._GetDeniedOrigins())

  def testNotificationWithPropertyMissing(self):
    """Test that a notification can be created if one property is missing."""
    self._AllowAllOrigins()
    self.NavigateToURL(self.TEST_PAGE_URL)
    self._CreateSimpleNotification('no_such_file.png', 'My Title', '')
    self.assertEquals(1, len(self.GetActiveNotifications()))
    html_data = urllib.unquote(self.GetActiveNotifications()[0]['content_url'])
    self.assertTrue('no_such_file.png' in html_data)
    self.assertTrue('My Title' in html_data)

  def testAllowNotificationsFromAllSites(self):
    """Verify that all domains can be allowed to show notifications."""
    self._SetDefaultPermissionSetting(self.ALLOW_ALL_SETTING)
    self.NavigateToURL(self.TEST_PAGE_URL)
    self._CreateHTMLNotification(self.EMPTY_PAGE_URL)
    self.assertEquals(1, len(self.GetActiveNotifications()))
    self.assertFalse(self.GetBrowserInfo()['windows'][0]['tabs'][0]['infobars'])

  def testDenyNotificationsFromAllSites(self):
    """Verify that no domain can show notifications."""
    self._SetDefaultPermissionSetting(self.DENY_ALL_SETTING)
    self.NavigateToURL(self.TEST_PAGE_URL)
    self._CreateHTMLNotification(self.EMPTY_PAGE_URL)
    self.assertFalse(self.GetActiveNotifications())

  def testDenyDomainAndAllowAll(self):
    """Verify that denying a domain and allowing all shouldn't show
    notifications from the denied domain."""
    self._DenyOrigin('file:///')
    self._SetDefaultPermissionSetting(self.ALLOW_ALL_SETTING)
    self.NavigateToURL(self.TEST_PAGE_URL)
    self._CreateHTMLNotification(self.EMPTY_PAGE_URL)
    self.assertFalse(self.GetActiveNotifications())

  def testAllowDomainAndDenyAll(self):
    """Verify that allowing a domain and denying all others should show
    notifications from the allowed domain."""
    self._AllowOrigin('file:///')
    self._SetDefaultPermissionSetting(self.DENY_ALL_SETTING)
    self.NavigateToURL(self.TEST_PAGE_URL)
    self._CreateHTMLNotification(self.EMPTY_PAGE_URL)
    self.assertEquals(1, len(self.GetActiveNotifications()))
    self.assertFalse(self.GetBrowserInfo()['windows'][0]['tabs'][0]['infobars'])

  def testDenyAndThenAllowDomain(self):
    """Verify that denying and again allowing should show notifications."""
    self._DenyOrigin('file:///')
    self.NavigateToURL(self.TEST_PAGE_URL)
    self._CreateHTMLNotification(self.EMPTY_PAGE_URL)
    self.assertEquals(len(self.GetActiveNotifications()), 0)
    self._AllowOrigin('file:///')
    self._CreateHTMLNotification(self.EMPTY_PAGE_URL)
    self.assertEquals(1, len(self.GetActiveNotifications()))
    self.assertFalse(self.GetBrowserInfo()['windows'][0]['tabs'][0]['infobars'])

  def testCreateDenyCloseNotifications(self):
    """Verify able to create, deny, and close the notification."""
    self._AllowAllOrigins()
    self.NavigateToURL(self.TEST_PAGE_URL)
    self._CreateHTMLNotification(self.EMPTY_PAGE_URL)
    self.assertEquals(1, len(self.GetActiveNotifications()))
    origin = 'file:///'
    self._DenyOrigin(origin)
    self.assertTrue(origin in self._GetDeniedOrigins())
    self.CloseNotification(0)
    self.assertEquals(0, len(self.GetActiveNotifications()))

  def testOriginPrefsNotSavedInIncognito(self):
    """Verify that allow/deny origin preferences are not saved in incognito."""
    self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
    self.NavigateToURL(self.TEST_PAGE_URL, 1, 0)
    self._RequestPermission(windex=1)
    self.assertTrue(self.WaitForInfobarCount(1, windex=1))
    self.PerformActionOnInfobar('cancel', 0, windex=1)

    self.CloseBrowserWindow(1)
    self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
    self.NavigateToURL(self.TEST_PAGE_URL, 1, 0)
    self._RequestPermission(windex=1)
    self.assertTrue(self.WaitForInfobarCount(1, windex=1))
    self.PerformActionOnInfobar('accept', 0, windex=1)
    self._CreateHTMLNotification(self.EMPTY_PAGE_URL, windex=1)
    self.assertEquals(1, len(self.GetActiveNotifications()))

    self.CloseBrowserWindow(1)
    self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
    self.NavigateToURL(self.TEST_PAGE_URL, 1, 0)
    self._RequestPermission(windex=1)
    self.assertTrue(self.WaitForInfobarCount(1, windex=1))

    self.assertFalse(self._GetDeniedOrigins())
    self.assertFalse(self._GetAllowedOrigins())

  def testExitBrowserWithInfobar(self):
    """Exit the browser window, when the infobar appears."""
    self.NavigateToURL(self.TEST_PAGE_URL)
    self._RequestPermission()
    self.assertTrue(self.WaitForInfobarCount(1))

  def testCrashTabWithPermissionInfobar(self):
    """Test crashing the tab with permission infobar doesn't crash Chrome."""
    self.AppendTab(pyauto.GURL(self.EMPTY_PAGE_URL))
    self.assertTrue(self.ActivateTab(0))
    self.NavigateToURL(self.TEST_PAGE_URL)
    self._RequestPermission()
    self.assertTrue(self.WaitForInfobarCount(1))
    self.KillRendererProcess(
        self.GetBrowserInfo()['windows'][0]['tabs'][0]['renderer_pid'])

  def testKillNotificationProcess(self):
    """Test killing a notification doesn't crash Chrome."""
    self._AllowAllOrigins()
    self.NavigateToURL(self.TEST_PAGE_URL)
    self._CreateHTMLNotification(self.EMPTY_PAGE_URL)
    self.KillRendererProcess(self.GetActiveNotifications()[0]['pid'])
    self.WaitForNotificationCount(0)

  def testIncognitoNotification(self):
    """Test notifications in incognito window."""
    self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
    self.NavigateToURL(self.TEST_PAGE_URL, 1, 0)
    self.assertTrue(self.ActivateTab(0, 1))
    self._RequestPermission(windex=1)
    self.assertTrue(self.WaitForInfobarCount(1, windex=1))
    self.PerformActionOnInfobar('accept', infobar_index=0, windex=1)
    self._CreateHTMLNotification(self.EMPTY_PAGE_URL, windex=1)
    self.assertEquals(1, len(self.GetActiveNotifications()))

  def testSpecialURLNotification(self):
    """Test a page cannot create a notification to a chrome: url."""
    self._AllowAllOrigins()
    self.NavigateToURL(self.TEST_PAGE_URL)
    self._CreateHTMLNotification('chrome://settings',
                                 wait_for_display=False);
    self.assertFalse(self.GetActiveNotifications())

  def testCloseTabWithPermissionInfobar(self):
    """Test that user can close tab when infobar present."""
    self.AppendTab(pyauto.GURL('about:blank'))
    self.ActivateTab(0)
    self.NavigateToURL(self.TEST_PAGE_URL)
    self._RequestPermission()
    self.assertTrue(self.WaitForInfobarCount(1))
    self.GetBrowserWindow(0).GetTab(0).Close(True)

  def testNavigateAwayWithPermissionInfobar(self):
    """Test navigating away when an infobar is present, then trying to create a
    notification from the same page."""
    self.AppendTab(pyauto.GURL('about:blank'))
    self.assertTrue(self.ActivateTab(0))
    self.NavigateToURL(self.TEST_PAGE_URL)
    self._RequestPermission()
    self.assertTrue(self.WaitForInfobarCount(1))
    self.NavigateToURL(self.TEST_PAGE_URL)
    self._RequestPermission()
    self.assertTrue(self.WaitForInfobarCount(1))
    self.PerformActionOnInfobar('accept', 0)
    self._CreateHTMLNotification(self.EMPTY_PAGE_URL)
    self.assertEquals(1, len(self.GetActiveNotifications()))

  def testCrashRendererNotificationRemain(self):
    """Test crashing renderer does not close or crash notification."""
    self._AllowAllOrigins()
    self.AppendTab(pyauto.GURL('about:blank'))
    self.ActivateTab(0)
    self.NavigateToURL(self.TEST_PAGE_URL)
    self._CreateHTMLNotification(self.EMPTY_PAGE_URL)
    self.assertEquals(1, len(self.GetActiveNotifications()))
    self.KillRendererProcess(
        self.GetBrowserInfo()['windows'][0]['tabs'][0]['renderer_pid'])
    self.assertEquals(1, len(self.GetActiveNotifications()))

  def testNotificationOrderAfterClosingOne(self):
    """Tests that closing a notification leaves the rest
    of the notifications in the correct order.
    """
    if self.IsWin7():
      return  # crbug.com/66072
    self._AllowAllOrigins()
    self.NavigateToURL(self.TEST_PAGE_URL)
    self._CreateSimpleNotification('', 'Title1', '')
    self._CreateSimpleNotification('', 'Title2', '')
    self._CreateSimpleNotification('', 'Title3', '')
    old_notifications = self.GetAllNotifications()
    self.assertEquals(3, len(old_notifications))
    self.CloseNotification(1)
    new_notifications = self.GetAllNotifications()
    self.assertEquals(2, len(new_notifications))
    self.assertEquals(old_notifications[0]['id'], new_notifications[0]['id'])
    self.assertEquals(old_notifications[2]['id'], new_notifications[1]['id'])

  def testNotificationReplacement(self):
    """Test that we can replace a notification using the replaceId."""
    self._AllowAllOrigins()
    self.NavigateToURL(self.TEST_PAGE_URL)
    self._CreateSimpleNotification('', 'Title2', '', 'chat')
    self.WaitForNotificationCount(1)
    # Since this notification has the same replaceId, 'chat', it should replace
    # the first notification.
    self._CreateHTMLNotification(self.EMPTY_PAGE_URL, 'chat')
    notifications = self.GetActiveNotifications()
    self.assertEquals(1, len(notifications))
    self.assertEquals(self.EMPTY_PAGE_URL, notifications[0]['content_url'])


if __name__ == '__main__':
  pyauto_functional.Main()
