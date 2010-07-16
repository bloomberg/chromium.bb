#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import time

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
    username1 = 'username1'
    password1 = 'password1'
    username2 = 'username2'
    password2 = 'password2'

    self.AddSavedPassword(username1, password1)
    passwords = self.GetSavedPasswords()['passwords']
    self.assertEqual(1, len(passwords))
    pw = passwords[0]
    self.assertEqual(username1, pw['username'])
    self.assertEqual(password1, pw['password'])

    now = time.time()
    self.AddSavedPassword(username2, password2, now)
    passwords = self.GetSavedPasswords()['passwords']
    self.assertEqual(2, len(passwords))
    pw1 = passwords[0]
    pw2 = passwords[1]
    self.assertEqual(username1, pw1['username'])
    self.assertEqual(password1, pw1['password'])
    self.assertEqual(username2, pw2['username'])
    self.assertEqual(password2, pw2['password'])
    self._AssertWithinOneSecond(now, pw2['time'])


if __name__ == '__main__':
  pyauto_functional.Main()
