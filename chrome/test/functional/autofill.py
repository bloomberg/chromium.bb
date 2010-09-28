#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

import pyauto_functional  # Must be imported before pyauto
import pyauto


class AutoFillTest(pyauto.PyUITest):
  """Tests that autofill works correctly"""

  def Debug(self):
    """Test method for experimentation.

    This method will not run automatically.
    """
    import pprint
    pp = pprint.PrettyPrinter(indent=2)
    while True:
      raw_input('Hit <enter> to dump info.. ')
      info = self.GetAutoFillProfile()
      pp.pprint(info)

  def testFillProfile(self):
    """Test filling profiles and overwriting with new profiles."""
    profiles = [{'NAME_FIRST': 'Bob',
                 'NAME_LAST': 'Smith', 'ADDRESS_HOME_ZIP': '94043',},
                {'EMAIL_ADDRESS': 'sue@example.com',
                 'COMPANY_NAME': 'Company X',}]
    credit_cards = [{'CREDIT_CARD_NUMBER': '6011111111111117',
                     'CREDIT_CARD_EXP_MONTH': '12',
                     'CREDIT_CARD_EXP_4_DIGIT_YEAR': '2011'},
                    {'CREDIT_CARD_NAME': 'Bob C. Smith'}]

    self.FillAutoFillProfile(profiles=profiles, credit_cards=credit_cards)
    profile = self.GetAutoFillProfile()
    self.assertEqual(profiles, profile['profiles'])
    self.assertEqual(credit_cards, profile['credit_cards'])

    profiles = [ {'NAME_FIRST': 'Larry'}]
    self.FillAutoFillProfile(profiles=profiles)
    profile = self.GetAutoFillProfile()
    self.assertEqual(profiles, profile['profiles'])
    self.assertEqual(credit_cards, profile['credit_cards'])

  def testFillProfileCrazyCharacters(self):
    """Test filling profiles with unicode strings and crazy characters."""
    # Adding autofill profiles.
    file_path = os.path.join(self.DataDir(), 'autofill', 'crazy_autofill.txt')
    profiles = self.EvalDataFrom(file_path)
    self.FillAutoFillProfile(profiles=profiles)

    self.assertEqual(profiles, self.GetAutoFillProfile()['profiles'])

    # Adding credit cards.
    file_path = os.path.join(self.DataDir(), 'autofill',
                             'crazy_creditcards.txt')
    credit_cards = self.EvalDataFrom(file_path)
    self.FillAutoFillProfile(credit_cards=credit_cards)
    self.assertEqual(credit_cards,
                     self.GetAutoFillProfile()['credit_cards'])

  def testGetProfilesEmpty(self):
    """Test getting profiles when none have been filled."""
    profile = self.GetAutoFillProfile()
    self.assertEqual([], profile['profiles'])
    self.assertEqual([], profile['credit_cards'])

  def testAutofillInvalid(self):
    """Test filling in invalid values for profiles and credit cards."""
    # First try profiles with invalid input.
    without_invalid = {'NAME_FIRST': u'Will',
                       'ADDRESS_HOME_CITY': 'Sunnyvale',
                       'ADDRESS_HOME_STATE': 'CA',
                       'ADDRESS_HOME_ZIP': 'my_zip',
                       'ADDRESS_HOME_COUNTRY': 'USA'}
    # Add some invalid fields.
    with_invalid = without_invalid.copy()
    with_invalid['PHONE_HOME_WHOLE_NUMBER'] = 'Invalid_Phone_Number'
    with_invalid['PHONE_FAX_WHOLE_NUMBER'] = 'Invalid_Fax_Number'
    self.FillAutoFillProfile(profiles=[with_invalid])
    self.assertEqual([without_invalid],
                     self.GetAutoFillProfile()['profiles'])

    # Then try credit cards with invalid input.
    credit_card = {'CREDIT_CARD_NUMBER': 'Not_Checked'}
    self.FillAutoFillProfile(credit_cards=[credit_card])
    self.assertEqual([credit_card],
                              self.GetAutoFillProfile()['credit_cards'])


if __name__ == '__main__':
  pyauto_functional.Main()
