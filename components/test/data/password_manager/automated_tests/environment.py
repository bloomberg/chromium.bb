# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The testing Environment class.

It holds the WebsiteTest instances, provides them with credentials,
provides clean browser environment, runs the tests, and gathers the
results.
"""

import os
import shutil
import time
from xml.etree import ElementTree

from selenium import webdriver
from selenium.webdriver.chrome.options import Options


# Message strings to look for in chrome://password-manager-internals.
MESSAGE_ASK = "Message: Decision: ASK the user"
MESSAGE_SAVE = "Message: Decision: SAVE the password"

INTERNALS_PAGE_URL = "chrome://password-manager-internals/"

class Environment:
  """Sets up the testing Environment. """

  def __init__(self, chrome_path, chromedriver_path, profile_path,
               passwords_path, enable_automatic_password_saving):
    """Creates a new testing Environment, starts Chromedriver.

    Args:
      chrome_path: The chrome binary file.
      chromedriver_path: The chromedriver binary file.
      profile_path: The chrome testing profile folder.
      passwords_path: The usernames and passwords file.
      enable_automatic_password_saving: If True, the passwords are going to be
          saved without showing the prompt.

    Raises:
      IOError: When the passwords file cannot be accessed.
      ParseError: When the passwords file cannot be parsed.
      Exception: An exception is raised if |profile_path| folder could not be
      removed.
    """

    # Cleaning the chrome testing profile folder.
    if os.path.exists(profile_path):
      shutil.rmtree(profile_path)

    options = Options()
    if enable_automatic_password_saving:
      options.add_argument("enable-automatic-password-saving")
    # TODO(vabr): show_prompt is used in WebsiteTest for asserting that
    # Chrome set-up corresponds to the test type. Remove that knowledge
    # about Environment from the WebsiteTest.
    self.show_prompt = not enable_automatic_password_saving
    options.binary_location = chrome_path
    options.add_argument("user-data-dir=%s" % profile_path)

    # The webdriver. It's possible to choose the port the service is going to
    # run on. If it's left to 0, a free port will be found.
    self.driver = webdriver.Chrome(chromedriver_path, 0, options)

    # Password internals page tab/window handle.
    self.internals_window = self.driver.current_window_handle

    # An xml tree filled with logins and passwords.
    self.passwords_tree = ElementTree.parse(passwords_path).getroot()

    self.website_window = self._OpenNewTab()

    self.websitetests = []

    # Map messages to the number of their appearance in the log.
    self.message_count = { MESSAGE_ASK: 0, MESSAGE_SAVE: 0 }

    # A list of (test_name, test_type, test_success, failure_log).
    self.tests_results = []

  def AddWebsiteTest(self, websitetest):
    """Adds a WebsiteTest to the testing Environment.

    TODO(vabr): Currently, this is only called at most once for each
    Environment instance. That is because to run all tests efficiently in
    parallel, each test gets its own process spawned (outside of Python).
    That makes sense, but then we should flatten the hierarchy of calls
    and consider making the 1:1 relation of environment to tests more
    explicit.

    Args:
      websitetest: The WebsiteTest instance to be added.
    """
    websitetest.environment = self
    # TODO(vabr): Make driver a property of WebsiteTest.
    websitetest.driver = self.driver
    if not websitetest.username:
      username_tag = (self.passwords_tree.find(
          ".//*[@name='%s']/username" % websitetest.name))
      websitetest.username = username_tag.text
    if not websitetest.password:
      password_tag = (self.passwords_tree.find(
          ".//*[@name='%s']/password" % websitetest.name))
      websitetest.password = password_tag.text
    self.websitetests.append(websitetest)

  def _ClearBrowserDataInit(self):
    """Opens and resets the chrome://settings/clearBrowserData dialog.

    It unchecks all checkboxes, and sets the time range to the "beginning of
    time".
    """

    self.driver.get("chrome://settings-frame/clearBrowserData")

    time_range_selector = "#clear-browser-data-time-period"
    # TODO(vabr): Wait until time_range_selector is displayed instead.
    time.sleep(2)
    set_time_range = (
        "var range = document.querySelector('{0}');".format(
            time_range_selector) +
        "range.value = 4"  # 4 == the beginning of time
        )
    self.driver.execute_script(set_time_range)

    all_cboxes_selector = (
        "#clear-data-checkboxes [type=\"checkbox\"]")
    uncheck_all = (
        "var checkboxes = document.querySelectorAll('{0}');".format(
            all_cboxes_selector ) +
        "for (var i = 0; i < checkboxes.length; ++i) {"
        "  checkboxes[i].checked = false;"
        "}"
        )
    self.driver.execute_script(uncheck_all)

  def _ClearDataForCheckbox(self, selector):
    """Causes the data associated with |selector| to be cleared.

    Opens chrome://settings/clearBrowserData, unchecks all checkboxes, then
    checks the one described by |selector|, then clears the corresponding
    browsing data for the full time range.

    Args:
      selector: describes the checkbox through which to delete the data.
    """

    self._ClearBrowserDataInit()
    check_cookies_and_submit = (
        "document.querySelector('{0}').checked = true;".format(selector) +
        "document.querySelector('#clear-browser-data-commit').click();"
        )
    self.driver.execute_script(check_cookies_and_submit)

  def _EnablePasswordSaving(self):
    """Make sure that password manager is enabled."""

    # TODO(melandory): We should check why it's off in a first place.
    # TODO(melandory): Investigate, maybe there is no need to enable it that
    # often.
    self.driver.get("chrome://settings-frame")
    script = "document.getElementById('advanced-settings-expander').click();"
    self.driver.execute_script(script)
    # TODO(vabr): Wait until element is displayed instead.
    time.sleep(2)
    script = (
        "if (!document.querySelector('#password-manager-enabled').checked) {"
        "  document.querySelector('#password-manager-enabled').click();"
        "}")
    self.driver.execute_script(script)
    time.sleep(2)

  def _OpenNewTab(self):
    """Open a new tab, and loads the internals page in the old tab.

    Returns:
      A handle to the new tab.
    """

    number_old_tabs = len(self.driver.window_handles)
    # There is no straightforward way to open a new tab with chromedriver.
    # One work-around is to go to a website, insert a link that is going
    # to be opened in a new tab, and click on it.
    self.driver.get("about:blank")
    a = self.driver.execute_script(
        "var a = document.createElement('a');"
        "a.target = '_blank';"
        "a.href = 'about:blank';"
        "a.innerHTML = '.';"
        "document.body.appendChild(a);"
        "return a;")
    a.click()
    while number_old_tabs == len(self.driver.window_handles):
      time.sleep(1)  # Wait until the new tab is opened.

    new_tab = self.driver.window_handles[-1]
    self.driver.get(INTERNALS_PAGE_URL)
    self.driver.switch_to_window(new_tab)
    return new_tab

  def _DidStringAppearUntilTimeout(self, strings, timeout):
    """Checks whether some of |strings| appeared in the current page.

    Waits for up to |timeout| seconds until at least one of |strings| is
    shown in the current page. Updates self.message_count with the current
    number of occurrences of the shown string. Assumes that at most
    one of |strings| is newly shown.

    Args:
      strings: A list of strings to look for.
      timeout: If any such string does not appear within the first |timeout|
        seconds, it is considered a no-show.

    Returns:
      True if one of |strings| is observed until |timeout|, False otherwise.
    """

    log = self.driver.find_element_by_css_selector("#log-entries")
    while timeout:
      for string in strings:
        count = log.text.count(string)
        if count > self.message_count[string]:
          self.message_count[string] = count
          return True
      time.sleep(1)
      timeout -= 1
    return False

  def CheckForNewString(self, strings, string_should_show_up, error):
    """Checks that |strings| show up on the internals page as it should.

    Switches to the internals page and looks for a new instances of |strings|
    being shown up there. It checks that |string_should_show_up| is true if
    and only if at leas one string from |strings| shows up, and throws an
    Exception if that check fails.

    Args:
      strings: A list of strings to look for in the internals page.
      string_should_show_up: Whether or not at least one string from |strings|
          is expected to be shown.
      error: Error message for the exception.

    Raises:
      Exception: (See above.)
    """

    self.driver.switch_to_window(self.internals_window)
    try:
      if (self._DidStringAppearUntilTimeout(strings, 15) !=
          string_should_show_up):
        raise Exception(error)
    finally:
      self.driver.switch_to_window(self.website_window)

  def DeleteCookies(self):
    """Deletes cookies via the settings page."""

    self._ClearDataForCheckbox("#delete-cookies-checkbox")

  def RunTestsOnSites(self, test_case_name):
    """Runs the specified test on the known websites.

    Also saves the test results in the environment. Note that test types
    differ in their requirements on whether the save password prompt
    should be displayed. Make sure that such requirements are consistent
    with the enable_automatic_password_saving argument passed to |self|
    on construction.

    Args:
      test_case_name: A test name which is a method of WebsiteTest.
    """

    self.DeleteCookies()
    self._ClearDataForCheckbox("#delete-passwords-checkbox")
    self._EnablePasswordSaving()

    for websitetest in self.websitetests:
      successful = True
      error = ""
      try:
        # TODO(melandory): Implement a decorator for WesiteTest methods
        # which allows to mark them as test cases. And then add a check if
        # test_case_name is a valid test case.
        getattr(websitetest, test_case_name)()
      except Exception as e:
        successful = False
        # httplib.CannotSendRequest doesn't define a message,
        # so type(e).__name__ will at least log exception name as a reason.
        # TODO(melandory): logging.exception(e) produces meaningful result
        # for httplib.CannotSendRequest, so we can try to propagate information
        # that reason is an exception to the logging phase.
        error = "Exception %s %s" % (type(e).__name__, e)
      self.tests_results.append(
          (websitetest.name, test_case_name, successful, error))

  def Quit(self):
    """Shuts down the driver."""

    self.driver.quit()
