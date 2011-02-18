#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import pickle

import dataset_converter
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
    test_data = self.EvalDataFrom(file_path)
    credit_cards_input = test_data['input']
    self.FillAutoFillProfile(credit_cards=credit_cards_input)
    self.assertEqual(test_data['expected'],
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

    # Then try credit cards with invalid input.  Should strip off all non-digits
    credit_card = {'CREDIT_CARD_NUMBER': 'Not_0123-5Checked'}
    expected_credit_card = {'CREDIT_CARD_NUMBER': '01235'}
    self.FillAutoFillProfile(credit_cards=[credit_card])
    self.assertEqual([expected_credit_card],
                     self.GetAutoFillProfile()['credit_cards'])

  def testFilterIncompleteAddresses(self):
    """Test Autofill filters out profile with incomplete address info."""
    profile = {'NAME_FIRST': 'Bob',
               'NAME_LAST': 'Smith',
               'EMAIL_ADDRESS': 'bsmith@example.com',
               'COMPANY_NAME': 'Company X',
               'PHONE_HOME_WHOLE_NUMBER': '650-123-4567',}
    url = self.GetHttpURLForDataPath(
        os.path.join('autofill', 'dup-profiles-test.html'))
    self.NavigateToURL(url)
    for key, value in profile.iteritems():
      script = ('document.getElementById("%s").value = "%s"; '
                'window.domAutomationController.send("done");') % (key, value)
      self.ExecuteJavascript(script, 0, 0)
    js_code = """
      document.getElementById("merge_dup").submit();
      window.addEventListener("unload", function() {
        window.domAutomationController.send("done");
      });
    """
    self.ExecuteJavascript(js_code, 0, 0)
    self.assertEqual([], self.GetAutoFillProfile()['profiles'])

  def testFilterMalformedEmailAddresses(self):
    """Test Autofill filters out malformed email address during form submit."""
    profile = {'NAME_FIRST': 'Bob',
               'NAME_LAST': 'Smith',
               'EMAIL_ADDRESS': 'garbage',
               'ADDRESS_HOME_LINE1': '1234 H St.',
               'ADDRESS_HOME_CITY': 'San Jose',
               'ADDRESS_HOME_STATE': 'CA',
               'ADDRESS_HOME_ZIP': '95110',
               'COMPANY_NAME': 'Company X',
               'PHONE_HOME_WHOLE_NUMBER': '408-123-4567',}
    url = self.GetHttpURLForDataPath(
        os.path.join('autofill', 'dup-profiles-test.html'))
    self.NavigateToURL(url)
    for key, value in profile.iteritems():
      script = ('document.getElementById("%s").value = "%s"; '
                'window.domAutomationController.send("done");') % (key, value)
      self.ExecuteJavascript(script, 0, 0)
    js_code = """
      document.getElementById("merge_dup").submit();
      window.addEventListener("unload", function() {
        window.domAutomationController.send("done");
      });
    """
    self.ExecuteJavascript(js_code, 0, 0)
    if 'EMAIL_ADDRESS' in self.GetAutoFillProfile()['profiles'][0]:
      raise KeyError('TEST FAIL: Malformed email address is saved in profiles.')

  def testAutofillCrowdsourcing(self):
    """Test able to send POST request of web form to Autofill server.

    The Autofill server processes the data offline, so it can take a few days
    for the result to be detectable. Manual verification is required.
    """
    # HTML file needs to be run from a specific http:// url to be able to verify
    # the results a few days later by visiting the same url.
    url = 'http://www.corp.google.com/~dyu/autofill/crowdsourcing-test.html'
    # Adding crowdsourcing Autofill profile.
    file_path = os.path.join(self.DataDir(), 'autofill',
                             'crowdsource_autofill.txt')
    profiles = self.EvalDataFrom(file_path)
    self.FillAutoFillProfile(profiles=profiles)
    # Autofill server captures 2.5% of the data posted.
    # Looping 1000 times is a safe minimum to exceed the server's threshold or
    # noise.
    for i in range(1000):
      fname = self.GetAutoFillProfile()['profiles'][0]['NAME_FIRST']
      lname = self.GetAutoFillProfile()['profiles'][0]['NAME_LAST']
      email = self.GetAutoFillProfile()['profiles'][0]['EMAIL_ADDRESS']
      # Submit form to collect crowdsourcing data for Autofill.
      self.NavigateToURL(url, 0, 0)
      fname_field = ('document.getElementById("fn").value = "%s"; '
                     'window.domAutomationController.send("done");') % fname
      lname_field = ('document.getElementById("ln").value = "%s"; '
                     'window.domAutomationController.send("done");') % lname
      email_field = ('document.getElementById("em").value = "%s"; '
                     'window.domAutomationController.send("done");') % email
      self.ExecuteJavascript(fname_field, 0, 0);
      self.ExecuteJavascript(lname_field, 0, 0);
      self.ExecuteJavascript(email_field, 0, 0);
      self.ExecuteJavascript('document.getElementById("frmsubmit").submit();'
                             'window.domAutomationController.send("done");',
                             0, 0)

  def testMergeDuplicateProfilesInAutofill(self):
    """Test Autofill ability to merge duplicate profiles and throw away junk."""
    # HTML file needs to be run from a http:// url.
    url = self.GetHttpURLForDataPath(
        os.path.join('autofill', 'duplicate_profiles_test.html'))
    # Run the parser script to generate the dictionary list needed for the
    # profiles.
    c = dataset_converter.DatasetConverter(
        os.path.join(self.DataDir(), 'autofill', 'dataset.txt'),
        logging_level=logging.INFO)  # Set verbosity to INFO, WARNING, ERROR.
    list_of_dict = c.Convert()

    for profile in list_of_dict:
      self.NavigateToURL(url)
      for key, value in profile.iteritems():
        script = ('document.getElementById("%s").value = "%s"; '
                  'window.domAutomationController.send("done");') % (key, value)
        self.ExecuteJavascript(script, 0, 0)
      self.ExecuteJavascript('document.getElementById("merge_dup").submit();'
                             'window.domAutomationController.send("done");',
                             0, 0)
    # Verify total number of inputted profiles is greater than the final number
    # of profiles after merging.
    self.assertTrue(
        len(list_of_dict) > len(self.GetAutoFillProfile()['profiles']))
    # Write profile dictionary to a file.
    merged_profile = os.path.join(self.DataDir(), 'autofill',
                                  'merged-profiles.txt')
    profile_dict = self.GetAutoFillProfile()['profiles']
    output = open(merged_profile, 'wb')
    pickle.dump(profile_dict, output)
    output.close()


if __name__ == '__main__':
  pyauto_functional.Main()
