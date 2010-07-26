#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pyauto_functional  # Must be imported before pyauto
import pyauto


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


if __name__ == '__main__':
  pyauto_functional.Main()
