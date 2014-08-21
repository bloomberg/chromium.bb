# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""WebsiteTest testing class."""

import logging
import sys
import time

sys.path.insert(0, '../../../../third_party/webdriver/pylib/')

from selenium.webdriver.common.action_chains import ActionChains
from selenium.webdriver.common.keys import Keys

import environment


def _IsOneSubstringOfAnother(s1, s2):
  """Checks if one of the string arguements is substring of the other.

  Args:
      s1: The first string.
      s2: The second string.
  Returns:

    True if one of the string arguements is substring of the other.
    False otherwise.
  """
  return s1 in s2 or s2 in s1


class WebsiteTest:
  """Handles a tested WebsiteTest."""

  class Mode:
    """Test mode."""
    # Password and username are expected to be autofilled.
    AUTOFILLED = 1
    # Password and username are not expected to be autofilled.
    NOT_AUTOFILLED = 2

    def __init__(self):
      pass

  def __init__(self, name, username_not_auto=False):
    """Creates a new WebsiteTest.

    Args:
      name: The website name.
      username_not_auto: Username inputs in some websites (like wikipedia) are
          sometimes filled with some messages and thus, the usernames are not
          automatically autofilled. This flag handles that and disables us from
          checking if the state of the DOM is the same as the username of
          website.
    """
    # Name of the website
    self.name = name
    # Username of the website.
    self.username = None
    # Password of the website.
    self.password = None
    # Username is not automatically filled.
    self.username_not_auto = username_not_auto
    # Autofilling mode.
    self.mode = self.Mode.NOT_AUTOFILLED
    # The |remaining_time_to_wait| limits the total time in seconds spent in
    # potentially infinite loops.
    self.remaining_time_to_wait = 200
    # The testing Environment.
    self.environment = None
    # The webdriver.
    self.driver = None
    # Whether or not the test was run.
    self.was_run = False

  # Mouse/Keyboard actions.

  def Click(self, selector):
    """Clicks on an element.

    Args:
      selector: The element CSS selector.
    """
    logging.info("action: Click %s" % selector)
    element = self.driver.find_element_by_css_selector(selector)
    element.click()

  def ClickIfClickable(self, selector):
    """Clicks on an element if it's clickable: If it doesn't exist in the DOM,
    it's covered by another element or it's out viewing area, nothing is
    done and False is returned. Otherwise, even if the element is 100%
    transparent, the element is going to receive a click and a True is
    returned.

    Args:
      selector: The element CSS selector.

    Returns:
      True if the click happens.
      False otherwise.
    """
    logging.info("action: ClickIfVisible %s" % selector)
    try:
      element = self.driver.find_element_by_css_selector(selector)
      element.click()
      return True
    except Exception:
      return False

  def GoTo(self, url):
    """Navigates the main frame to the |url|.

    Args:
      url: The URL.
    """
    logging.info("action: GoTo %s" % self.name)
    if self.environment.first_go_to:
      self.environment.OpenTabAndGoToInternals(url)
      self.environment.first_go_to = False
    else:
      self.driver.get(url)

  def HoverOver(self, selector):
    """Hovers over an element.

    Args:
      selector: The element CSS selector.
    """
    logging.info("action: Hover %s" % selector)
    element = self.driver.find_element_by_css_selector(selector)
    hover = ActionChains(self.driver).move_to_element(element)
    hover.perform()

  def SendEnterTo(self, selector):
    """Sends an enter key to an element.

    Args:
      selector: The element CSS selector.
    """
    logging.info("action: SendEnterTo %s" % selector)
    body = self.driver.find_element_by_tag_name("body")
    body.send_keys(Keys.ENTER)

  # Waiting/Displaying actions.

  def IsDisplayed(self, selector):
    """Returns False if an element doesn't exist in the DOM or is 100%
    transparent. Otherwise, returns True even if it's covered by another
    element or it's out viewing area.

    Args:
      selector: The element CSS selector.
    """
    logging.info("action: IsDisplayed %s" % selector)
    try:
      element = self.driver.find_element_by_css_selector(selector)
      return element.is_displayed()
    except Exception:
      return False

  def Wait(self, duration):
    """Wait for a duration in seconds. This needs to be used in potentially
    infinite loops, to limit their running time.

    Args:
      duration: The time to wait in seconds.
    """
    logging.info("action: Wait %s" % duration)
    time.sleep(duration)
    self.remaining_time_to_wait -= 1
    if self.remaining_time_to_wait < 0:
      raise Exception("Tests took more time than expected for the following "
                      "website : %s \n" % self.name)

  def WaitUntilDisplayed(self, selector, timeout=10):
    """Waits until an element is displayed.

    Args:
      selector: The element CSS selector.
      timeout: The maximum waiting time in seconds before failing.
    """
    if not self.IsDisplayed(selector):
      self.Wait(1)
      timeout = timeout - 1
      if (timeout <= 0):
        raise Exception("Error: Element %s not shown before timeout is "
                        "finished for the following website: %s"
                        % (selector, self.name))
      else:
        self.WaitUntilDisplayed(selector, timeout)

  # Form actions.

  def FillPasswordInto(self, selector):
    """If the testing mode is the Autofilled mode, compares the website
    password to the DOM state.
    If the testing mode is the NotAutofilled mode, checks that the DOM state
    is empty.
    Then, fills the input with the Website password.

    Args:
      selector: The password input CSS selector.

    Raises:
      Exception: An exception is raised if the DOM value of the password is
          different than the one we expected.
    """
    logging.info("action: FillPasswordInto %s" % selector)

    password_element = self.driver.find_element_by_css_selector(selector)
    # Chrome protects the password inputs and doesn't fill them until
    # the user interacts with the page. To be sure that such thing has
    # happened we perform |Keys.CONTROL| keypress.
    action_chains = ActionChains(self.driver)
    action_chains.key_down(Keys.CONTROL).key_up(Keys.CONTROL).perform()
    if self.mode == self.Mode.AUTOFILLED:
      autofilled_password = password_element.get_attribute("value")
      if autofilled_password != self.password:
        raise Exception("Error: autofilled password is different from the one "
                        "we just saved for the following website : %s p1: %s "
                        "p2:%s \n" % (self.name,
                                      password_element.get_attribute("value"),
                                      self.password))

    elif self.mode == self.Mode.NOT_AUTOFILLED:
      autofilled_password = password_element.get_attribute("value")
      if autofilled_password:
        raise Exception("Error: password is autofilled when it shouldn't  be "
                        "for the following website : %s \n"
                        % self.name)

      password_element.send_keys(self.password)

  def FillUsernameInto(self, selector):
    """If the testing mode is the Autofilled mode, compares the website
    username to the input value. Then, fills the input with the website
    username.

    Args:
      selector: The username input CSS selector.

    Raises:
      Exception: An exception is raised if the DOM value of the username is
          different that the one we expected.
    """
    logging.info("action: FillUsernameInto %s" % selector)
    username_element = self.driver.find_element_by_css_selector(selector)

    if (self.mode == self.Mode.AUTOFILLED and not self.username_not_auto):
      if not (username_element.get_attribute("value") == self.username):
        raise Exception("Error: autofilled username is different form the one "
                        "we just saved for the following website : %s \n" %
                        self.name)
    else:
      username_element.clear()
      username_element.send_keys(self.username)

  def Submit(self, selector):
    """Finds an element using CSS Selector and calls its submit() handler.

    Args:
      selector: The input CSS selector.
    """
    logging.info("action: Submit %s" % selector)
    element = self.driver.find_element_by_css_selector(selector)
    element.submit()

  # Login/Logout Methods

  def Login(self):
    """Login Method. Has to be overloaded by the WebsiteTest test."""
    raise NotImplementedError("Login is not implemented.")

  def LoginWhenAutofilled(self):
    """Logs in and checks that the password is autofilled."""
    self.mode = self.Mode.AUTOFILLED
    self.Login()

  def LoginWhenNotAutofilled(self):
    """Logs in and checks that the password is not autofilled."""
    self.mode = self.Mode.NOT_AUTOFILLED
    self.Login()

  def Logout(self):
    """Logout Method."""
    self.environment.ClearAllCookies()

  # Tests

  def WrongLoginTest(self):
    """Does the wrong login test: Tries to login with a wrong password and
    checks that the password is not saved.

    Raises:
      Exception: An exception is raised if the test fails: If there is a
          problem when performing the login (ex: the login button is not
          available ...), if the state of the username and password fields is
          not like we expected or if the password is saved.
    """
    logging.info("\nWrong Login Test for %s \n" % self.name)
    correct_password = self.password
    self.password = self.password + "1"
    self.LoginWhenNotAutofilled()
    self.password = correct_password
    self.Wait(2)
    self.environment.SwitchToInternals()
    self.environment.CheckForNewMessage(
        environment.MESSAGE_SAVE,
        False,
        "Error: password manager thinks that a login with wrong password was "
        "successful for the following website : %s \n" % self.name)
    self.environment.SwitchFromInternals()

  def SuccessfulLoginTest(self):
    """Does the successful login when the password is not expected to be
    autofilled test: Checks that the password is not autofilled, tries to login
    with a right password and checks if the password is saved. Then logs out.

    Raises:
      Exception: An exception is raised if the test fails: If there is a
          problem when performing the login and the logout (ex: the login
          button is not available ...), if the state of the username and
          password fields is not like we expected or if the password is not
          saved.
    """
    logging.info("\nSuccessful Login Test for %s \n" % self.name)
    self.LoginWhenNotAutofilled()
    self.Wait(2)
    self.environment.SwitchToInternals()
    self.environment.CheckForNewMessage(
        environment.MESSAGE_SAVE,
        True,
        "Error: password manager hasn't detected a successful login for the "
        "following website : %s \n"
        % self.name)
    self.environment.SwitchFromInternals()
    self.Logout()

  def SuccessfulLoginWithAutofilledPasswordTest(self):
    """Does the successful login when the password is expected to be autofilled
    test: Checks that the password is autofilled, tries to login with the
    autofilled password and checks if the password is saved. Then logs out.

    Raises:
      Exception: An exception is raised if the test fails: If there is a
          problem when performing the login and the logout (ex: the login
          button is not available ...), if the state of the username and
          password fields is not like we expected or if the password is not
          saved.
    """
    logging.info("\nSuccessful Login With Autofilled Password"
                        " Test %s \n" % self.name)
    self.LoginWhenAutofilled()
    self.Wait(2)
    self.environment.SwitchToInternals()
    self.environment.CheckForNewMessage(
        environment.MESSAGE_SAVE,
        True,
        "Error: password manager hasn't detected a successful login for the "
        "following website : %s \n"
        % self.name)
    self.environment.SwitchFromInternals()
    self.Logout()

  def PromptTest(self):
    """Does the prompt test: Tries to login with a wrong password and
    checks that the prompt is not shown. Then tries to login with a right
    password and checks that the prompt is not shown.

    Raises:
      Exception: An exception is raised if the test fails: If there is a
          problem when performing the login (ex: the login button is not
          available ...), if the state of the username and password fields is
          not like we expected or if the prompt is not shown for the right
          password or is shown for a wrong one.
    """
    logging.info("\nPrompt Test for %s \n" % self.name)
    correct_password = self.password
    self.password = self.password + "1"
    self.LoginWhenNotAutofilled()
    self.password = correct_password
    self.Wait(2)
    self.environment.SwitchToInternals()
    self.environment.CheckForNewMessage(
        environment.MESSAGE_ASK,
        False,
        "Error: password manager thinks that a login with wrong password was "
        "successful for the following website : %s \n" % self.name)
    self.environment.SwitchFromInternals()

    self.LoginWhenNotAutofilled()
    self.Wait(2)
    self.environment.SwitchToInternals()
    self.environment.CheckForNewMessage(
        environment.MESSAGE_ASK,
        True,
        "Error: password manager hasn't detected a successful login for the "
        "following website : %s \n" % self.name)
    self.environment.SwitchFromInternals()
