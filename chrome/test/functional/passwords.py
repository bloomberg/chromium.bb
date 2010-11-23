#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pyauto_functional  # Must be imported before pyauto
import pyauto
import test_utils


class PasswordTest(pyauto.PyUITest):
  """Tests that passwords work correctly"""

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

  def testSavePassword(self):
    """Test saving a password and getting saved passwords."""
    password1 = { 'username_value': 'user@example.com',
      'password_value': 'test.password',
      'signon_realm': 'https://www.example.com/',
      'time': 1279650942.0,
      'origin_url': 'https://www.example.com/login',
      'username_element': 'username',
      'password_element': 'password',
      'submit_element': 'submit',
      'action_target': 'https://www.example.com/login/',
      'blacklist': False }
    self.assertTrue(self.AddSavedPassword(password1))
    self.assertEquals(self.GetSavedPasswords(), [password1])

  def testDisplayAndSavePasswordInfobar(self):
    """Verify password infobar displays and able to save password."""
    url_https = 'https://www.google.com/accounts/'
    url_logout = 'https://www.google.com/accounts/Logout'
    creds = self.GetPrivateInfo()['test_google_account']
    username = creds['username']
    password = creds['password']
    test_utils.GoogleAccountsLogin(self, ['url'], username, password)
    # Wait until page completes loading.
    self.WaitUntil(
        lambda: self.GetDOMValue('document.readyState'), 'complete')
    self.WaitForInfobarCount(1)
    infobar = self.GetBrowserInfo()['windows'][0]['tabs'][0]['infobars']
    self.assertEqual(infobar[0]['type'], 'confirm_infobar')
    self.PerformActionOnInfobar('accept', infobar_index=0)
    self.NavigateToURL(url_logout)
    self.NavigateToURL(url_https)
    test_utils.VerifyGoogleAccountCredsFilled(self, username, password)
    self.ExecuteJavascript('document.getElementById("gaia_loginform").submit();'
                           'window.domAutomationController.send("done")')
    test_utils.ClearPasswords(self)


if __name__ == '__main__':
  pyauto_functional.Main()
