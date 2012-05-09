#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

import pyauto_functional  # Must be imported before pyauto
import pyauto
import test_utils
from webdriver_pages import settings


class PasswordTest(pyauto.PyUITest):
  """Tests that passwords work correctly."""

  INFOBAR_TYPE = 'password_infobar'
  URL = 'https://www.google.com/accounts/ServiceLogin'
  URL_HTTPS = 'https://www.google.com/accounts/Login'
  URL_LOGOUT = 'https://www.google.com/accounts/Logout'

  def Debug(self):
    """Test method for experimentation.

    This method will not run automatically.
    """
    while True:
      raw_input('Interact with the browser and hit <enter> to dump passwords. ')
      print '*' * 20
      self.pprint(self.GetSavedPasswords())

  def setUp(self):
    pyauto.PyUITest.setUp(self)
    self.assertFalse(self.GetSavedPasswords())

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

  def _ClickOnLoginPage(self, window_index, tab_index):
    # In some cases (such as on Windows) the current page displays an account
    # name and e-mail, rather than an e-mail and password.  Clicking on a
    # particular DOM element causes the e-mail and password to be displayed.
    click_js = """
      var elements = document.getElementsByClassName("accounts");
      if (elements && elements.length > 0) {
        elements = elements[0].getElementsByTagName("p");
        if (elements && elements.length > 0)
          elements[0].onclick();
      }
      window.domAutomationController.send("done");
    """
    self.ExecuteJavascript(click_js, tab_index, window_index)

    # Wait until username/password is filled by the Password manager on the
    # login page.
    js_template = """
      var value = "";
      var element = document.getElementById("%s");
      if (element)
        value = element.value;
      window.domAutomationController.send(value);
    """
    self.assertTrue(self.WaitUntil(
        lambda: self.ExecuteJavascript(js_template % 'Email',
                                      tab_index, window_index) != '' and
                self.ExecuteJavascript(js_template % 'Passwd',
                                      tab_index, window_index) != ''))

  def testSavePassword(self):
    """Test saving a password and getting saved passwords."""
    password1 = self._ConstructPasswordDictionary(
        'user@example.com', 'test.password',
        'https://www.example.com/', 'https://www.example.com/login',
        'username', 'password', 'https://www.example.com/login/')
    self.assertTrue(self.AddSavedPassword(password1))
    self.assertEqual(self.GetSavedPasswords(), [password1])

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
    creds = self.GetPrivateInfo()['test_google_account']
    username = creds['username']
    password = creds['password']
    # Disable one-click login infobar for sync.
    self.SetPrefs(pyauto.kReverseAutologinEnabled, False)
    test_utils.GoogleAccountsLogin(self, username, password)
    # Wait until page completes loading.
    self.WaitUntil(
        lambda: self.GetDOMValue('document.readyState'),
        expect_retval='complete')
    self.PerformActionOnInfobar(
        'accept', infobar_index=test_utils.WaitForInfobarTypeAndGetIndex(
            self, self.INFOBAR_TYPE))
    self.NavigateToURL(self.URL_LOGOUT)
    self.NavigateToURL(self.URL_HTTPS)
    self._ClickOnLoginPage(0, 0)
    test_utils.VerifyGoogleAccountCredsFilled(self, username, password,
                                              tab_index=0, windex=0)
    test_utils.ClearPasswords(self)

  def testNeverSavePasswords(self):
    """Verify passwords not saved/deleted when 'never for this site' chosen."""
    creds1 = self.GetPrivateInfo()['test_google_account']
    # Disable one-click login infobar for sync.
    self.SetPrefs(pyauto.kReverseAutologinEnabled, False)
    test_utils.GoogleAccountsLogin(
        self, creds1['username'], creds1['password'])
    self.PerformActionOnInfobar(
        'accept', infobar_index=test_utils.WaitForInfobarTypeAndGetIndex(
            self, self.INFOBAR_TYPE))
    self.assertEquals(1, len(self.GetSavedPasswords()))
    self.AppendTab(pyauto.GURL(creds1['logout_url']))
    creds2 = self.GetPrivateInfo()['test_google_account_2']
    test_utils.GoogleAccountsLogin(
        self, creds2['username'], creds2['password'], tab_index=1)
    # Selecting 'Never for this site' option on password infobar.
    self.PerformActionOnInfobar(
        'cancel', infobar_index=test_utils.WaitForInfobarTypeAndGetIndex(
            self, self.INFOBAR_TYPE, tab_index=1), tab_index=1)

    # TODO: GetSavedPasswords() doesn't return anything when empty.
    # http://crbug.com/64603
    # self.assertFalse(self.GetSavedPasswords())
    # TODO: Check the exceptions list

  def testSavedPasswordInTabsAndWindows(self):
    """Verify saved username/password shows in window and tab."""
    creds = self.GetPrivateInfo()['test_google_account']
    username = creds['username']
    password = creds['password']
    # Disable one-click login infobar for sync.
    self.SetPrefs(pyauto.kReverseAutologinEnabled, False)
    # Login to Google a/c
    test_utils.GoogleAccountsLogin(self, username, password)
    self.PerformActionOnInfobar(
        'accept', infobar_index=test_utils.WaitForInfobarTypeAndGetIndex(
            self, self.INFOBAR_TYPE))
    self.NavigateToURL(self.URL_LOGOUT)
    self.NavigateToURL(self.URL)
    self._ClickOnLoginPage(0, 0)
    test_utils.VerifyGoogleAccountCredsFilled(self, username, password,
        tab_index=0, windex=0)
    self.AppendTab(pyauto.GURL(self.URL))
    self._ClickOnLoginPage(0, 1)
    test_utils.VerifyGoogleAccountCredsFilled(self, username, password,
        tab_index=1, windex=0)
    test_utils.ClearPasswords(self)

  def testLoginCredsNotShownInIncognito(self):
    """Verify login creds are not shown in Incognito mode."""
    creds = self.GetPrivateInfo()['test_google_account']
    username = creds['username']
    password = creds['password']
    # Disable one-click login infobar for sync.
    self.SetPrefs(pyauto.kReverseAutologinEnabled, False)
    # Login to Google account.
    test_utils.GoogleAccountsLogin(self, username, password)
    self.PerformActionOnInfobar(
        'accept', infobar_index=test_utils.WaitForInfobarTypeAndGetIndex(
            self, self.INFOBAR_TYPE))
    self.NavigateToURL(self.URL_LOGOUT)
    self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
    self.NavigateToURL(self.URL, 1, 0)
    email_value = self.GetDOMValue('document.getElementById("Email").value',
                                   tab_index=0, windex=1)
    passwd_value = self.GetDOMValue('document.getElementById("Passwd").value',
                                    tab_index=0, windex=1)
    self.assertEqual(email_value, '',
                    msg='Email creds displayed %s.' % email_value)
    self.assertEqual(passwd_value, '', msg='Password creds displayed.')

  def testPasswordAutofilledInIncognito(self):
    """Verify saved password is autofilled in Incognito mode.

    Saved passwords should be autofilled once the username is entered in
    incognito mode.
    """
    self._driver = self.NewWebDriver()
    username = 'test@google.com'
    password = 'test.password'
    password_dict = self._ConstructPasswordDictionary(
        username, password,
        'https://www.google.com/', 'https://www.google.com/accounts',
        'Email', 'Passwd', 'https://www.google.com/accounts')
    self.AddSavedPassword(password_dict)
    self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
    self.NavigateToURL(self.URL, 1, 0)
    # Switch to window 1.
    self._driver.switch_to_window(self._driver.window_handles[1])
    self._driver.find_element_by_id('Email').send_keys(username + '\t')
    incognito_passwd = self.GetDOMValue(
        'document.getElementById("Passwd").value', tab_index=0, windex=1)
    self.assertEqual(incognito_passwd, password,
                    msg='Password creds did not autofill in incognito mode.')

  def testInfoBarDisappearByNavigatingPage(self):
    """Test password infobar is dismissed when navigating to different page."""
    creds = self.GetPrivateInfo()['test_google_account']
    # Disable one-click login infobar for sync.
    self.SetPrefs(pyauto.kReverseAutologinEnabled, False)
    # Login to Google account.
    test_utils.GoogleAccountsLogin(self, creds['username'], creds['password'])
    self.PerformActionOnInfobar(
        'accept', infobar_index=test_utils.WaitForInfobarTypeAndGetIndex(
            self, self.INFOBAR_TYPE))
    self.NavigateToURL('chrome://version')
    self.assertTrue(self.WaitForInfobarCount(0))
    # To make sure user is navigated to Version page.
    self.assertEqual('About Version', self.GetActiveTabTitle())
    test_utils.AssertInfobarTypeDoesNotAppear(self, self.INFOBAR_TYPE)

  def testInfoBarDisappearByReload(self):
    """Test that Password infobar disappears by the page reload."""
    creds = self.GetPrivateInfo()['test_google_account']
    # Disable one-click login infobar for sync.
    self.SetPrefs(pyauto.kReverseAutologinEnabled, False)
    # Login to Google a/c
    test_utils.GoogleAccountsLogin(self, creds['username'], creds['password'])
    self.PerformActionOnInfobar(
        'accept', infobar_index=test_utils.WaitForInfobarTypeAndGetIndex(
            self, self.INFOBAR_TYPE))
    self.GetBrowserWindow(0).GetTab(0).Reload()
    test_utils.AssertInfobarTypeDoesNotAppear(self, self.INFOBAR_TYPE)

  def testPasswdInfoNotStoredWhenAutocompleteOff(self):
    """Verify that password infobar does not appear when autocomplete is off.

    If the password field has autocomplete turned off, then the password infobar
    should not offer to save the password info.
    """
    password_info = {'Email': 'test@google.com',
                     'Passwd': 'test12345'}

    # Disable one-click login infobar for sync.
    self.SetPrefs(pyauto.kReverseAutologinEnabled, False)
    url = self.GetHttpURLForDataPath(
        os.path.join('password', 'password_autocomplete_off_test.html'))
    self.NavigateToURL(url)
    for key, value in password_info.iteritems():
      script = ('document.getElementById("%s").value = "%s"; '
                'window.domAutomationController.send("done");') % (key, value)
      self.ExecuteJavascript(script, 0, 0)
    self.assertTrue(self.SubmitForm('loginform'))
    test_utils.AssertInfobarTypeDoesNotAppear(self, self.INFOBAR_TYPE)

  def _SendCharToPopulateField(self, char, tab_index=0, windex=0):
    """Simulate a char being typed into a field.

    Args:
      char: the char value to be typed into the field.
      tab_index: tab index to work on. Defaults to 0 (first tab).
      windex: window index to work on. Defaults to 0 (first window).
    """
    CHAR_KEYPRESS = ord((char).upper())  # ASCII char key press.
    KEY_DOWN_TYPE = 0  # kRawKeyDownType
    KEY_UP_TYPE = 3  # kKeyUpType

    self.SendWebkitKeyEvent(KEY_DOWN_TYPE, CHAR_KEYPRESS, tab_index, windex)
    self.SendWebkitCharEvent(char, tab_index, windex)
    self.SendWebkitKeyEvent(KEY_UP_TYPE, CHAR_KEYPRESS, tab_index, windex)

  def testClearFetchedCredForNewUserName(self):
    """Verify that the fetched credentials are cleared for a new username.

    This test requires sending key events rather than pasting a new username
    into the Email field.
    """
    creds = self.GetPrivateInfo()['test_google_account']
    username = creds['username']
    password = creds['password']
    # Disable one-click login infobar for sync.
    self.SetPrefs(pyauto.kReverseAutologinEnabled, False)
    # Login to Google a/c
    test_utils.GoogleAccountsLogin(self, username, password)
    self.PerformActionOnInfobar(
        'accept', infobar_index=test_utils.WaitForInfobarTypeAndGetIndex(
            self, self.INFOBAR_TYPE))
    self.NavigateToURL(self.URL_LOGOUT)
    self.NavigateToURL(self.URL)
    self._ClickOnLoginPage(0, 0)
    test_utils.VerifyGoogleAccountCredsFilled(self, username, password,
        tab_index=0, windex=0)
    clear_username_field = (
        'document.getElementById("Email").value = ""; '
        'window.domAutomationController.send("done");')
    set_focus = (
        'document.getElementById("Email").focus(); '
        'window.domAutomationController.send("done");')
    self.ExecuteJavascript(clear_username_field, 0, 0)
    self.ExecuteJavascript(set_focus, 0, 0)
    self._SendCharToPopulateField('t', tab_index=0, windex=0)
    passwd_value = self.GetDOMValue('document.getElementById("Passwd").value')
    self.assertFalse(passwd_value,
                     msg='Password field not empty for new username.')
    test_utils.ClearPasswords(self)

  def testPasswordInfobarShowsForBlockedDomain(self):
    """Verify that password infobar shows when cookies are blocked.

    Password infobar should be shown if cookies are blocked for Google
    accounts domain.
    """
    creds = self.GetPrivateInfo()['test_google_account']
    username = creds['username']
    password = creds['password']
    # Block cookies for Google accounts domain.
    self.SetPrefs(pyauto.kContentSettingsPatternPairs,
                  {'https://accounts.google.com/': {'cookies': 2}})
    test_utils.GoogleAccountsLogin(self, username, password)
    test_utils.WaitForInfobarTypeAndGetIndex(self, self.INFOBAR_TYPE)


if __name__ == '__main__':
  pyauto_functional.Main()
