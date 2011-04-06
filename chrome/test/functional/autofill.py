#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import pickle

import autofill_dataset_converter
import autofill_dataset_generator
import pyauto_functional  # Must be imported before pyauto
import pyauto


class AutofillTest(pyauto.PyUITest):
  """Tests that autofill works correctly"""

  def Debug(self):
    """Test method for experimentation.

    This method will not run automatically.
    """
    import pprint
    pp = pprint.PrettyPrinter(indent=2)
    while True:
      raw_input('Hit <enter> to dump info.. ')
      info = self.GetAutofillProfile()
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

    self.FillAutofillProfile(profiles=profiles, credit_cards=credit_cards)
    profile = self.GetAutofillProfile()
    self.assertEqual(profiles, profile['profiles'])
    self.assertEqual(credit_cards, profile['credit_cards'])

    profiles = [ {'NAME_FIRST': 'Larry'}]
    self.FillAutofillProfile(profiles=profiles)
    profile = self.GetAutofillProfile()
    self.assertEqual(profiles, profile['profiles'])
    self.assertEqual(credit_cards, profile['credit_cards'])

  def testFillProfileCrazyCharacters(self):
    """Test filling profiles with unicode strings and crazy characters."""
    # Adding autofill profiles.
    file_path = os.path.join(self.DataDir(), 'autofill', 'crazy_autofill.txt')
    profiles = self.EvalDataFrom(file_path)
    self.FillAutofillProfile(profiles=profiles)

    self.assertEqual(profiles, self.GetAutofillProfile()['profiles'])

    # Adding credit cards.
    file_path = os.path.join(self.DataDir(), 'autofill',
                             'crazy_creditcards.txt')
    test_data = self.EvalDataFrom(file_path)
    credit_cards_input = test_data['input']
    self.FillAutofillProfile(credit_cards=credit_cards_input)
    self.assertEqual(test_data['expected'],
                     self.GetAutofillProfile()['credit_cards'])

  def testGetProfilesEmpty(self):
    """Test getting profiles when none have been filled."""
    profile = self.GetAutofillProfile()
    self.assertEqual([], profile['profiles'])
    self.assertEqual([], profile['credit_cards'])

  def testAutofillInvalid(self):
    """Test filling in invalid values for profiles and credit cards."""
    # First try profiles with invalid input.
    without_invalid = {'NAME_FIRST': u'Will',
                       'ADDRESS_HOME_CITY': 'Sunnyvale',
                       'ADDRESS_HOME_STATE': 'CA',
                       'ADDRESS_HOME_ZIP': 'my_zip',
                       'ADDRESS_HOME_COUNTRY': 'United States'}
    # Add some invalid fields.
    with_invalid = without_invalid.copy()
    with_invalid['PHONE_HOME_WHOLE_NUMBER'] = 'Invalid_Phone_Number'
    with_invalid['PHONE_FAX_WHOLE_NUMBER'] = 'Invalid_Fax_Number'
    self.FillAutofillProfile(profiles=[with_invalid])
    self.assertEqual([without_invalid],
                     self.GetAutofillProfile()['profiles'])

    # Then try credit cards with invalid input.  Should strip off all non-digits
    credit_card = {'CREDIT_CARD_NUMBER': 'Not_0123-5Checked'}
    expected_credit_card = {'CREDIT_CARD_NUMBER': '01235'}
    self.FillAutofillProfile(credit_cards=[credit_card])
    self.assertEqual([expected_credit_card],
                     self.GetAutofillProfile()['credit_cards'])

  def testFilterIncompleteAddresses(self):
    """Test Autofill filters out profile with incomplete address info."""
    profile = {'NAME_FIRST': 'Bob',
               'NAME_LAST': 'Smith',
               'EMAIL_ADDRESS': 'bsmith@example.com',
               'COMPANY_NAME': 'Company X',
               'PHONE_HOME_WHOLE_NUMBER': '650-123-4567',}
    url = self.GetHttpURLForDataPath(
        os.path.join('autofill', 'duplicate_profiles_test.html'))
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
    self.assertEqual([], self.GetAutofillProfile()['profiles'])

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
        os.path.join('autofill', 'duplicate_profiles_test.html'))
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
    if 'EMAIL_ADDRESS' in self.GetAutofillProfile()['profiles'][0]:
      raise KeyError('TEST FAIL: Malformed email address is saved in profiles.')

  def _SendKeyEventsToPopulateForm(self, tab_index=0, windex=0):
    """Send key events to populate a web form with Autofill profile data.

    Args:
      tab_index: The tab index, default is 0.
      windex: The window index, default is 0.
    """
    TAB_KEYPRESS = 0x09  # Tab keyboard key press.
    DOWN_KEYPRESS = 0x28  # Down arrow keyboard key press.
    RETURN_KEYPRESS = 0x0D  # Return keyboard key press.

    self.SendWebkitKeyEvent(TAB_KEYPRESS, tab_index, windex)
    self.SendWebkitKeyEvent(DOWN_KEYPRESS, tab_index, windex)
    self.SendWebkitKeyEvent(DOWN_KEYPRESS, tab_index, windex)
    self.SendWebkitKeyEvent(RETURN_KEYPRESS, tab_index, windex)

  def testComparePhoneNumbers(self):
    """Test phone fields parse correctly from a given profile.

    The high level key presses execute the following: Select the first text
    field, invoke the autofill popup list, select the first profile within the
    list, and commit to the profile to populate the form.
    """
    profile_path = os.path.join(self.DataDir(), 'autofill',
                                'phone_pinput_autofill.txt')
    profile_expected_path = os.path.join(self.DataDir(), 'autofill',
                                         'phone_pexpected_autofill.txt')
    profiles = self.EvalDataFrom(profile_path)
    profiles_expected = self.EvalDataFrom(profile_expected_path)
    self.FillAutofillProfile(profiles=profiles)
    url = self.GetHttpURLForDataPath(
        os.path.join('autofill', 'form_phones.html'))
    for profile_expected in profiles_expected:
      self.NavigateToURL(url)
      self._SendKeyEventsToPopulateForm()
      form_values = {}
      for key, value in profile_expected.iteritems():
        js_returning_field_value = (
            'var field_value = document.getElementById("%s").value;'
            'window.domAutomationController.send(field_value);'
            ) % key
        form_values[key] = self.ExecuteJavascript(
            js_returning_field_value, 0, 0)
        self.assertEqual(
            form_values[key], value,
            ('Original profile not equal to expected profile at key: "%s"\n'
             'Expected: "%s"\nReturned: "%s"' % (key, value, form_values[key])))

  def testCCInfoNotStoredWhenAutocompleteOff(self):
    """Test CC info not offered to be saved when autocomplete=off for CC field.

    If the credit card number field has autocomplete turned off, then the credit
    card infobar should not offer to save the credit card info.
    """
    credit_card_info = {'CREDIT_CARD_NAME': 'Bob Smith',
                        'CREDIT_CARD_NUMBER': '6011111111111117',
                        'CREDIT_CARD_EXP_MONTH': '12',
                        'CREDIT_CARD_EXP_4_DIGIT_YEAR': '2014'}

    url = self.GetHttpURLForDataPath(
        os.path.join('autofill', 'cc_autocomplete_off_test.html'))
    self.NavigateToURL(url)
    for key, value in credit_card_info.iteritems():
      script = ('document.getElementById("%s").value = "%s"; '
                'window.domAutomationController.send("done");') % (key, value)
      self.ExecuteJavascript(script, 0, 0)
    js_code = """
      document.getElementById("cc_submit").submit();
      window.addEventListener("unload", function() {
        window.domAutomationController.send("done");
      });
    """
    self.ExecuteJavascript(js_code, 0, 0)
    # Wait until form is submitted and page completes loading.
    self.WaitUntil(
        lambda: self.GetDOMValue('document.readyState'),
        expect_retval='complete')
    cc_infobar = self.GetBrowserInfo()['windows'][0]['tabs'][0]['infobars']
    self.assertEqual(0, len(cc_infobar),
                     'Save credit card infobar offered to save CC info.')

  def testNoAutofillForReadOnlyFields(self):
    """Test that Autofill does not fill in read-only fields."""
    profile = {'NAME_FIRST': 'Bob',
               'NAME_LAST': 'Smith',
               'EMAIL_ADDRESS': 'bsmith@gmail.com',
               'ADDRESS_HOME_LINE1': '1234 H St.',
               'ADDRESS_HOME_CITY': 'San Jose',
               'ADDRESS_HOME_STATE': 'CA',
               'ADDRESS_HOME_ZIP': '95110',
               'COMPANY_NAME': 'Company X',
               'PHONE_HOME_WHOLE_NUMBER': '408-123-4567',}

    self.FillAutofillProfile(profiles=[profile])
    url = self.GetHttpURLForDataPath(
        os.path.join('autofill', 'read_only_field_test.html'))
    self.NavigateToURL(url)
    self._SendKeyEventsToPopulateForm()
    js_return_readonly_field = (
        'var field_value = document.getElementById("email").value;'
        'window.domAutomationController.send(field_value);')
    readonly_field_value = self.ExecuteJavascript(
        js_return_readonly_field, 0, 0)
    js_return_addrline1_field = (
        'var field_value = document.getElementById("address").value;'
        'window.domAutomationController.send(field_value);')
    addrline1_field_value = self.ExecuteJavascript(
        js_return_addrline1_field, 0, 0)
    self.assertNotEqual(
        readonly_field_value, profile['EMAIL_ADDRESS'],
        'Autofill filled in value "%s" for a read-only field.'
        % readonly_field_value)
    self.assertEqual(
        addrline1_field_value, profile['ADDRESS_HOME_LINE1'],
        'Unexpected value "%s" in the Address field.' % addrline1_field_value)

  def FormFillLatencyAfterSubmit(self):
    """Test latency time on form submit with lots of stored Autofill profiles.

    This test verifies when a profile is selected from the Autofill dictionary
    that consists of thousands of profiles, the form does not hang after being
    submitted.

    The high level key presses execute the following: Select the first text
    field, invoke the autofill popup list, select the first profile within the
    list, and commit to the profile to populate the form.

    This test is partially automated. The bulk of the work is done, such as
    generating 1500 plus profiles, inserting those profiles into Autofill,
    selecting a profile from the list. The tester will need to click on the
    submit button and check if the browser hangs.
    """
    # HTML file needs to be run from a http:// url.
    url = self.GetHttpURLForDataPath(
        os.path.join('autofill', 'latency_after_submit_test.html'))
    # Run the generator script to generate the dictionary list needed for the
    # profiles.
    gen = autofill_dataset_generator.DatasetGenerator(
        logging_level=logging.ERROR)
    list_of_dict = gen.GenerateDataset(num_of_dict_to_generate=1501)
    self.FillAutofillProfile(profiles=list_of_dict)
    self.NavigateToURL(url)
    # Tab keyboard key press.
    self.SendWebkitKeyEvent(TAB_KEYPRESS, windex=0, tab_index=0)
    # Down arrow keyboard key press.
    self.SendWebkitKeyEvent(DOWN_KEYPRESS, windex=0, tab_index=0)
    # Down arrow keyboard key press.
    self.SendWebkitKeyEvent(DOWN_KEYPRESS, windex=0, tab_index=0)
    # Return keyboard key press.
    self.SendWebkitKeyEvent(RETURN_KEYPRESS, windex=0, tab_index=0)
    # TODO (dyu): add automated form hang or crash verification.
    raw_input(
        'Verify the test manually. Test hang time after submitting the form.')


  def AutofillCrowdsourcing(self):
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
    self.FillAutofillProfile(profiles=profiles)
    # Autofill server captures 2.5% of the data posted.
    # Looping 1000 times is a safe minimum to exceed the server's threshold or
    # noise.
    for i in range(1000):
      fname = self.GetAutofillProfile()['profiles'][0]['NAME_FIRST']
      lname = self.GetAutofillProfile()['profiles'][0]['NAME_LAST']
      email = self.GetAutofillProfile()['profiles'][0]['EMAIL_ADDRESS']
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

  def MergeDuplicateProfilesInAutofill(self):
    """Test Autofill ability to merge duplicate profiles and throw away junk."""
    # HTML file needs to be run from a http:// url.
    url = self.GetHttpURLForDataPath(
        os.path.join('autofill', 'duplicate_profiles_test.html'))
    # Run the parser script to generate the dictionary list needed for the
    # profiles.
    c = autofill_dataset_converter.DatasetConverter(
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
        len(list_of_dict) > len(self.GetAutofillProfile()['profiles']))
    # Write profile dictionary to a file.
    merged_profile = os.path.join(self.DataDir(), 'autofill',
                                  'merged-profiles.txt')
    profile_dict = self.GetAutofillProfile()['profiles']
    output = open(merged_profile, 'wb')
    pickle.dump(profile_dict, output)
    output.close()


if __name__ == '__main__':
  pyauto_functional.Main()
