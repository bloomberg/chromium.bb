# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import types

import selenium.common.exceptions
from selenium.webdriver.common.action_chains import ActionChains
from selenium.webdriver.support.ui import WebDriverWait


def _FocusField(driver, list_elem, field_elem):
  """Focuses a field in a dynamic list.

  Note, the item containing the field should not be focused already.

  Typing into a field is tricky because the js automatically focuses and
  selects the text field after 50ms after it first receives focus. This
  method focuses the field and waits for the timeout to occur.
  For more info, see inline_editable_list.js and search for setTimeout.
  See crbug.com/97369.

  Args:
    list_elem: An element in the HTML list.
    field_elem: An element in the HTML text field.

  Raises:
    RuntimeError: If a timeout occurs when waiting for the focus event.
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
  driver.set_script_timeout(5)
  try:
    driver.execute_async_script(correct_focus_script, list_elem, field_elem)
  except selenium.common.exceptions.TimeoutException:
    raise RuntimeError('Unable to focus list item ' + field_elem.tag_name)


class Item(object):
  """A list item web element."""
  def __init__(self, elem):
    self._elem = elem

  def Remove(self, driver):
    button = self._elem.find_element_by_xpath('./button')
    ActionChains(driver).move_to_element(button).click().perform()


class TextFieldsItem(Item):
  """An item consisting only of text fields."""
  def _GetFields(self):
    """Returns the text fields list."""
    return self._elem.find_elements_by_tag_name('input')

  def Set(self, values):
    """Sets the value(s) of the item's text field(s).

    Args:
      values: The new value or the list of the new values of the fields.
    """
    field_list = self._GetFields()
    if len(field_list) > 1:
      assert type(values) == types.ListType, \
          """The values must be a list for a HTML list that has multi-field
          items. '%s' should be in a list.""" % values
      value_list = values
    else:
      value_list = [values]

    assert len(field_list) == len(value_list), \
        """The item to be added must have the same number of fields as an item
        in the HTML list. Given item '%s' should have %s fields.""" % (
            value_list, len(field_list))
    for field, value in zip(field_list, value_list):
      field.clear()
      field.send_keys(value)
    field_list[-1].send_keys('\n') # press enter on the last field.

  def Get(self):
    """Returns the list of the text field values."""
    return map(lambda f: f.get_attribute('value'), self._GetFields())


class TextField(object):
  """A text field web element."""
  def __init__(self, elem):
    self._elem = elem

  def Set(self, value):
    """Sets the value of the text field.

    Args:
      value: The new value of the field.
    """
    self._elem.clear()
    self._elem.send_keys(value)

  def Get(self):
    """Returns the value of the text field."""
    return self._elem.get_attribute('value')


class List(object):
  """A web element that holds a list of items."""

  def __init__(self, driver, elem, item_class=Item):
    """item element is an element in the HTML list.
    item class is the class of item the list holds."""
    self._driver = driver
    self._elem = elem
    self._item_class = item_class

  def RemoveAll(self):
    """Removes all items from the list.

    In the loop the removal of an elem renders the remaining elems of the list
    invalid. After each item is removed, GetItems() is called.
    """
    for i in range(len(self.GetItems())):
      self.GetItems()[0].Remove(self._driver)

  def GetItems(self):
    """Returns all the items that are in the list."""
    items = self._GetItemElems()
    return map(lambda x: self._item_class(x), items)

  def GetSize(self):
    """Returns the number of items in the list."""
    return len(self._GetItemElems())

  def _GetItemElems(self):
    return self._elem.find_elements_by_xpath('.//*[@role="listitem"]')


class DynamicList(List):
  """A web element that holds a dynamic list of items of text fields.

  Terminology:
    item element: an element in the HTML list item.
    item_class: the class of item the list holds
    placeholder: the last item element in the list, which is not committed yet

  The user can add new items to the list by typing in the placeholder item.
  When a user presses enter or focuses something else, the placeholder item
  is committed and a new placeholder is created. An item may contain 1 or
  more text fields.
  """

  def __init__(self, driver, elem, item_class=TextFieldsItem):
    return super(DynamicList, self).__init__(
        driver, elem, item_class=item_class)

  def GetPlaceholderItem(self):
    return self.GetItems()[-1]

  def GetCommittedItems(self):
    """Returns all the items that are in the list, except the placeholder."""
    return map(lambda x: self._item_class(x), self._GetCommittedItemElems())

  def GetSize(self):
    """Returns the number of items in the list, excluding the placeholder."""
    return len(self._GetCommittedItemElems())

  def _GetCommittedItemElems(self):
    return self._GetItemElems()[:-1]

  def _GetPlaceholderElem(self):
    return self._GetItemElems()[-1]


class AutofillEditAddressDialog(object):
  """The overlay for editing an autofill address."""

  _URL = 'chrome://settings-frame/autofillEditAddress'

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
          list.GetPlaceholderItem().Set(value)

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
    return [item.Get()[0] for item in list.GetCommittedItems()]


class ContentTypes(object):
  COOKIES = 'cookies'
  IMAGES = 'images'
  JAVASCRIPT = 'javascript'
  HANDLERS = 'handlers'
  PLUGINS = 'plugins'
  POPUPS = 'popups'
  GEOLOCATION = 'location'
  NOTIFICATIONS = 'notifications'
  PASSWORDS = 'passwords'


class Behaviors(object):
  ALLOW = 'allow'
  SESSION_ONLY = 'session_only'
  ASK = 'ask'
  BLOCK = 'block'


class ContentSettingsPage(object):
  """The overlay for managing exceptions on the Content Settings page."""

  _URL = 'chrome://settings-frame/content'

  @staticmethod
  def FromNavigation(driver):
    """Creates an instance of the dialog by navigating directly to it."""
    driver.get(ContentSettingsPage._URL)
    return ContentSettingsPage(driver)

  def __init__(self, driver):
    assert self._URL == driver.current_url
    self.page_elem = driver.find_element_by_id(
        'content-settings-page')

  def SetContentTypeOption(self, content_type, option):
    """Set the option for the specified content type.

    Args:
      content_type: The content type to manage.
      option: The option to allow, deny or ask.
    """
    self.page_elem.find_element_by_xpath(
        './/*[@name="%s"][@value="%s"]' % (content_type, option)).click()


class ManageExceptionsPage(object):
  """The overlay for the content exceptions page."""

  @staticmethod
  def FromNavigation(driver, content_type):
    """Creates an instance of the dialog by navigating directly to it.

    Args:
      driver: The remote WebDriver instance to manage some content type.
      content_type: The content type to manage.
    """
    content_url = 'chrome://settings-frame/contentExceptions#%s' % content_type
    driver.get(content_url)
    return ManageExceptionsPage(driver, content_type)

  def __init__(self, driver, content_type):
    self._list_elem = driver.find_element_by_xpath(
        './/*[@id="content-settings-exceptions-area"]'
        '//*[@contenttype="%s"]//list[@role="list"]'
        '[@class="settings-list"]' % content_type)
    self._driver = driver
    self._content_type = content_type
    try:
      self._incognito_list_elem = driver.find_element_by_xpath(
          './/*[@id="content-settings-exceptions-area"]'
          '//*[@contenttype="%s"]//div[not(@hidden)]'
          '//list[@mode="otr"][@role="list"]'
          '[@class="settings-list"]' % content_type)
    except selenium.common.exceptions.NoSuchElementException:
      self._incognito_list_elem = None

  def _AssertIncognitoAvailable(self):
    if not self._incognito_list_elem:
      raise AssertionError(
          'Incognito settings in "%s" content page not available'
          % self._content_type)

  def _GetExceptionList(self, incognito):
    if not incognito:
      list_elem = self._list_elem
    else:
      list_elem = self._incognito_list_elem
    return DynamicList(self._driver, list_elem)

  def _GetPatternList(self, incognito):
    if not incognito:
      list_elem = self._list_elem
    else:
      list_elem = self._incognito_list_elem
    pattern_list = [p.text for p in
        list_elem.find_elements_by_xpath(
            './/*[contains(@class, "exception-pattern")]'
            '//*[@class="static-text"]')]
    return pattern_list

  def AddNewException(self, pattern, behavior, incognito=False):
    """Add a new pattern and behavior to the Exceptions page.

    Args:
      pattern: Hostname pattern string.
      behavior: Setting for the hostname pattern (Allow, Block, Session Only).
      incognito: Incognito list box. Display to false.

    Raises:
      AssertionError when an exception cannot be added on the content page.
    """
    if incognito:
      self._AssertIncognitoAvailable()
      list_elem = self._incognito_list_elem
    else:
      list_elem = self._list_elem
    # Select behavior first.
    try:
      list_elem.find_element_by_xpath(
          './/*[@class="exception-setting"]'
          '[not(@displaymode)]//option[@value="%s"]'
             % behavior).click()
    except selenium.common.exceptions.NoSuchElementException:
      raise AssertionError(
          'Adding new exception not allowed in "%s" content page'
          % self._content_type)
    # Set pattern now.
    self._GetExceptionList(incognito).GetPlaceholderItem().Set(pattern)

  def DeleteException(self, pattern, incognito=False):
    """Delete the exception for the selected hostname pattern.

    Args:
      pattern: Hostname pattern string.
      incognito: Incognito list box. Default to false.
    """
    if incognito:
      self._AssertIncognitoAvailable()
    list = self._GetExceptionList(incognito)
    items = filter(lambda item: item.Get()[0] == pattern,
                   list.GetComittedItems())
    map(lambda item: item.Remove(self._driver), items)

  def GetExceptions(self, incognito=False):
    """Returns a dictionary of {pattern: behavior}.

    Example: {'file:///*': 'block'}

    Args:
      incognito: Incognito list box. Default to false.
    """
    if incognito:
      self._AssertIncognitoAvailable()
      list_elem = self._incognito_list_elem
    else:
      list_elem = self._list_elem
    pattern_list = self._GetPatternList(incognito)
    behavior_list = list_elem.find_elements_by_xpath(
        './/*[@role="listitem"][@class="deletable-item"]'
        '//*[@class="exception-setting"][@displaymode="static"]')
    assert len(pattern_list) == len(behavior_list), \
           'Number of patterns does not match the behaviors.'
    return dict(zip(pattern_list, [b.text.lower() for b in behavior_list]))

  def GetBehaviorForPattern(self, pattern, incognito=False):
    """Returns the behavior for a given pattern on the Exceptions page.

    Args:
      pattern: Hostname pattern string.
      incognito: Incognito list box. Default to false.
     """
    if incognito:
      self._AssertIncognitoAvailable()
    assert self.GetExceptions(incognito).has_key(pattern), \
           'No displayed host name matches pattern "%s"' % pattern
    return self.GetExceptions(incognito)[pattern]

  def SetBehaviorForPattern(self, pattern, behavior, incognito=False):
    """Set the behavior for the selected pattern on the Exceptions page.

    Args:
      pattern: Hostname pattern string.
      behavior: Setting for the hostname pattern (Allow, Block, Session Only).
      incognito: Incognito list box. Default to false.

    Raises:
      AssertionError when the behavior cannot be changed on the content page.
    """
    if incognito:
      self._AssertIncognitoAvailable()
      list_elem = self._incognito_list_elem
    else:
      list_elem = self._list_elem
    pattern_list = self._GetPatternList(incognito)
    listitem_list = list_elem.find_elements_by_xpath(
        './/*[@role="listitem"][@class="deletable-item"]')
    pattern_listitem_dict = dict(zip(pattern_list, listitem_list))
    # Set focus to appropriate listitem.
    listitem_elem = pattern_listitem_dict[pattern]
    listitem_elem.click()
    # Set behavior.
    try:
      listitem_elem.find_element_by_xpath(
          './/option[@value="%s"]' % behavior).click()
    except selenium.common.exceptions.ElementNotVisibleException:
      raise AssertionError(
          'Changing the behavior is invalid for pattern '
          '"%s" in "%s" content page' % (behavior, self._content_type))
    # Send enter key.
    pattern_elem = listitem_elem.find_element_by_tag_name('input')
    pattern_elem.send_keys('\n')


class RestoreOnStartupType(object):
  NEW_TAB_PAGE = 5
  RESTORE_SESSION = 1
  RESTORE_URLS = 4


class BasicSettingsPage(object):
  """The basic settings page."""
  _URL = 'chrome://settings-frame/settings'

  @staticmethod
  def FromNavigation(driver):
    """Creates an instance of BasicSetting page by navigating to it."""
    driver.get(BasicSettingsPage._URL)
    return BasicSettingsPage(driver)

  def __init__(self, driver):
    self._driver = driver
    assert self._URL == driver.current_url

  def SetOnStartupOptions(self, on_startup_option):
    """Set on-startup options.

    Args:
      on_startup_option: option types for on start up settings.

    Raises:
      AssertionError when invalid startup option type is provided.
    """
    if on_startup_option == RestoreOnStartupType.NEW_TAB_PAGE:
      startup_option_elem = self._driver.find_element_by_id('startup-newtab')
    elif on_startup_option == RestoreOnStartupType.RESTORE_SESSION:
      startup_option_elem = self._driver.find_element_by_id(
          'startup-restore-session')
    elif on_startup_option == RestoreOnStartupType.RESTORE_URLS:
      startup_option_elem = self._driver.find_element_by_id(
          'startup-show-pages')
    else:
      raise AssertionError('Invalid value for restore start up option!')
    startup_option_elem.click()

  def _GoToStartupSetPages(self):
    self._driver.find_element_by_id('startup-set-pages').click()

  def _FillStartupURL(self, url):
    list = DynamicList(self._driver, self._driver.find_element_by_id(
                       'startupPagesList'))
    list.GetPlaceholderItem().Set(url + '\n')

  def AddStartupPage(self, url):
    """Add a startup URL.

    Args:
      url: A startup url.
    """
    self._GoToStartupSetPages()
    self._FillStartupURL(url)
    self._driver.find_element_by_id('startup-overlay-confirm').click()
    self._driver.get(self._URL)

  def UseCurrentPageForStartup(self, title_list):
    """Use current pages and verify page url show up in settings.

    Args:
      title_list: startup web page title list.
    """
    self._GoToStartupSetPages()
    self._driver.find_element_by_id('startupUseCurrentButton').click()
    self._driver.find_element_by_id('startup-overlay-confirm').click()
    def is_current_page_visible(driver):
      title_elem_list = driver.find_elements_by_xpath(
          '//*[contains(@class, "title")][text()="%s"]' % title_list[0])
      if len(title_elem_list) == 0:
        return False
      return True
    WebDriverWait(self._driver, 10).until(is_current_page_visible)
    self._driver.get(self._URL)

  def VerifyStartupURLs(self, title_list):
    """Verify saved startup URLs appear in set page UI.

    Args:
      title_list: A list of startup page title.

    Raises:
      AssertionError when start up URLs do not appear in set page UI.
    """
    self._GoToStartupSetPages()
    for i in range(len(title_list)):
      try:
        self._driver.find_element_by_xpath(
            '//*[contains(@class, "title")][text()="%s"]' % title_list[i])
      except selenium.common.exceptions.NoSuchElementException:
        raise AssertionError("Current page %s did not appear as startup page."
            % title_list[i])
    self._driver.find_element_by_id('startup-overlay-cancel').click()

  def CancelStartupURLSetting(self, url):
    """Cancel start up URL settings.

    Args:
      url: A startup url.
    """
    self._GoToStartupSetPages()
    self._FillStartupURL(url)
    self._driver.find_element_by_id('startup-overlay-cancel').click()
    self._driver.get(self._URL)


class PasswordsSettings(object):
  """The overlay for managing passwords on the Content Settings page."""

  _URL = 'chrome://settings-frame/passwords'

  class PasswordsItem(Item):
    """A list of passwords item web element."""
    def _GetFields(self):
      """Returns the field list element."""
      return self._elem.find_elements_by_xpath('./div/*')

    def GetSite(self):
      """Returns the site field value."""
      return self._GetFields()[0].text

    def GetUsername(self):
      """Returns the username field value."""
      return self._GetFields()[1].text


  @staticmethod
  def FromNavigation(driver):
    """Creates an instance of the dialog by navigating directly to it.

    Args:
      driver: The remote WebDriver instance to manage some content type.
    """
    driver.get(PasswordsSettings._URL)
    return PasswordsSettings(driver)

  def __init__(self, driver):
    self._driver = driver
    assert self._URL == driver.current_url
    list_elem = driver.find_element_by_id('saved-passwords-list')
    self._items_list = List(self._driver, list_elem, self.PasswordsItem)

  def DeleteItem(self, url, username):
    """Deletes a line entry in Passwords Content Settings.

    Args:
      url: The URL string as it appears in the UI.
      username: The username string as it appears in the second column.
    """
    for password_item in self._items_list.GetItems():
      if (password_item.GetSite() == url and
          password_item.GetUsername() == username):
        password_item.Remove(self._driver)


class CookiesAndSiteDataSettings(object):
  """The overlay for managing cookies on the Content Settings page."""

  _URL = 'chrome://settings-frame/cookies'

  @staticmethod
  def FromNavigation(driver):
    """Creates an instance of the dialog by navigating directly to it.

    Args:
      driver: The remote WebDriver instance for managing content type.
    """
    driver.get(CookiesAndSiteDataSettings._URL)
    return CookiesAndSiteDataSettings(driver)

  def __init__(self, driver):
    self._driver = driver
    assert self._URL == driver.current_url
    self._list_elem = driver.find_element_by_id('cookies-list')

  def GetSiteNameList(self):
    """Returns a list of the site names.

    This is a public function since the test needs to check if the site is
    deleted.
    """
    site_list = [p.text for p in
                 self._list_elem.find_elements_by_xpath(
                     './/*[contains(@class, "deletable-item")]'
                     '//div[@class="cookie-site"]')]
    return site_list

  def _GetCookieNameList(self):
    """Returns a list where each item is the list of cookie names of each site.

    Example: site1 | cookie1 cookie2
             site2 | cookieA
             site3 | cookieA cookie1 cookieB

    Returns:
      A cookie names list such as:
      [ ['cookie1', 'cookie2'], ['cookieA'], ['cookieA', 'cookie1', 'cookieB'] ]
    """
    cookie_name_list = []
    for elem in self._list_elem.find_elements_by_xpath(
        './/*[@role="listitem"]'):
      elem.click()
      cookie_name_list.append([c.text for c in
            elem.find_elements_by_xpath('.//div[@class="cookie-item"]')])
    return cookie_name_list

  def DeleteSiteData(self, site):
    """Delete a site entry with its cookies in cookies content settings.

    Args:
      site: The site string as it appears in the UI.
    """
    delete_button_list = self._list_elem.find_elements_by_class_name(
        'row-delete-button')
    site_list = self.GetSiteNameList()
    for i in range(len(site_list)):
      if site_list[i] == site:
        # Highlight the item so the close button shows up, then delete button
        # shows up, then click on the delete button.
        ActionChains(self._driver).move_to_element(
            delete_button_list[i]).click().perform()
