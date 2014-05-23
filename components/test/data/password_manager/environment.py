# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The testing Environment class."""

import logging
import shutil
import time

from selenium import webdriver
from selenium.common.exceptions import NoSuchElementException
from selenium.common.exceptions import WebDriverException
from selenium.webdriver.chrome.options import Options
from xml.etree import ElementTree

# Message strings to look for in chrome://password-manager-internals
MESSAGE_ASK = "Message: Decision: ASK the user"
MESSAGE_SAVE = "Message: Decision: SAVE the password"


class Environment:
  """Sets up the testing Environment. """

  def __init__(self, chrome_path, chromedriver_path, profile_path,
               passwords_path, enable_automatic_password_saving,
               numeric_level=None, log_to_console=False, log_file=""):
    """Creates a new testing Environment.

    Args:
      chrome_path: The chrome binary file.
      chromedriver_path: The chromedriver binary file.
      profile_path: The chrome testing profile folder.
      passwords_path: The usernames and passwords file.
      enable_automatic_password_saving: If True, the passwords are going to be
          saved without showing the prompt.
      numeric_level: The log verbosity.
      log_to_console: If True, the debug logs will be shown on the console.
      log_file: The file where to store the log. If it's empty, the log will
            not be stored.

    Raises:
      Exception: An exception is raised if |profile_path| folder could not be
      removed.
    """
    # Setting up the login.
    if numeric_level is not None:
      if log_file:
        # Set up logging to file.
        logging.basicConfig(level=numeric_level,
                            filename=log_file,
                            filemode='w')

        if log_to_console:
          console = logging.StreamHandler()
          console.setLevel(numeric_level)
          # Add the handler to the root logger.
          logging.getLogger('').addHandler(console)

      elif log_to_console:
        logging.basicConfig(level=numeric_level)

    # Cleaning the chrome testing profile folder.
    try:
      shutil.rmtree(profile_path)
    except Exception, e:
      # The tests execution can continue, but this make them less stable.
      logging.error("Error: Could not wipe the chrome profile directory (%s). \
          This affects the stability of the tests. Continuing to run tests."
          % e)
    options = Options()
    if enable_automatic_password_saving:
      options.add_argument("enable-automatic-password-saving")
    # Chrome path.
    options.binary_location = chrome_path
    # Chrome testing profile path.
    options.add_argument("user-data-dir=%s" % profile_path)

    # The webdriver. It's possible to choose the port the service is going to
    # run on. If it's left to 0, a free port will be found.
    self.driver = webdriver.Chrome(chromedriver_path, 0, options)
    # The password internals window.
    self.internals_window = self.driver.current_window_handle
    # Password internals page.
    self.internals_page = "chrome://password-manager-internals/"
    # The Website window.
    self.website_window = None
    # The WebsiteTests list.
    self.websitetests = []
    # An xml tree filled with logins and passwords.
    self.passwords_tree = ElementTree.parse(passwords_path).getroot()
    # The enabled WebsiteTests list.
    self.working_tests = []
    # Map messages to the number of their appearance in the log.
    self.message_count = dict()
    self.message_count[MESSAGE_ASK] = 0
    self.message_count[MESSAGE_SAVE] = 0
    # The tests needs two tabs to work. A new tab is opened with the first
    # GoTo. This is why we store here whether or not it's the first time to
    # execute GoTo.
    self.first_go_to = True

  def AddWebsiteTest(self, websitetest, disabled=False):
    """Adds a WebsiteTest to the testing Environment.

    Args:
      websitetest: The WebsiteTest instance to be added.
      disabled: Whether test is disabled.
    """
    websitetest.environment = self
    websitetest.driver = self.driver
    if self.passwords_tree is not None:
      if not websitetest.username:
        username_tag = (
            self.passwords_tree.find(
                ".//*[@name='%s']/username" % websitetest.name))
        if username_tag.text:
          websitetest.username = username_tag.text
      if not websitetest.password:
        password_tag = (
            self.passwords_tree.find(
                ".//*[@name='%s']/password" % websitetest.name))
        if password_tag.text:
          websitetest.password = password_tag.text
    self.websitetests.append(websitetest)
    if not disabled:
      self.working_tests.append(websitetest.name)

  def RemoveAllPasswords(self):
    """Removes all the stored passwords."""
    logging.info("\nRemoveAllPasswords\n")
    self.driver.get("chrome://settings/passwords")
    self.driver.switch_to_frame("settings")
    while True:
      try:
        self.driver.execute_script("document.querySelector('"
          "#saved-passwords-list .row-delete-button').click()")
        time.sleep(1)
      except NoSuchElementException:
        break
      except WebDriverException:
        break

  def OpenTabAndGoToInternals(self, url):
    """If there is no |self.website_window|, opens a new tab and navigates to
    |url| in the new tab. Navigates to the passwords internals page in the
    first tab. Raises an exception otherwise.

    Args:
      url: Url to go to in the new tab.

    Raises:
      Exception: An exception is raised if |self.website_window| already
          exists.
    """
    if self.website_window:
      raise Exception("Error: The window was already opened.")

    self.driver.get("chrome://newtab")
    # There is no straightforward way to open a new tab with chromedriver.
    # One work-around is to go to a website, insert a link that is going
    # to be opened in a new tab, click on it.
    a = self.driver.execute_script(
        "var a = document.createElement('a');"
        "a.target = '_blank';"
        "a.href = arguments[0];"
        "a.innerHTML = '.';"
        "document.body.appendChild(a);"
        "return a;",
        url)

    a.click()
    time.sleep(1)

    self.website_window = self.driver.window_handles[-1]
    self.driver.get(self.internals_page)
    self.driver.switch_to_window(self.website_window)

  def SwitchToInternals(self):
    """Switches from the Website window to internals tab."""
    self.driver.switch_to_window(self.internals_window)

  def SwitchFromInternals(self):
    """Switches from internals tab to the Website window."""
    self.driver.switch_to_window(self.website_window)

  def _DidMessageAppearUntilTimeout(self, log_message, timeout):
    """Checks whether the save password prompt is shown.

    Args:
      log_message: Log message to look for in the password internals.
      timeout: There is some delay between the login and the password
          internals update. The method checks periodically during the first
          |timeout| seconds if the internals page reports the prompt being
          shown. If the prompt is not reported shown within the first
          |timeout| seconds, it is considered not shown at all.

    Returns:
      True if the save password prompt is shown.
      False otherwise.
    """
    log = self.driver.find_element_by_css_selector("#log-entries")
    count = log.text.count(log_message)

    if count > self.message_count[log_message]:
      self.message_count[log_message] = count
      return True
    elif timeout > 0:
      time.sleep(1)
      return self._DidMessageAppearUntilTimeout(log_message, timeout - 1)
    else:
      return False

  def CheckForNewMessage(self, log_message, message_should_show_up,
                         error_message, timeout=3):
    """Detects whether the save password prompt is shown.

    Args:
      log_message: Log message to look for in the password internals. The
          only valid values are the constants MESSAGE_* defined at the
          beginning of this file.
      message_should_show_up: Whether or not the message is expected to be
          shown.
      error_message: Error message for the exception.
      timeout: There is some delay between the login and the password
          internals update. The method checks periodically during the first
          |timeout| seconds if the internals page reports the prompt being
          shown. If the prompt is not reported shown within the first
          |timeout| seconds, it is considered not shown at all.

    Raises:
      Exception: An exception is raised in case the result does not match the
          expectation
    """
    if (self._DidMessageAppearUntilTimeout(log_message, timeout) !=
        message_should_show_up):
      raise Exception(error_message)

  def AllTests(self, prompt_test):
    """Runs the tests on all the WebsiteTests.

    Args:
      prompt_test: If True, tests caring about showing the save-password
          prompt are going to be run, otherwise tests which don't care about
          the prompt are going to be run.

    Raises:
      Exception: An exception is raised if the tests fail.
    """
    if prompt_test:
      self.PromptTestList(self.websitetests)
    else:
      self.TestList(self.websitetests)

  def WorkingTests(self, prompt_test):
    """Runs the tests on all the enabled WebsiteTests.

    Args:
      prompt_test: If True, tests caring about showing the save-password
          prompt are going to be run, otherwise tests which don't care about
          the prompt are going to be executed.

    Raises:
      Exception: An exception is raised if the tests fail.
    """
    self.Test(self.working_tests, prompt_test)

  def Test(self, tests, prompt_test):
    """Runs the tests on websites named in |tests|.

    Args:
      tests: A list of the names of the WebsiteTests that are going to be
          tested.
      prompt_test: If True, tests caring about showing the save-password
          prompt are going to be run, otherwise tests which don't care about
          the prompt are going to be executed.

    Raises:
      Exception: An exception is raised if the tests fail.
    """
    websitetests = []
    for websitetest in self.websitetests:
      if websitetest.name in tests:
        websitetests.append(websitetest)

    if prompt_test:
      self.PromptTestList(websitetests)
    else:
      self.TestList(websitetests)

  def TestList(self, websitetests):
    """Runs the tests on the websites in |websitetests|.

    Args:
      websitetests: A list of WebsiteTests that are going to be tested.

    Raises:
      Exception: An exception is raised if the tests fail.
    """
    self.RemoveAllPasswords()

    for websitetest in websitetests:
      websitetest.WrongLoginTest()
      websitetest.SuccessfulLoginTest()
      websitetest.SuccessfulLoginWithAutofilledPasswordTest()

    self.RemoveAllPasswords()
    for websitetest in websitetests:
      websitetest.SuccessfulLoginTest()

  def PromptTestList(self, websitetests):
    """Runs the prompt tests on the websites in |websitetests|.

    Args:
      websitetests: A list of WebsiteTests that are going to be tested.

    Raises:
      Exception: An exception is raised if the tests fail.
    """
    self.RemoveAllPasswords()

    for websitetest in websitetests:
      websitetest.PromptTest()

  def Quit(self):
    """Closes the tests."""
    # Close the webdriver.
    self.driver.quit()
