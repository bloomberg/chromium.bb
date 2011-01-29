#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pyauto_functional  # Must be imported before pyauto
import pyauto
import test_utils


class PasswordTest(pyauto.PyUITest):
  """Tests that passwords work correctly."""

  def Debug(self):
    """Test method for experimentation.

    This method will not run automatically.
    """
    while True:
      raw_input('Interact with the browser and hit <enter> to dump passwords. ')
      print '*' * 20
      import pprint
      pp = pprint.PrettyPrinter(indent=2)
      pp.pprint(self.GetSavedPasswords())

  def _AssertWithinOneSecond(self, time1, time2):
    self.assertTrue(abs(time1 - time2) < 1.0,
                    'Times not within an acceptable range. '
                    'First was %lf, second was %lf' % (time1, time2))

  def _ConstructPasswordDictionary(self, username_value, password_value,
                                   signon_realm, origin_url, username_element,
                                   password_element, action_target,
                                   time=1279650942.0, submit_element='submit',
                                   blacklist=False):
    """Construct a password dictionary with all the required fields."""
    return {'username_value': username_value,
            'password_value': password_value,
            'signon_realm': signon_realm,
            'time': time,
            'origin_url': origin_url,
            'username_element': username_element,
            'password_element': password_element,
            'submit_element': submit_element,
            'action_target': action_target,
            'blacklist': blacklist}

  def testSavePassword(self):
    """Test saving a password and getting saved passwords."""
    password1 = self._ConstructPasswordDictionary(
        'user@example.com', 'test.password',
        'https://www.example.com/', 'https://www.example.com/login',
        'username', 'password', 'https://www.example.com/login/')
    self.assertTrue(self.AddSavedPassword(password1))
    self.assertEquals(self.GetSavedPasswords(), [password1])

  def testRemovePasswords(self):
    """Verify that saved passwords can be removed."""
    password1 = self._ConstructPasswordDictionary(
        'user1@example.com', 'test1.password',
        'https://www.example.com/', 'https://www.example.com/login',
        'username1', 'password', 'https://www.example.com/login/')
    password2 = self._ConstructPasswordDictionary(
        'user2@example.com', 'test2.password',
        'https://www.example.com/', 'https://www.example.com/login',
        'username2', 'password2', 'https://www.example.com/login/')
    self.AddSavedPassword(password1)
    self.AddSavedPassword(password2)
    self.assertEquals(2, len(self.GetSavedPasswords()))
    self.assertEquals([password1, password2], self.GetSavedPasswords())
    self.RemoveSavedPassword(password1)
    self.assertEquals(1, len(self.GetSavedPasswords()))
    self.assertEquals([password2], self.GetSavedPasswords())
    self.RemoveSavedPassword(password2)
    # TODO: GetSavedPasswords() doesn't return anything when empty.
    # http://crbug.com/64603
    # self.assertFalse(self.GetSavedPasswords())

  def testDisplayAndSavePasswordInfobar(self):
    """Verify password infobar displays and able to save password."""
    test_utils.ClearPasswords(self)
    url_https = 'https://www.google.com/accounts/'
    url_logout = 'https://www.google.com/accounts/Logout'
    creds = self.GetPrivateInfo()['test_google_account']
    username = creds['username']
    password = creds['password']
    test_utils.GoogleAccountsLogin(self, username, password)
    # Wait until page completes loading.
    self.WaitUntil(
        lambda: self.GetDOMValue('document.readyState'),
        expect_retval='complete')
    self.assertTrue(self.WaitForInfobarCount(1),
                    'Save password infobar did not appear.')
    infobar = self.GetBrowserInfo()['windows'][0]['tabs'][0]['infobars']
    self.assertEqual(infobar[0]['type'], 'confirm_infobar')
    self.PerformActionOnInfobar('accept', infobar_index=0)
    self.NavigateToURL(url_logout)
    self.AppendTab(pyauto.GURL(url_https))  # New tab to avoid bug 70694
    # Wait until accounts page load to detect value in username field.
    self.WaitUntil(lambda: self.GetDOMValue('document.readyState', 0, 1),
                   expect_retval='complete')
    test_utils.VerifyGoogleAccountCredsFilled(self, username, password,
                                              tab_index=1, windex=0)
    self.ExecuteJavascript('document.getElementById("gaia_loginform").submit();'
                           'window.domAutomationController.send("done")',
                           0, 1)
    test_utils.ClearPasswords(self)

  def testNeverSavePasswords(self):
    """Verify that we don't save passwords and delete saved passwords
    for a domain when 'never for this site' is chosen."""
    creds1 = self.GetPrivateInfo()['test_google_account']
    test_utils.GoogleAccountsLogin(
        self, creds1['username'], creds1['password'])
    self.assertTrue(self.WaitForInfobarCount(1))
    self.PerformActionOnInfobar('accept', infobar_index=0)
    self.assertEquals(1, len(self.GetSavedPasswords()))
    self.AppendTab(pyauto.GURL(creds1['logout_url']))
    creds2 = self.GetPrivateInfo()['test_google_account_2']
    test_utils.GoogleAccountsLogin(
        self, creds2['username'], creds2['password'], tab_index=1)
    self.assertTrue(self.WaitForInfobarCount(1, tab_index=1))
    # Selecting 'Never for this site' option on password infobar.
    self.PerformActionOnInfobar('cancel', infobar_index=0, tab_index=1)

    # TODO: GetSavedPasswords() doesn't return anything when empty.
    # http://crbug.com/64603
    # self.assertFalse(self.GetSavedPasswords())
    # TODO: Check the exceptions list

  def testSavedPasswordInTabsAndWindows(self):
    """Verify saved username/password displays in Regular/Incognito Window
       and NTP"""
    username = 'test'
    password = 'test12345'
    password_dict = {
    u'action_target': u'https://www.google.com/accounts/ServiceLoginAuth',
    u'blacklist': False,
    u'origin_url': u'https://www.google.com/accounts/ServiceLogin',
    u'password_element': u'Passwd',
    u'password_value': u'test12345',
    u'signon_realm': u'https://www.google.com/',
    u'submit_element': u'',
    u'time': 1280939865.0,
    u'username_element': u'Email',
    u'username_value': u'test'}
    url = 'https://www.google.com/accounts/ServiceLogin'
    self.AddSavedPassword(password_dict)
    self.NavigateToURL(url)
    test_utils.VerifyGoogleAccountCredsFilled(self, username, password,
        tab_index=0, windex=0)
    self.AppendTab(pyauto.GURL(url))
    test_utils.VerifyGoogleAccountCredsFilled(self, username, password,
        tab_index=1, windex=0)
    self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
    self.NavigateToURL(url, 1, 0)
    test_utils.VerifyGoogleAccountCredsFilled(self, username, password,
        tab_index=0, windex=1)
    test_utils.ClearPasswords(self)

  def testInfoBarDisappearByNavigatingPage(self):
    """Test that Password infobar is dismissed by navigating to
       different page."""
    creds = self.GetPrivateInfo()['test_google_account']
    # Login to Google a/c
    test_utils.GoogleAccountsLogin(self, creds['username'], creds['password'])
    # Wait for the infobar to appear
    self.assertTrue(self.WaitForInfobarCount(1))
    self.assertTrue(self.GetBrowserInfo()['windows'][0]['tabs'][0]['infobars'])
    self.NavigateToURL('chrome://history')
    self.assertTrue(self.WaitForInfobarCount(0))
    # To make sure user is navigated to History page.
    self.assertEqual('History', self.GetActiveTabTitle())
    self.assertFalse(self.GetBrowserInfo()['windows'][0]['tabs'][0]['infobars'])

  def testInfoBarDisappearByReload(self):
    """Test that Password infobar disappears by the page reload."""
    creds = self.GetPrivateInfo()['test_google_account']
    # Login to Google a/c
    test_utils.GoogleAccountsLogin(self, creds['username'], creds['password'])
    # Wait for the infobar to appear
    self.assertTrue(self.WaitForInfobarCount(1))
    self.assertTrue(self.GetBrowserInfo()['windows'][0]['tabs'][0]['infobars'])
    self.GetBrowserWindow(0).GetTab(0).Reload()
    self.assertTrue(self.WaitForInfobarCount(0))
    self.assertFalse(self.GetBrowserInfo()['windows'][0]['tabs'][0]['infobars'])


if __name__ == '__main__':
  pyauto_functional.Main()
