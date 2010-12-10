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
        lambda: self.GetDOMValue('document.readyState'), 'complete')
    self.assertTrue(self.WaitForInfobarCount(1),
                    'Did not get save password infobar')
    infobar = self.GetBrowserInfo()['windows'][0]['tabs'][0]['infobars']
    self.assertEqual(infobar[0]['type'], 'confirm_infobar')
    self.PerformActionOnInfobar('accept', infobar_index=0)
    self.NavigateToURL(url_logout)
    self.NavigateToURL(url_https)
    test_utils.VerifyGoogleAccountCredsFilled(self, username, password)
    self.ExecuteJavascript('document.getElementById("gaia_loginform").submit();'
                           'window.domAutomationController.send("done")')
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

    creds2 = self.GetPrivateInfo()['etouchqa_google_account']
    test_utils.GoogleAccountsLogin(
        self, creds2['username'], creds2['password'], tab_index=1)
    self.assertTrue(self.WaitForInfobarCount(1, tab_index=1))
    # Selecting 'Never for this site' option on password infobar.
    self.PerformActionOnInfobar('cancel', infobar_index=0, tab_index=1)

    # TODO: GetSavedPasswords() doesn't return anything when empty.
    # http://crbug.com/64603
    # self.assertFalse(self.GetSavedPasswords())
    # TODO: Check the exceptions list


if __name__ == '__main__':
  pyauto_functional.Main()
