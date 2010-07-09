#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pyauto_functional  # Must be imported before pyauto
import pyauto


class AutoFillTest(pyauto.PyUITest):
  """Tests that autofill works correctly"""

  def testFillProfile(self):
    """Test filling profiles and overwriting with new profiles"""
    profiles = [{'label': 'Profile 1', 'NAME_FIRST': 'Bob',
                 'NAME_LAST': 'Smith', 'ADDRESS_HOME_ZIP': '94043',},
                {'label': 'Profile 2', 'EMAIL_ADDRESS': 'sue@example.com',
                 'COMPANY_NAME': 'Company X',}]
    credit_cards = [{'label': 'Credit Card 1',
                     'CREDIT_CARD_NUMBER': '6011111111111117',
                     'CREDIT_CARD_EXP_MONTH': '12',
                     'CREDIT_CARD_EXP_4_DIGIT_YEAR': '2011'},
                    {'label': 'Credit Card 2',
                     'CREDIT_CARD_NAME': 'Bob C. Smith',
                     'CREDIT_CARD_TYPE': 'Visa'}]

    self.FillAutoFillProfile(profiles=profiles, credit_cards=credit_cards)
    profile = self.GetAutoFillProfile()
    self.assertEqual(profiles, profile['profiles'])
    self.assertEqual(credit_cards, profile['credit_cards'])

    profiles = [ {'label': 'Profile3', 'NAME_FIRST': 'Larry'}]
    self.FillAutoFillProfile(profiles=profiles)
    profile = self.GetAutoFillProfile()
    self.assertEqual(profiles, profile['profiles'])
    self.assertEqual(credit_cards, profile['credit_cards'])

  def testFillProfileUnicode(self):
    """Test filling profiles with unicode strings"""
    profiles = [{'label': u'unic\u00F3de', 'NAME_FIRST': u'J\u00E4n',
                'ADDRESS_HOME_LINE1': u'123 R\u00F6d'}]
    self.FillAutoFillProfile(profiles)
    self.assertEqual(profiles, self.GetAutoFillProfile()['profiles'])

  def testGetProfilesEmpty(self):
    """Test getting profiles when none have been filled"""
    profile = self.GetAutoFillProfile()
    self.assertEqual([], profile['profiles'])
    self.assertEqual([], profile['credit_cards'])


if __name__ == '__main__':
  pyauto_functional.Main()
