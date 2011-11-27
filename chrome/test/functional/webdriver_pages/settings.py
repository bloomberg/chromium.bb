# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import types

import selenium.common.exceptions
from selenium.webdriver.common.action_chains import ActionChains


class DynamicList(object):
  """A web element that holds a dynamic list of items.

  Terminology:
    field: an HTML text field
    item element: an element in the HTML list
    item: the value of an item element; if the item element only has 1
      field, it will be a single value; if the item element has multiple
      fields, it will be a python list of values
    placeholder: the last item element in the list, which is not committed yet

  The user can add new items to the list by typing in the placeholder item.
  When a user presses enter or focuses something else, the placeholder item
  is committed and a new placeholder is created. An item may contain 1 or
  more text fields.
  """

  def __init__(self, driver, elem):
    self.driver = driver
    self.elem = elem

  def Add(self, item):
    """Adds an item to the list.

    An item may contain several fields, in which case 'item' should be a python
    list.

    Args:
      item: The item to add to the list.
    """
    fields = self._GetPlaceholderElem().find_elements_by_tag_name('input')
    if len(fields) > 1:
      assert type(item) == types.ListType, \
          """The item must be a list for a HTML list that has multi-field
          items. '%s' should be in a list.""" % item
      values = item
    else:
      values = [item]

    assert len(fields) == len(values), \
        """The item to be added must have the same number of fields as an item
        in the HTML list. Given item '%s' should have %s fields.""" % (
            item, len(fields))
    self._FocusField(fields[0])
    for field, value in zip(fields, values)[1:-1]:
      field.send_keys(value + '\t')
    fields[-1].send_keys(values[-1] + '\n')

  def RemoveAll(self):
    """Removes all items from the list."""
    # There's no close button for the last field.
    close_buttons = self.elem.find_elements_by_class_name('close-button')[:-1]
    for button in close_buttons:
      # Highlight the item, so the close button shows up, then click it.
      ActionChains(self.driver).move_to_element(button).click().perform()

  def GetCommittedItems(self):
    """Returns all the items that are in the list, except the placeholder."""
    def GetItemFromElem(elem):
      """Gets the item held by the list item element.

      This may be an array of items, if there are multiple fields per item.
      """
      fields = elem.find_elements_by_tag_name('input')
      if len(fields) == 1:
        return TextField(fields[0]).Get()
      return map(lambda x: TextField(x).Get(), fields)

    return map(GetItemFromElem, self._GetCommittedItemElems())

  def GetSize(self):
    """Returns the number of items in the list, excluding the placeholder."""
    return len(self._GetCommittedItemElems())

  def _FocusField(self, field):
    """Focuses a field in the list.

    Note, the item containing the field should not be focused already.

    Typing into a field is tricky because the js automatically focuses and
    selects the text field after 50ms after it first receives focus. This
    method focuses the field and waits for the timeout to occur.
    For more info, see inline_editable_list.js and search for setTimeout.
    See crbug.com/97369.

    Args:
      field: HTML text field to focus.

    Raises:
      RuntimeError: A timeout occurred when waiting for the focus event.
    """
    # To wait properly for the focus, we focus the last text field, and then
    # add a focus listener to it, so that we return when the element is focused
    # again after the timeout. We have to focus a different element in between
    # these steps, otherwise the focus event will not fire since the element
    # already has focus.
    # Ideally this should be fixed in the page.

    correct_focus_script = """
        (function(listElem, itemElem, callback) {
          if (document.activeElement == itemElem) {
            callback();
            return;
          }
          itemElem.focus();
          listElem.focus();
          itemElem.addEventListener("focus", callback);
        }).apply(null, arguments);
    """
    self.driver.set_script_timeout(5)
    try:
      self.driver.execute_async_script(correct_focus_script, self.elem, field)
    except selenium.common.exceptions.TimeoutException:
      raise RuntimeError('Unable to focus list item' + value)

  def _GetCommittedItemElems(self):
    return self._GetItemElems()[:-1]

  def _GetPlaceholderElem(self):
    return self._GetItemElems()[-1]

  def _GetItemElems(self):
    return self.elem.find_elements_by_xpath('.//*[@role="listitem"]')


class TextField(object):
  """A text field web element."""
  def __init__(self, elem):
    self.elem = elem

  def Set(self, value):
    """Sets the value of the text field.

    Args:
      value: The new value of the field.
    """
    self.elem.clear()
    self.elem.send_keys(value)

  def Get(self):
    """Returns the value of the text field."""
    return self.elem.get_attribute('value')


class AutofillEditAddressDialog(object):
  """The overlay for editing an autofill address."""

  _URL = 'chrome://settings/autofillEditAddress'

  @staticmethod
  def FromNavigation(driver):
    """Creates an instance of the dialog by navigating directly to it."""
    driver.get(AutofillEditAddressDialog._URL)
    return AutofillEditAddressDialog(driver)

  def __init__(self, driver):
    self.driver = driver
    assert self._URL == driver.current_url
    self.dialog_elem = driver.find_element_by_id(
        'autofill-edit-address-overlay')

  def Fill(self, names=None, addr_line_1=None, city=None, state=None,
           postal_code=None, country_code=None, phones=None):
    """Fills in the given data into the appropriate fields.

    If filling into a text field, the given value will replace the current one.
    If filling into a list, the values will be added after all items are
    deleted.

    Note: 'names', in the new autofill UI, is an array of full names. A full
      name is an array of first, middle, last names. Example:
        names=[['Joe', '', 'King'], ['Fred', 'W', 'Michno']]

    Args:
      names: List of names; each name should be [first, middle, last].
      addr_line_1: First line in the address.
      city: City.
      state: State.
      postal_code: Postal code (zip code for US).
      country_code: Country code (e.g., US or FR).
      phones: List of phone numbers.
    """
    id_dict = {'addr-line-1': addr_line_1,
               'city': city,
               'state': state,
               'postal-code': postal_code}
    for id, value in id_dict.items():
      if value is not None:
        TextField(self.dialog_elem.find_element_by_id(id)).Set(value)

    list_id_dict = {'full-name-list': names,
                    'phone-list': phones}
    for list_id, values in list_id_dict.items():
      if values is not None:
        list = DynamicList(self.driver,
                           self.dialog_elem.find_element_by_id(list_id))
        list.RemoveAll()
        for value in values:
          list.Add(value)

    if country_code is not None:
      self.dialog_elem.find_element_by_xpath(
          './/*[@id="country"]/*[@value="%s"]' % country_code).click()

  def GetStateLabel(self):
    """Returns the label used for the state text field."""
    return self.dialog_elem.find_element_by_id('state-label').text

  def GetPostalCodeLabel(self):
    """Returns the label used for the postal code text field."""
    return self.dialog_elem.find_element_by_id('postal-code-label').text

  def GetPhones(self):
    """Returns a list of the phone numbers in the phones list."""
    list = DynamicList(
        self.driver, self.dialog_elem.find_element_by_id('phone-list'))
    return list.GetCommittedItems()
