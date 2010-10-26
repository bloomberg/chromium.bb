#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import urllib

import pyauto_functional
import pyauto


class NotificationsTest(pyauto.PyUITest):
  """Test of HTML5 desktop notifications."""
  def __init__(self, methodName='runTest'):
    super(NotificationsTest, self).__init__(methodName)
    self.NO_SUCH_URL = 'http://no_such_url_exists/'
    # Content settings for default notification permission.
    self.ALLOW_ALL_SETTING = 1
    self.DENY_ALL_SETTING = 2
    self.ASK_SETTING = 3

    # HTML page used for notification testing.
    self.TEST_PAGE_URL = (
        self.GetFileURLForDataPath('notifications/notification_tester.html'))

  def Debug(self):
    """Test method for experimentation.

    This method will not run automatically.
    """
    while True:
      raw_input('Interact with the browser and hit <enter> to dump notification'
                'state...')
      print '*' * 20
      import pprint
      pp = pprint.PrettyPrinter(indent=2)
      pp.pprint(self.GetActiveNotifications())

  def _SetDefaultPermissionSetting(self, setting):
    """Sets the default setting for whether sites are allowed to create
    notifications.
    """
    self.SetPrefs(pyauto.kDesktopNotificationDefaultContentSetting, setting)

  def _GetDefaultPermissionSetting(self):
    """Gets the default setting for whether sites are allowed to create
    notifications.
    """
    return self.GetPrefsInfo().Prefs(
        pyauto.kDesktopNotificationDefaultContentSetting)

  def _GetDeniedOrigins(self):
    """Gets the list of origins that are explicitly denied to create
    notifications.
    """
    return self.GetPrefsInfo().Prefs(pyauto.kDesktopNotificationDeniedOrigins)

  def _GetAllowedOrigins(self):
    """Gets the list of origins that are explicitly allowed to create
    notifications.
    """
    return self.GetPrefsInfo().Prefs(pyauto.kDesktopNotificationAllowedOrigins)

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
    denied = self._GetDeniedOrigins() or []
    if new_origin not in denied:
      self._SetDeniedOrigins(denied + [new_origin])

  def _AllowOrigin(self, new_origin):
    """Allows the given origin to create notifications. If it was explicitly
    denied, that preference is dropped.
    """
    self._DropOriginPreference(new_origin)
    allowed = self._GetAllowedOrigins() or []
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

  def _CallJavascriptFunc(self, function, args=[], tab_index=0, windex=0):
    """Helper function to execute a script that calls a given function.

    Defaults to first tab in first window.

    Args:
      function: name of the function
      args: list of all the arguments to pass into the called function. These
            should be able to be converted to a string using the |str| function.
      tab_index: index of the tab within the given window
      windex: index of the window
    """
    # Convert the given arguments for evaluation in a javascript statement.
    converted_args = []
    for arg in args:
      # If it is a string argument, we need to quote and escape it properly.
      if type(arg) == type('string') or type(arg) == type(u'unicode'):
        # We must convert all " in the string to \", so that we don't try
        # to evaluate invalid javascript like ""arg"".
        converted_arg = '"' + arg.replace('"', '\\"') + '"'
      else:
        # Convert it to a string so that we can use |join| later.
        converted_arg = str(arg)
      converted_args += [converted_arg]
    js = '%s(%s)' % (function, ', '.join(converted_args))
    return self.ExecuteJavascript(js, windex, tab_index)

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
    return self._CallJavascriptFunc('createNotification',
                                    [img_url, title, text, replace_id],
                                    tab_index,
                                    windex);

  def _CreateHTMLNotification(self, content_url, replace_id='', tab_index=0,
                              windex=0):
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
      tab_index: index of the tab within the given window
      windex: index of the window
    """
    return self._CallJavascriptFunc('createHTMLNotification',
                                    [content_url, replace_id],
                                    tab_index,
                                    windex)

  def _RequestPermission(self, tab_index=0, windex=0):
    """Requests permission to create notifications.

    This will only work if the current page is navigated to |TEST_PAGE_URL|.

    Args:
      tab_index: index of the tab within the given window
      windex: index of the window
    """
    self._CallJavascriptFunc('requestPermission', [], windex, tab_index)

  def _CancelNotification(self, notification_id, tab_index=0, windex=0):
    """Cancels a notification with the given id.

    This canceling is done in the page that showed that notification and so
    follows a different path than closing a notification via the UI.

    A notification can be canceled even if it has not been shown yet.
    This will only work if the page is navigated to |TEST_PAGE_URL|.

    Args:
      tab_index: index of the tab within the given window that created the
          notification
      windex: index of the window
    """
    self._CallJavascriptFunc(
        'cancelNotification', [notification_id], tab_index, windex)

  def testCreateSimpleNotification(self):
    """Creates a simple notification."""
    self._AllowAllOrigins()
    self.NavigateToURL(self.TEST_PAGE_URL)
    self._CreateSimpleNotification('no_such_file.png', 'My Title', 'My Body')
    self.assertEquals(len(self.GetActiveNotifications()), 1)
    notification = self.GetActiveNotifications()[0]
    html_data = urllib.unquote(notification['content_url'])
    self.assertTrue('no_such_file.png' in html_data)
    self.assertTrue('My Title' in html_data)
    self.assertTrue('My Body' in html_data)

  def testCreateHTMLNotification(self):
    """Creates an HTML notification using a fake url."""
    self._AllowAllOrigins()
    self.NavigateToURL(self.TEST_PAGE_URL)
    self._CreateHTMLNotification(self.NO_SUCH_URL)
    self.assertEquals(len(self.GetActiveNotifications()), 1)
    notification = self.GetActiveNotifications()[0]
    self.assertEquals(self.NO_SUCH_URL, notification['content_url'])
    self.assertEquals('', notification['display_source'])
    self.assertEquals('file:///', notification['origin_url'])

  def testCloseNotification(self):
    """Creates a notification and closes it."""
    self._AllowAllOrigins()
    self.NavigateToURL(self.TEST_PAGE_URL)
    self._CreateHTMLNotification(self.NO_SUCH_URL)
    self.CloseNotification(0)
    self.assertEquals(len(self.GetActiveNotifications()), 0)

  def testCancelNotification(self):
    """Creates a notification and cancels it in the origin page."""
    self._AllowAllOrigins()
    self.NavigateToURL(self.TEST_PAGE_URL)
    note_id = self._CreateHTMLNotification(self.NO_SUCH_URL)
    self.assertNotEquals(-1, note_id)
    self._CancelNotification(note_id)
    self.assertEquals(len(self.GetActiveNotifications()), 0)

  def testPermissionInfobarAppears(self):
    """Requests notification privileges and verifies the infobar appears."""
    self.NavigateToURL(self.TEST_PAGE_URL)
    self._RequestPermission()
    self.assertTrue(self.WaitForInfobarCount(1))
    self.assertEquals(len(self.GetActiveNotifications()), 0)
    self._VerifyInfobar('')  # file:/// origins are blank

  def testAllowOnPermissionInfobar(self):
    """Tries to create a notification and clicks allow on the infobar."""
    self.NavigateToURL(self.TEST_PAGE_URL)
    # This notification should not be shown because we don't have permission.
    self._CreateHTMLNotification(self.NO_SUCH_URL)
    self.assertEquals(len(self.GetActiveNotifications()), 0)

    self._RequestPermission()
    self.assertTrue(self.WaitForInfobarCount(1))
    self.PerformActionOnInfobar('accept', infobar_index=0)
    self._CreateHTMLNotification(self.NO_SUCH_URL)
    self.WaitForNotificationCount(1)

  def testOriginPreferencesBasic(self):
    """Tests that we can allow and deny origins."""
    altavista = 'www.altavista.com'
    gmail = 'www.gmail.com'
    yahoo = 'www.yahoo.com'
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


if __name__ == '__main__':
  pyauto_functional.Main()
