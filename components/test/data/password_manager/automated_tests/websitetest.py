# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""WebsiteTest testing class."""

import logging
import time

from selenium.webdriver.common.action_chains import ActionChains
from selenium.webdriver.common.keys import Keys

import environment

SCRIPT_DEBUG = 9  # TODO(vabr) -- make this consistent with run_tests.py.

class WebsiteTest:
  """WebsiteTest testing class.

  Represents one website, defines some generic operations on that site.
  To customise for a particular website, this class needs to be inherited
  and the Login() method overridden.
  """

  # Possible values of self.autofill_expectation.
  AUTOFILLED = 1  # Expect password and username to be autofilled.
  NOT_AUTOFILLED = 2  # Expect password and username not to be autofilled.

  # The maximal accumulated time to spend in waiting for website UI
  # interaction.
  MAX_WAIT_TIME_IN_SECONDS = 200

  def __init__(self, name, username_not_auto=False, password_not_auto=False):
    """Creates a new WebsiteTest.

    Args:
      name: The website name, identifying it in the test results.
      username_not_auto: Expect that the tested website fills username field
        on load, and Chrome cannot autofill in that case.
      password_not_auto: Expect that the tested website fills password field
        on load, and Chrome cannot autofill in that case.
    """
    self.name = name
    self.username = None
    self.password = None
    self.username_not_auto = username_not_auto
    self.password_not_auto = password_not_auto

    # Specify, whether it is expected that credentials get autofilled.
    self.autofill_expectation = WebsiteTest.NOT_AUTOFILLED
    self.remaining_seconds_to_wait = WebsiteTest.MAX_WAIT_TIME_IN_SECONDS
    # The testing Environment, if added to any.
    self.environment = None
    # The webdriver from the environment.
    self.driver = None

  # Mouse/Keyboard actions.

  def Click(self, selector):
    """Clicks on the element described by |selector|.

    Args:
      selector: The clicked element's CSS selector.
    """

    logging.log(SCRIPT_DEBUG, "action: Click %s" % selector)
    element = self.WaitUntilDisplayed(selector)
    element.click()

  def ClickIfClickable(self, selector):
    """Clicks on the element described by |selector| if it is clickable.

    The driver's find_element_by_css_selector method defines what is clickable
    -- anything for which it does not throw, is clickable. To be clickable,
    the element must:
      * exist in the DOM,
      * be not covered by another element
      * be inside the visible area.
    Note that transparency does not influence clickability.

    Args:
      selector: The clicked element's CSS selector.

    Returns:
      True if the element is clickable (and was clicked on).
      False otherwise.
    """

    logging.log(SCRIPT_DEBUG, "action: ClickIfVisible %s" % selector)
    element = self.WaitUntilDisplayed(selector)
    try:
      element.click()
      return True
    except Exception:
      return False

  def GoTo(self, url):
    """Navigates the main frame to |url|.

    Args:
      url: The URL of where to go to.
    """

    logging.log(SCRIPT_DEBUG, "action: GoTo %s" % self.name)
    self.driver.get(url)

  def HoverOver(self, selector):
    """Hovers over the element described by |selector|.

    Args:
      selector: The CSS selector of the element to hover over.
    """

    logging.log(SCRIPT_DEBUG, "action: Hover %s" % selector)
    element = self.WaitUntilDisplayed(selector)
    hover = ActionChains(self.driver).move_to_element(element)
    hover.perform()

  # Waiting/Displaying actions.

  def _ReturnElementIfDisplayed(self, selector):
    """Returns the element described by |selector|, if displayed.

    Note: This takes neither overlapping among elements nor position with
    regards to the visible area into account.

    Args:
      selector: The CSS selector of the checked element.

    Returns:
      The element if displayed, None otherwise.
    """

    try:
      element = self.driver.find_element_by_css_selector(selector)
      return element if element.is_displayed() else None
    except Exception:
      return None

  def IsDisplayed(self, selector):
    """Check if the element described by |selector| is displayed.

    Note: This takes neither overlapping among elements nor position with
    regards to the visible area into account.

    Args:
      selector: The CSS selector of the checked element.

    Returns:
      True if the element is in the DOM and less than 100% transparent.
      False otherwise.
    """

    logging.log(SCRIPT_DEBUG, "action: IsDisplayed %s" % selector)
    return self._ReturnElementIfDisplayed(selector) is not None

  def Wait(self, duration):
    """Wait for |duration| in seconds.

    To avoid deadlocks, the accummulated waiting time for the whole object does
    not exceed MAX_WAIT_TIME_IN_SECONDS.

    Args:
      duration: The time to wait in seconds.

    Raises:
      Exception: In case the accummulated waiting limit is exceeded.
    """

    logging.log(SCRIPT_DEBUG, "action: Wait %s" % duration)
    self.remaining_seconds_to_wait -= duration
    if self.remaining_seconds_to_wait < 0:
      raise Exception("Waiting limit exceeded for website: %s" % self.name)
    time.sleep(duration)

  # TODO(vabr): Pull this out into some website-utils and use in Environment
  # also?
  def WaitUntilDisplayed(self, selector):
    """Waits until the element described by |selector| is displayed.

    Args:
      selector: The CSS selector of the element to wait for.

    Returns:
      The displayed element.
    """

    element = self._ReturnElementIfDisplayed(selector)
    while not element:
      self.Wait(1)
      element = self._ReturnElementIfDisplayed(selector)
    return element

  # Form actions.

  def FillPasswordInto(self, selector):
    """Ensures that the selected element's value is the saved password.

    Depending on self.autofill_expectation, this either checks that the
    element already has the password autofilled, or checks that the value
    is empty and replaces it with the password. If self.password_not_auto
    is true, it skips the checks and just overwrites the value with the
    password.

    Args:
      selector: The CSS selector for the filled element.

    Raises:
      Exception: An exception is raised if the element's value is different
          from the expectation.
    """

    logging.log(SCRIPT_DEBUG, "action: FillPasswordInto %s" % selector)
    password_element = self.WaitUntilDisplayed(selector)

    # Chrome protects the password inputs and doesn't fill them until
    # the user interacts with the page. To be sure that such thing has
    # happened we perform |Keys.CONTROL| keypress.
    action_chains = ActionChains(self.driver)
    action_chains.key_down(Keys.CONTROL).key_up(Keys.CONTROL).perform()

    self.Wait(2)  # TODO(vabr): Detect when autofill finished.
    if not self.password_not_auto:
      if self.autofill_expectation == WebsiteTest.AUTOFILLED:
        if password_element.get_attribute("value") != self.password:
          raise Exception("Error: autofilled password is different from the"
                          "saved one on website: %s" % self.name)
      elif self.autofill_expectation == WebsiteTest.NOT_AUTOFILLED:
        if password_element.get_attribute("value"):
          raise Exception("Error: password value unexpectedly not empty on"
                          "website: %s" % self.name)

    password_element.clear()
    password_element.send_keys(self.password)

  def FillUsernameInto(self, selector):
    """Ensures that the selected element's value is the saved username.

    Depending on self.autofill_expectation, this either checks that the
    element already has the username autofilled, or checks that the value
    is empty and replaces it with the password. If self.username_not_auto
    is true, it skips the checks and just overwrites the value with the
    username.

    Args:
      selector: The CSS selector for the filled element.

    Raises:
      Exception: An exception is raised if the element's value is different
          from the expectation.
    """

    logging.log(SCRIPT_DEBUG, "action: FillUsernameInto %s" % selector)
    username_element = self.WaitUntilDisplayed(selector)

    self.Wait(2)  # TODO(vabr): Detect when autofill finished.
    if not self.username_not_auto:
      if self.autofill_expectation == WebsiteTest.AUTOFILLED:
        if username_element.get_attribute("value") != self.username:
          raise Exception("Error: filled username different from the saved"
                          " one on website: %s" % self.name)
        return
      if self.autofill_expectation == WebsiteTest.NOT_AUTOFILLED:
        if username_element.get_attribute("value"):
          raise Exception("Error: username value unexpectedly not empty on"
                          "website: %s" % self.name)

    username_element.clear()
    username_element.send_keys(self.username)

  def Submit(self, selector):
    """Finds an element using CSS |selector| and calls its submit() handler.

    Args:
      selector: The CSS selector for the element to call submit() on.
    """

    logging.log(SCRIPT_DEBUG, "action: Submit %s" % selector)
    element = self.WaitUntilDisplayed(selector)
    element.submit()

  # Login/Logout methods

  def Login(self):
    """Login Method. Has to be overridden by the WebsiteTest test."""

    raise NotImplementedError("Login is not implemented.")

  def LoginWhenAutofilled(self):
    """Logs in and checks that the password is autofilled."""

    self.autofill_expectation = WebsiteTest.AUTOFILLED
    self.Login()

  def LoginWhenNotAutofilled(self):
    """Logs in and checks that the password is not autofilled."""

    self.autofill_expectation = WebsiteTest.NOT_AUTOFILLED
    self.Login()

  def Logout(self):
    self.environment.DeleteCookies()

  # Test scenarios

  def PromptFailTest(self):
    """Checks that prompt is not shown on a failed login attempt.

    Tries to login with a wrong password and checks that the password
    is not offered for saving.

    Raises:
      Exception: An exception is raised if the test fails.
    """

    logging.log(SCRIPT_DEBUG, "PromptFailTest for %s" % self.name)
    correct_password = self.password
    # Hardcoded random wrong password. Chosen by fair `pwgen` call.
    # For details, see: http://xkcd.com/221/.
    self.password = "ChieF2ae"
    self.LoginWhenNotAutofilled()
    self.password = correct_password
    self.environment.CheckForNewString(
        [environment.MESSAGE_ASK, environment.MESSAGE_SAVE],
        False,
        "Error: did not detect wrong login on website: %s" % self.name)

  def PromptSuccessTest(self):
    """Checks that prompt is shown on a successful login attempt.

    Tries to login with a correct password and checks that the password
    is offered for saving. Chrome cannot have the auto-save option on
    when running this test.

    Raises:
      Exception: An exception is raised if the test fails.
    """

    logging.log(SCRIPT_DEBUG, "PromptSuccessTest for %s" % self.name)
    if not self.environment.show_prompt:
      raise Exception("Switch off auto-save during PromptSuccessTest.")
    self.LoginWhenNotAutofilled()
    self.environment.CheckForNewString(
        [environment.MESSAGE_ASK],
        True,
        "Error: did not detect login success on website: %s" % self.name)

  def SaveAndAutofillTest(self):
    """Checks that a correct password is saved and autofilled.

    Tries to login with a correct password and checks that the password
    is saved and autofilled on next visit. Chrome must have the auto-save
    option on when running this test.

    Raises:
      Exception: An exception is raised if the test fails.
    """

    logging.log(SCRIPT_DEBUG, "SaveAndAutofillTest for %s" % self.name)
    if self.environment.show_prompt:
      raise Exception("Switch off auto-save during PromptSuccessTest.")
    self.LoginWhenNotAutofilled()
    self.environment.CheckForNewString(
        [environment.MESSAGE_SAVE],
        True,
        "Error: did not detect login success on website: %s" % self.name)
    self.Logout()
    self.LoginWhenAutofilled()
    self.environment.CheckForNewString(
        [environment.MESSAGE_SAVE],
        True,
        "Error: failed autofilled login on website: %s" % self.name)

