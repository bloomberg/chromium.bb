#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import pickle
import re
import simplejson

import pyauto_functional  # Must be imported before pyauto
import pyauto
import test_utils
from selenium.webdriver.common.keys import Keys
from selenium.webdriver.common.action_chains import ActionChains
from webdriver_pages import settings


class AutofillTest(pyauto.PyUITest):
  """Tests that autofill UI works correctly. Also contains a manual test for
     the crowdsourcing server."""

  def setUp(self):
    pyauto.PyUITest.setUp(self)
    self._driver = self.NewWebDriver()

  def AutofillCrowdsourcing(self):
    """Test able to send POST request of web form to Autofill server.

    The Autofill server processes the data offline, so it can take a few days
    for the result to be detectable. Manual verification is required.
    """
    # HTML file needs to be run from a specific http:// url to be able to verify
    # the results a few days later by visiting the same url.
    url = 'http://www.corp.google.com/~dyu/autofill/crowdsourcing-test.html'
    # Autofill server captures 2.5% of the data posted.
    # Looping 1000 times is a safe minimum to exceed the server's threshold or
    # noise.
    for i in range(1000):
      fname = 'David'
      lname = 'Yu'
      email = 'david.yu@gmail.com'
      # Submit form to collect crowdsourcing data for Autofill.
      self.NavigateToURL(url, 0, 0)
      profile = {'fn': fname, 'ln': lname, 'em': email}
      js = ''.join(['document.getElementById("%s").value = "%s";' %
                    (key, value) for key, value in profile.iteritems()])
      js += 'document.getElementById("testform").submit();'
      self.ExecuteJavascript(js)

  def _SelectOptionXpath(self, value):
    """Returns an xpath query used to select an item from a dropdown list.
    Args:
      value: Option selected for the drop-down list field.

    Returns:
      The value of the xpath query.
    """
    return '//option[@value="%s"]' % value

  def testPostalCodeAndStateLabelsBasedOnCountry(self):
    """Verify postal code and state labels based on selected country."""
    data_file = os.path.join(self.DataDir(), 'autofill', 'functional',
                             'state_zip_labels.txt')
    test_data = simplejson.loads(open(data_file).read())

    page = settings.AutofillEditAddressDialog.FromNavigation(self._driver)
    # Initial check of State and ZIP labels.
    self.assertEqual('State', page.GetStateLabel())
    self.assertEqual('ZIP code', page.GetPostalCodeLabel())

    for country_code in test_data:
      page.Fill(country_code=country_code)

      # Compare postal code labels.
      actual_postal_label = page.GetPostalCodeLabel()
      self.assertEqual(
          test_data[country_code]['postalCodeLabel'],
          actual_postal_label,
          msg=('Postal code label "%s" does not match Country "%s"' %
               (actual_postal_label, country_code)))

      # Compare state labels.
      actual_state_label = page.GetStateLabel()
      self.assertEqual(
          test_data[country_code]['stateLabel'],
          actual_state_label,
          msg=('State label "%s" does not match Country "%s"' %
               (actual_state_label, country_code)))

  def testNoDuplicatePhoneNumsInPrefs(self):
    """Test duplicate phone numbers entered in prefs are removed."""
    page = settings.AutofillEditAddressDialog.FromNavigation(self._driver)
    non_duplicates = ['111-1111', '222-2222']
    duplicates = ['111-1111']
    page.Fill(phones=non_duplicates + duplicates)
    self.assertEqual(non_duplicates, page.GetPhones(),
        msg='Duplicate phone number in prefs unexpectedly saved.')

  def testDisplayLineItemForEntriesWithNoCCNum(self):
    """Verify Autofill creates a line item for CC entries with no CC number."""
    self.NavigateToURL('chrome://settings-frame/autofillEditCreditCard')
    self._driver.find_element_by_id('name-on-card').send_keys('Jane Doe')
    query_month = self._SelectOptionXpath('12')
    query_year = self._SelectOptionXpath('2014')
    self._driver.find_element_by_id('expiration-month').find_element_by_xpath(
        query_month).click()
    self._driver.find_element_by_id('expiration-year').find_element_by_xpath(
        query_year).click()
    self._driver.find_element_by_id(
        'autofill-edit-credit-card-apply-button').click()
    # Refresh the page to ensure the UI is up-to-date.
    self._driver.refresh()
    list_entry = self._driver.find_element_by_class_name('autofill-list-item')
    self.assertTrue(list_entry.is_displayed)
    self.assertEqual('Jane Doe', list_entry.text,
                     msg='Saved CC line item not same as what was entered.')

  def _GetElementList(self, container_elem, fields_to_select):
    """Returns all sub elements of specific characteristics.

    Args:
      container_elem: An element that contains other elements.
      fields_to_select: A list of fields to select with strings that
                        help create an xpath string, which in turn identifies
                        the elements needed.
                        For example: ['input', 'button']
                        ['div[@id]', 'button[@disabled]']
                        ['*[class="example"]']

    Returns:
      List of all subelements found in the container element.
    """
    self.assertTrue(fields_to_select, msg='No fields specified for selection.')
    fields_to_select = ['.//' + field for field in fields_to_select]
    xpath_arg = ' | '.join(fields_to_select)
    field_elems = container_elem.find_elements_by_xpath(xpath_arg)
    return field_elems

  def _GetElementInfo(self, element):
    """Returns visual comprehensive info about an element.

    This function identifies the text of the correspoinding label when tab
    ordering fails.
    This info consists of:
      The labels, buttons, ids, placeholder attribute values, or the element id.

    Args:
      element: The target element.

    Returns:
      A string that identifies the element in the page.
    """
    element_info = ''
    if element.tag_name == 'button':
      element_info = element.text
    element_info = (element_info or element.get_attribute('id') or
        element.get_attribute('placeholder') or
        element.get_attribute('class') or element.id)
    return '%s: %s' % (element.tag_name, element_info)

  def _LoadPageAndGetFieldList(self):
    """Navigate to autofillEditAddress page and finds the elements with focus.

    These elements are of input, select, and button types.

    Returns:
      A list with all elements that can receive focus.
    """
    url = 'chrome://settings-frame/autofillEditAddress'
    self._driver.get(url)
    container_elem = self._driver.find_element_by_id(
        'autofill-edit-address-overlay')
    # The container element contains input, select and button fields. Some of
    # the buttons are disabled so they are ignored.
    field_list = self._GetElementList(container_elem,
                                      ['input', 'select',
                                       'button[not(@disabled)]'])
    self.assertTrue(field_list, 'No fields found in "%s".' % url)
    return field_list

  def testTabOrderForEditAddress(self):
    """Verify the TAB ordering for Edit Address page is correct."""
    tab_press = ActionChains(self._driver).send_keys(Keys.TAB)
    field_list = self._LoadPageAndGetFieldList()

    # Creates a dictionary where a field key returns the value of the next field
    # in the field list. The last field of the field list is mapped to the first
    # field of the field list.
    field_nextfield_dict = dict(
        zip(field_list, field_list[1:] + field_list[:1]))

    # Wait until a field of |field_list| has received the focus.
    self.WaitUntil(lambda:
                   self._driver.switch_to_active_element().id in
                   [f.id for f in field_list])
    # The first field is expected to receive the focus.
    self.assertEqual(self._driver.switch_to_active_element().id,
                     field_list[0].id,
                     msg='The first field did not receive tab focus.')
    for field in field_list:
      tab_press.perform()
      # Wait until a field of |field_list|, other than the current field, has
      # received the focus.
      self.WaitUntil(lambda:
                     self._driver.switch_to_active_element().id != field.id and
                     self._driver.switch_to_active_element().id in
                     [f.id for f in field_list])

      self.assertEqual(self._driver.switch_to_active_element().id,
                       field_nextfield_dict[field].id,
                       msg=('The TAB ordering is broken. Previous field: "%s"\n'
                            'Field expected to receive focus: "%s"\n'
                            'Field that received focus instead: "%s"')
                       % (self._GetElementInfo(field),
                          self._GetElementInfo(field_nextfield_dict[field]),
                          self._GetElementInfo(
                              self._driver.switch_to_active_element())))


if __name__ == '__main__':
  pyauto_functional.Main()
