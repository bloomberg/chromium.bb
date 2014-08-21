# -*- coding: utf-8 -*-
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Automated tests for many websites"""

import argparse
import logging

from environment import Environment
from websitetest import WebsiteTest


class TypeOfTestedWebsites:
  """An enum to specify which groups of tests to run."""
  # Runs only the disabled tests.
  DISABLED_TESTS = 0
  # Runs only the enabled tests.
  ENABLED_TESTS = 1
  # Runs all the tests.
  ALL_TESTS = 2
  # Runs a specified list of tests.
  LIST_OF_TESTS = 3

  def __init__(self):
    pass


class Facebook(WebsiteTest):

  def Login(self):
    self.GoTo("https://www.facebook.com")
    self.FillUsernameInto("[name='email']")
    self.FillPasswordInto("[name='pass']")
    self.Submit("[name='pass']")


class Google(WebsiteTest):

  def Login(self):
    self.GoTo("https://accounts.google.com/ServiceLogin?sacu=1&continue=")
    self.WaitUntilDisplayed("#Email")
    self.FillUsernameInto("#Email")
    self.FillPasswordInto("#Passwd")
    self.Submit("#Passwd")


class Linkedin(WebsiteTest):

  def Login(self):
    self.GoTo("https://www.linkedin.com")
    self.FillUsernameInto("#session_key-login")
    self.FillPasswordInto("#session_password-login")
    self.Submit("#session_password-login")


class Mailru(WebsiteTest):

  def Login(self):
    self.GoTo("https://mail.ru")
    self.FillUsernameInto("#mailbox__login")
    self.FillPasswordInto("#mailbox__password")
    self.Submit("#mailbox__password")


class Nytimes(WebsiteTest):

  def Login(self):
    self.GoTo("https://myaccount.nytimes.com/auth/login")
    self.FillUsernameInto("#userid")
    self.FillPasswordInto("#password")
    self.Submit("#password")


class Pinterest(WebsiteTest):

  def Login(self):
    self.GoTo("https://www.pinterest.com/login/")
    self.FillUsernameInto("[name='username_or_email']")
    self.FillPasswordInto("[name='password']")
    self.Submit("[name='password']")


class Reddit(WebsiteTest):

  def Login(self):
    self.GoTo("http://www.reddit.com")
    self.Click(".user .login-required")
    self.FillUsernameInto("#user_login")
    self.FillPasswordInto("#passwd_login")
    self.Wait(2)
    self.Submit("#passwd_login")


class Tumblr(WebsiteTest):

  def Login(self):
    self.GoTo("https://www.tumblr.com/login")
    self.FillUsernameInto("#signup_email")
    self.FillPasswordInto("#signup_password")
    self.Submit("#signup_password")


class Twitter(WebsiteTest):

  def Login(self):
    self.GoTo("https:///twitter.com")
    self.WaitUntilDisplayed("#signin-email")
    self.FillUsernameInto("#signin-email")
    self.FillPasswordInto("#signin-password")
    self.Submit("#signin-password")


class Wikipedia(WebsiteTest):

  def Login(self):
    self.GoTo("https://en.wikipedia.org/w/index.php?title=Special:UserLogin")
    self.FillUsernameInto("#wpName1")
    self.FillPasswordInto("#wpPassword1")
    self.Submit("#wpPassword1")


class Yahoo(WebsiteTest):

  def Login(self):
    self.GoTo("https://login.yahoo.com")
    self.FillUsernameInto("#username")
    self.FillPasswordInto("#passwd")
    self.Submit("#passwd")


class Yandex(WebsiteTest):

  def Login(self):
    self.GoTo("https://mail.yandex.com")
    self.FillUsernameInto("#b-mail-domik-username11")
    self.FillPasswordInto("#b-mail-domik-password11")
    self.Click(".b-mail-button__button")


# Disabled tests.


# Bug not reproducible without test.
class Amazon(WebsiteTest):

  def Login(self):
    self.GoTo(
        "https://www.amazon.com/ap/signin?openid.assoc_handle=usflex"
        "&openid.mode=checkid_setup&openid.ns=http%3A%2F%2Fspecs.openid.net"
        "%2Fauth%2F2.0")
    self.FillUsernameInto("[name='email']")
    self.FillPasswordInto("[name='password']")
    self.Click("#signInSubmit-input")


# Password not saved.
class Ask(WebsiteTest):

  def Login(self):
    self.GoTo("http://www.ask.com/answers/browse?qsrc=321&q=&o=0&l=dir#")
    while not self.IsDisplayed("[name='username']"):
      self.Click("#a16CnbSignInText")
      self.Wait(1)
    self.FillUsernameInto("[name='username']")
    self.FillPasswordInto("[name='password']")
    self.Click(".signin_show.signin_submit")


# Password not saved.
class Baidu(WebsiteTest):

  def Login(self):
    self.GoTo("https://passport.baidu.com")
    self.WaitUntilDisplayed("[name='userName']")
    self.FillUsernameInto("[name='userName']")
    self.FillPasswordInto("[name='password']")
    self.Submit("[name='password']")


# http://crbug.com/368690
class Cnn(WebsiteTest):

  def Login(self):
    self.GoTo("http://www.cnn.com")
    self.Wait(5)
    while not self.IsDisplayed(".cnnOvrlyBtn.cnnBtnLogIn"):
      self.ClickIfClickable("#hdr-auth .no-border.no-pad-right a")
      self.Wait(1)

    self.Click(".cnnOvrlyBtn.cnnBtnLogIn")
    self.FillUsernameInto("#cnnOverlayEmail1l")
    self.FillPasswordInto("#cnnOverlayPwd")
    self.Click(".cnnOvrlyBtn.cnnBtnLogIn")
    self.Click(".cnnOvrlyBtn.cnnBtnLogIn")
    self.Wait(5)


# http://crbug.com/368690
class Ebay(WebsiteTest):

  def Login(self):
    self.GoTo("https://signin.ebay.com/")
    self.WaitUntilDisplayed("[name='userid']")
    self.FillUsernameInto("[name='userid']")
    self.FillPasswordInto("[name='pass']")
    self.Submit("[name='pass']")


# Iframe, password saved but not autofileld.
class Espn(WebsiteTest):

  def Login(self):
    self.GoTo("http://espn.go.com/")
    while not self.IsDisplayed("#cboxLoadedContent iframe"):
      self.Click("#signin .cbOverlay")
      self.Wait(1)
    frame = self.driver.find_element_by_css_selector("#cboxLoadedContent "
                                                     "iframe")
    self.driver.switch_to_frame(frame)
    self.WaitUntilDisplayed("#username")
    self.FillUsernameInto("#username")
    self.FillPasswordInto("#password")
    while self.IsDisplayed("#password"):
      self.ClickIfClickable("#submitBtn")
      self.Wait(1)


# http://crbug.com/367768
class Live(WebsiteTest):

  def Login(self):
    self.GoTo("https://login.live.com")
    self.WaitUntilDisplayed("[name='login']")
    self.FillUsernameInto("[name='login']")
    self.FillPasswordInto("[name='passwd']")
    self.Submit("[name='passwd']")


# http://crbug.com/368690
class One63(WebsiteTest):

  def Login(self):
    self.GoTo("http://www.163.com")
    self.WaitUntilDisplayed("#js_N_navHighlight")
    self.HoverOver("#js_N_navHighlight")
    self.WaitUntilDisplayed("#js_loginframe_username")
    self.FillUsernameInto("#js_loginframe_username")
    self.FillPasswordInto(".ntes-loginframe-label-ipt[type='password']")
    self.Click(".ntes-loginframe-btn")


# http://crbug.com/368690
class Vube(WebsiteTest):

  def Login(self):
    self.GoTo("https://vube.com")
    self.WaitUntilDisplayed("[vube-login='']")
    self.Click("[vube-login='']")
    self.WaitUntilDisplayed("[ng-model='login.user']")
    self.FillUsernameInto("[ng-model='login.user']")
    self.FillPasswordInto("[ng-model='login.pass']")
    while (self.IsDisplayed("[ng-model='login.pass']")
           and not self.IsDisplayed(".prompt.alert")):
      self.ClickIfClickable("[ng-click='login()']")
      self.Wait(1)


def Tests(environment):


  # Working tests.


  environment.AddWebsiteTest(Facebook("facebook"))

  environment.AddWebsiteTest(Google("google"))

  environment.AddWebsiteTest(Linkedin("linkedin"))

  environment.AddWebsiteTest(Mailru("mailru"))

  environment.AddWebsiteTest(Nytimes("nytimes"))

  environment.AddWebsiteTest(Pinterest("pinterest"))

  environment.AddWebsiteTest(Reddit("reddit", username_not_auto=True))

  environment.AddWebsiteTest(Tumblr("tumblr", username_not_auto=True))

  environment.AddWebsiteTest(Twitter("twitter"))

  environment.AddWebsiteTest(Wikipedia("wikipedia", username_not_auto=True))

  environment.AddWebsiteTest(Yahoo("yahoo", username_not_auto=True))

  environment.AddWebsiteTest(Yandex("yandex"))

  # Disabled tests.


  # Bug not reproducible without test.
  environment.AddWebsiteTest(Amazon("amazon"), disabled=True)

  # Password not saved.
  environment.AddWebsiteTest(Ask("ask"), disabled=True)

  # Password not saved.
  environment.AddWebsiteTest(Baidu("baidu"), disabled=True)

  # http://crbug.com/368690
  environment.AddWebsiteTest(Cnn("cnn"), disabled=True)

  # http://crbug.com/368690
  environment.AddWebsiteTest(Ebay("ebay"), disabled=True)

  # Iframe, password saved but not autofileld.
  environment.AddWebsiteTest(Espn("espn"), disabled=True)

  # http://crbug.com/367768
  environment.AddWebsiteTest(Live("live", username_not_auto=True),
                             disabled=True)

  # http://crbug.com/368690
  environment.AddWebsiteTest(One63("163"), disabled=True)

  # http://crbug.com/368690
  environment.AddWebsiteTest(Vube("vube"), disabled=True)

def saveResults(environment_tests_results, environment_save_path):
  """Save the test results in an xml file.

  Args:
    environment_tests_results: A list of the TestResults that are going to be
        saved.
    environment_save_path: The file where the results are going to be saved.
        If it's None, the results are not going to be stored.
  Raises:
    Exception: An exception is raised if the file is not found.
  """
  if environment_save_path:
    xml = "<result>"
    for test_result in environment_tests_results:
      xml += ("<test name='%s' successful='%s' type='%s'>%s</test>"
          % (test_result.name, str(test_result.successful),
          test_result.test_type, test_result.message))
    xml += "</result>"
    with open(environment_save_path, "w") as save_file:
      save_file.write(xml)

def RunTests(chrome_path, chromedriver_path, profile_path,
             environment_passwords_path, enable_automatic_password_saving,
             environment_numeric_level, log_to_console, environment_log_file,
             environment_tested_websites, tests=None):

  """Runs the the tests

  Args:
    chrome_path: The chrome binary file.
    chromedriver_path: The chromedriver binary file.
    profile_path: The chrome testing profile folder.
    environment_passwords_path: The usernames and passwords file.
    enable_automatic_password_saving: If True, the passwords are going to be
        saved without showing the prompt.
    environment_numeric_level: The log verbosity.
    log_to_console: If True, the debug logs will be shown on the console.
    environment_log_file: The file where to store the log. If it's empty, the
        log is not stored.
    environment_tested_websites: One of the TypeOfTestedWebsites values,
        indicating which group of tests to run.
    tests: Specifies which tests to run. Ignored unless
       |environment_tested_websites| is equal to LIST_OF_TESTS.

  Returns:
    The results of tests as list of TestResults.
  Raises:
    Exception: An exception is raised if one of the tests fails.
  """

  environment = Environment(chrome_path, chromedriver_path, profile_path,
                            environment_passwords_path,
                            enable_automatic_password_saving,
                            environment_numeric_level,
                            log_to_console,
                            environment_log_file)

  # Test which care about the save-password prompt need the prompt
  # to be shown. Automatic password saving results in no prompt.
  run_prompt_tests = not enable_automatic_password_saving

  Tests(environment)

  if environment_tested_websites == TypeOfTestedWebsites.ALL_TESTS:
    environment.AllTests(run_prompt_tests)
  elif environment_tested_websites == TypeOfTestedWebsites.DISABLED_TESTS:
    environment.DisabledTests(run_prompt_tests)
  elif environment_tested_websites == TypeOfTestedWebsites.LIST_OF_TESTS:
    environment.Test(tests, run_prompt_tests)
  elif environment_tested_websites == TypeOfTestedWebsites.ENABLED_TESTS:
    environment.WorkingTests(run_prompt_tests)
  else:
    raise Exception("Error: |environment_tested_websites| has to be one of the"
        "TypeOfTestedWebsites values")


  environment.Quit()
  return environment.tests_results

# Tests setup.
if __name__ == "__main__":
  parser = argparse.ArgumentParser(
      description="Password Manager automated tests help.")

  parser.add_argument(
      "--chrome-path", action="store", dest="chrome_path",
      help="Set the chrome path (required).", nargs=1, required=True)
  parser.add_argument(
      "--chromedriver-path", action="store", dest="chromedriver_path",
      help="Set the chromedriver path (required).", nargs=1, required=True)
  parser.add_argument(
      "--profile-path", action="store", dest="profile_path",
      help="Set the profile path (required). You just need to choose a "
           "temporary empty folder. If the folder is not empty all its content "
           "is going to be removed.",
      nargs=1, required=True)

  parser.add_argument(
      "--passwords-path", action="store", dest="passwords_path",
      help="Set the usernames/passwords path (required).", nargs=1,
      required=True)
  parser.add_argument("--all", action="store_true", dest="all",
                      help="Run all tests.")
  parser.add_argument("--disabled", action="store_true", dest="disabled",
                      help="Run only disabled tests.")
  parser.add_argument("--log", action="store", nargs=1, dest="log_level",
                      help="Set log level.")
  parser.add_argument("--log-screen", action="store_true", dest="log_screen",
                      help="Show log on the screen.")
  parser.add_argument("--log-file", action="store", dest="log_file",
                      help="Write the log in a file.", nargs=1)
  parser.add_argument("--save-path", action="store", nargs=1, dest="save_path",
                      help="Write the results in a file.")
  parser.add_argument("tests", help="Tests to be run.",  nargs="*")

  args = parser.parse_args()

  passwords_path = args.passwords_path[0]

  if args.all:
    tested_websites = TypeOfTestedWebsites.ALL_TESTS
  elif args.disabled:
    tested_websites = TypeOfTestedWebsites.DISABLED_TESTS
  elif args.tests:
    tested_websites = TypeOfTestedWebsites.LIST_OF_TESTS
  else:
    tested_websites = TypeOfTestedWebsites.ENABLED_TESTS

  numeric_level = None
  if args.log_level:
    numeric_level = getattr(logging, args.log_level[0].upper(), None)
    if not isinstance(numeric_level, int):
      raise ValueError("Invalid log level: %s" % args.log_level[0])

  log_file = None
  if args.log_file:
    log_file = args.log_file[0]

  save_path = None
  if args.save_path:
    save_path = args.save_path[0]

  # Run the test without enable-automatic-password-saving to check whether or
  # not the prompt is shown in the way we expected.
  tests_results = RunTests(args.chrome_path[0],
                           args.chromedriver_path[0],
                           args.profile_path[0],
                           passwords_path,
                           False,
                           numeric_level,
                           args.log_screen,
                           log_file,
                           tested_websites,
                           args.tests)

  # Run the test with enable-automatic-password-saving to check whether or not
  # the passwords is stored in the the way we expected.
  tests_results += RunTests(args.chrome_path[0],
                            args.chromedriver_path[0],
                            args.profile_path[0],
                            passwords_path,
                            True,
                            numeric_level,
                            args.log_screen,
                            log_file,
                            tested_websites,
                            args.tests)

  saveResults(tests_results, save_path)
