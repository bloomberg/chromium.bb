# -*- coding: utf-8 -*-
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Automated tests for many websites"""

import argparse

from environment import Environment
from websitetest import WebsiteTest


TEST_CASES = ("PromptFailTest", "PromptSuccessTest", "SaveAndAutofillTest")


class Alexa(WebsiteTest):

  def Login(self):
    self.GoTo("https://www.alexa.com/secure/login")
    self.FillUsernameInto("#email")
    self.FillPasswordInto("#pwd")
    self.Submit("#pwd")


class Dropbox(WebsiteTest):

  def Login(self):
    self.GoTo("https://www.dropbox.com/login")
    self.FillUsernameInto(".text-input-input[name='login_email']")
    self.FillPasswordInto(".text-input-input[name='login_password']")
    self.Submit(".text-input-input[name='login_password']")


class Facebook(WebsiteTest):

  def Login(self):
    self.GoTo("https://www.facebook.com")
    self.FillUsernameInto("[name='email']")
    self.FillPasswordInto("[name='pass']")
    self.Submit("[name='pass']")


class Github(WebsiteTest):

  def Login(self):
    self.GoTo("https://github.com/login")
    self.FillUsernameInto("[name='login']")
    self.FillPasswordInto("[name='password']")
    self.Submit("[name='commit']")


class Google(WebsiteTest):

  def Login(self):
    self.GoTo("https://accounts.google.com/ServiceLogin?sacu=1&continue=")
    self.FillUsernameInto("#Email")
    self.Submit("#Email")
    self.FillPasswordInto("#Passwd")
    self.Submit("#Passwd")


class Imgur(WebsiteTest):

  def Login(self):
    self.GoTo("https://imgur.com/signin")
    self.FillUsernameInto("[name='username']")
    self.FillPasswordInto("[name='password']")
    self.Submit("[name='password']")


class Liveinternet(WebsiteTest):

  def Login(self):
    self.GoTo("http://liveinternet.ru/journals.php?s=&action1=login")
    self.FillUsernameInto("[name='username']")
    self.FillPasswordInto("[name='password']")
    self.Submit("[name='password']")


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

class Odnoklassniki(WebsiteTest):

  def Login(self):
    self.GoTo("https://ok.ru")
    self.FillUsernameInto("#field_email")
    self.FillPasswordInto("#field_password")
    self.Submit("#field_password")

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
    self.FillUsernameInto("#signin-email")
    self.FillPasswordInto("#signin-password")
    self.Submit("#signin-password")


class Vkontakte(WebsiteTest):

  def Login(self):
    self.GoTo("https:///vk.com")
    self.FillUsernameInto("[name='email']")
    self.FillPasswordInto("[name='pass']")
    self.Submit("[name='pass']")


class Wikia(WebsiteTest):

  def Login(self):
    self.GoTo("https://wikia.com");
    self.Click("#AccountNavigation");
    self.FillUsernameInto("#usernameInput")
    self.FillPasswordInto("#passwordInput")
    self.Submit("input.login-button")


class Wikipedia(WebsiteTest):

  def Login(self):
    self.GoTo("https://en.wikipedia.org/w/index.php?title=Special:UserLogin")
    self.FillUsernameInto("#wpName1")
    self.FillPasswordInto("#wpPassword1")
    self.Submit("#wpPassword1")


class Wordpress(WebsiteTest):

  def Login(self):
    self.GoTo("https://de.wordpress.com/wp-login.php")
    self.FillUsernameInto("[name='log']")
    self.FillPasswordInto("[name='pwd']")
    self.Submit("[name='pwd']")



class Yahoo(WebsiteTest):

  def Login(self):
    self.GoTo("https://login.yahoo.com")
    self.FillUsernameInto("#login-username")
    self.FillPasswordInto("#login-passwd")
    self.Click("#login-signin")


class Yandex(WebsiteTest):

  def Login(self):
    self.GoTo("https://mail.yandex.com")
    self.FillUsernameInto("[name='login']")
    self.FillPasswordInto("[name='passwd']")
    self.Submit("[name='passwd']")


# Fails due to test framework issue(?).
class Aliexpress(WebsiteTest):

  def Login(self):
    self.GoTo("https://login.aliexpress.com/buyer.htm?return=http%3A%2F%2Fwww.aliexpress.com%2F")
    self.WaitUntilDisplayed("iframe#alibaba-login-box")
    frame = self.driver.find_element_by_css_selector("iframe#alibaba-login-box")
    self.driver.switch_to_frame(frame)
    self.FillUsernameInto("#fm-login-id")
    self.FillPasswordInto("#fm-login-password")
    self.Click("#fm-login-submit")


# Fails to save password.
class Adobe(WebsiteTest):

  def Login(self):
    self.GoTo("https://adobeid-na1.services.adobe.com/renga-idprovider/pages/l"
              "ogin?callback=https%3A%2F%2Fims-na1.adobelogin.com%2Fims%2Fadob"
              "eid%2Fadobedotcom2%2FAdobeID%2Ftoken%3Fredirect_uri%3Dhttps%253"
              "A%252F%252Fwww.adobe.com%252F%2523from_ims%253Dtrue%2526old_has"
              "h%253D%2526client_id%253Dadobedotcom2%2526scope%253Dcreative_cl"
              "oud%25252CAdobeID%25252Copenid%25252Cgnav%25252Cread_organizati"
              "ons%25252Cadditional_info.projectedProductContext%2526api%253Da"
              "uthorize&client_id=adobedotcom2&scope=creative_cloud%2CAdobeID%"
              "2Copenid%2Cgnav%2Cread_organizations%2Cadditional_info.projecte"
              "dProductContext&display=web_v2&denied_callback=https%3A%2F%2Fim"
              "s-na1.adobelogin.com%2Fims%2Fdenied%2Fadobedotcom2%3Fredirect_u"
              "ri%3Dhttps%253A%252F%252Fwww.adobe.com%252F%2523from_ims%253Dtr"
              "ue%2526old_hash%253D%2526client_id%253Dadobedotcom2%2526scope%2"
              "53Dcreative_cloud%25252CAdobeID%25252Copenid%25252Cgnav%25252Cr"
              "ead_organizations%25252Cadditional_info.projectedProductContext"
              "%2526api%253Dauthorize%26response_type%3Dtoken&relay=afebfef8-e"
              "2b3-4c0e-9c94-07baf205bae8&locale=en_US&flow_type=token&dc=fals"
              "e&client_redirect=https%3A%2F%2Fims-na1.adobelogin.com%2Fims%2F"
              "redirect%2Fadobedotcom2%3Fclient_redirect%3Dhttps%253A%252F%252"
              "Fwww.adobe.com%252F%2523from_ims%253Dtrue%2526old_hash%253D%252"
              "6client_id%253Dadobedotcom2%2526scope%253Dcreative_cloud%25252C"
              "AdobeID%25252Copenid%25252Cgnav%25252Cread_organizations%25252C"
              "additional_info.projectedProductContext%2526api%253Dauthorize&i"
              "dp_flow_type=login")
    self.FillUsernameInto("[name='username']")
    self.FillPasswordInto("[name='password']")
    self.Submit("#sign_in")


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
    self.FillUsernameInto("[name='userName']")
    self.FillPasswordInto("[name='password']")
    self.Submit("[name='password']")

# Chrome crashes.
class Buzzfeed(WebsiteTest):

  def Login(self):
    self.GoTo("http://www.buzzfeed.com/signin")
    self.FillUsernameInto("#login-username")
    self.FillPasswordInto("#login-password")
    self.Submit("#login-password")


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


# Fails due to "Too many failed logins. Please wait a minute".
# http://crbug.com/466953
class Craigslist(WebsiteTest):

  def Login(self):
    self.GoTo("https://accounts.craigslist.org/login")
    self.FillUsernameInto("#inputEmailHandle")
    self.FillPasswordInto("#inputPassword")
    self.Submit("button")


# Crashes.
class Dailymotion(WebsiteTest):

  def Login(self):
    self.GoTo("http://www.dailymotion.com/gb")
    self.Click(".sd_header__login span")
    self.FillUsernameInto("[name='username']")
    self.FillPasswordInto("[name='password']")
    self.Submit("[name='save']")


# http://crbug.com/368690
class Ebay(WebsiteTest):

  def Login(self):
    self.GoTo("https://signin.ebay.com/")
    self.FillUsernameInto("[name='userid']")
    self.FillPasswordInto("[name='pass']")
    self.Submit("[name='pass']")


# Iframe, password saved but not autofilled.
class Espn(WebsiteTest):

  def Login(self):
    self.GoTo("http://espn.go.com/")
    while not self.IsDisplayed("#cboxLoadedContent iframe"):
      self.Click("#signin .cbOverlay")
      self.Wait(1)
    frame = self.driver.find_element_by_css_selector("#cboxLoadedContent "
                                                     "iframe")
    self.driver.switch_to_frame(frame)
    self.FillUsernameInto("#username")
    self.FillPasswordInto("#password")
    while self.IsDisplayed("#password"):
      self.ClickIfClickable("#submitBtn")
      self.Wait(1)


# Saves wrong username -- http://crbug.com/554004
class Flipkart(WebsiteTest):

  def Login(self):
    self.GoTo("http://www.flipkart.com/")
    while (not self.IsDisplayed(".appDownloadInterstitialDialog .window .close")
           or not self.IsDisplayed(".header-links .js-login")):
      self.Wait(1)
    if self.IsDisplayed(".appDownloadInterstitialDialog .window .close"):
      self.Click(".appDownloadInterstitialDialog .window .close")
    self.Click(".header-links .js-login")
    self.FillUsernameInto(".login-input-wrap .user-email")
    self.FillPasswordInto(".login-input-wrap .user-pwd")
    self.Click(".login-btn-wrap .login-btn")


class Instagram(WebsiteTest):

  def Login(self):
    self.GoTo("https://instagram.com/accounts/login/")
    self.FillUsernameInto("#lfFieldInputUsername")
    self.FillPasswordInto("#lfFieldInputPassword")
    self.Submit(".lfSubmit")


# http://crbug.com/367768
class Live(WebsiteTest):

  def Login(self):
    self.GoTo("https://login.live.com")
    self.FillUsernameInto("[name='login']")
    self.FillPasswordInto("[name='passwd']")
    self.Submit("[name='passwd']")


# http://crbug.com/368690
class One63(WebsiteTest):

  def Login(self):
    self.GoTo("http://www.163.com")
    self.HoverOver("#js_N_navHighlight")
    self.FillUsernameInto("#js_loginframe_username")
    self.FillPasswordInto(".ntes-loginframe-label-ipt[type='password']")
    self.Click(".ntes-loginframe-btn")

class StackExchange(WebsiteTest):

  def Login(self):
    self.GoTo("https://stackexchange.com/users/login#log-in")
    iframe_selector = "#affiliate-signin-iframe"
    self.WaitUntilDisplayed(iframe_selector)
    frame = self.driver.find_element_by_css_selector(iframe_selector)
    self.driver.switch_to_frame(frame)
    self.FillUsernameInto("[name='email']")
    self.FillPasswordInto("[name='password']")
    self.Submit("[value='Sign In']")

# http://crbug.com/368690
class Vube(WebsiteTest):

  def Login(self):
    self.GoTo("https://vube.com")
    self.Click("[vube-login='']")
    self.FillUsernameInto("[ng-model='login.user']")
    self.FillPasswordInto("[ng-model='login.pass']")
    while (self.IsDisplayed("[ng-model='login.pass']")
           and not self.IsDisplayed(".prompt.alert")):
      self.ClickIfClickable("[ng-click='login()']")
      self.Wait(1)


# Password not saved.
class Ziddu(WebsiteTest):

  def Login(self):
    self.GoTo("http://www.ziddu.com/login.php")
    self.FillUsernameInto("#email")
    self.FillPasswordInto("#password")
    self.Click(".login input")


all_tests = {
  "163": One63("163"), # http://crbug.com/368690
  "adobe": Adobe("adobe"), # Password saving not offered.
  "alexa": Alexa("alexa"),
  "aliexpress": Aliexpress("aliexpress"), # Fails due to test framework issue.
  "amazon": Amazon("amazon"), # Bug not reproducible without test.
  "ask": Ask("ask"), # Password not saved.
  "baidu": Baidu("baidu"), # Password not saved.
  "buzzfeed": Buzzfeed("buzzfeed",
                       username_not_auto=True,
                       password_not_auto=True),
  "cnn": Cnn("cnn"), # http://crbug.com/368690
  "craigslist": Craigslist("craigslist"), # Too many failed logins per time.
  "dailymotion": Dailymotion("dailymotion"), # Crashes.
  "dropbox": Dropbox("dropbox"),
  "ebay": Ebay("ebay"), # http://crbug.com/368690
  "espn": Espn("espn"), # Iframe, password saved but not autofilled.
  "facebook": Facebook("facebook"),
  "flipkart": Flipkart("flipkart"), # Fails due to test framework issue.
  "github": Github("github"),
  "google": Google("google"),
  "imgur": Imgur("imgur"),
  "instagram": Instagram("instagram"), # Iframe, pw saved but not autofilled.
  "linkedin": Linkedin("linkedin"),
  "liveinternet": Liveinternet("liveinternet"),
  "live": Live("live", username_not_auto=True),  # http://crbug.com/367768
  "mailru": Mailru("mailru"),
  "nytimes": Nytimes("nytimes"),
  "odnoklassniki": Odnoklassniki("odnoklassniki"),
  "pinterest": Pinterest("pinterest"),
  "reddit": Reddit("reddit", username_not_auto=True),
  "stackexchange": StackExchange("stackexchange"), # Iframe, not autofilled.
  "tumblr": Tumblr("tumblr", username_not_auto=True),
  "twitter": Twitter("twitter"),
  "vkontakte": Vkontakte("vkontakte"),
  "vube": Vube("vube"), # http://crbug.com/368690
  "wikia": Wikia("wikia"),
  "wikipedia": Wikipedia("wikipedia", username_not_auto=True),
  "wordpress": Wordpress("wordpress"),
  "yahoo": Yahoo("yahoo", username_not_auto=True),
  "yandex": Yandex("yandex"),
  "ziddu": Ziddu("ziddu"), # Password not saved.
}


def SaveResults(environment_tests_results, environment_save_path,
                save_only_failures):
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
    for (name, test_type, success, failure_log) in environment_tests_results:
      if not (save_only_failures and success):
        xml += (
            "<test name='{0}' successful='{1}' type='{2}'>{3}</test>".format(
                name, success, test_type, failure_log))
    xml += "</result>"
    with open(environment_save_path, "w") as save_file:
      save_file.write(xml)


def RunTest(chrome_path, chromedriver_path, profile_path,
            environment_passwords_path, website_test_name,
            test_case_name):
  """Runs the test for the specified website.

  Args:
    chrome_path: The chrome binary file.
    chromedriver_path: The chromedriver binary file.
    profile_path: The chrome testing profile folder.
    environment_passwords_path: The usernames and passwords file.
    website_test_name: Name of the website to test (refer to keys in
        all_tests above).

  Returns:
    The results of the test as list of TestResults.

  Raises:
    Exception: An exception is raised if one of the tests for the website
        fails, or if the website name is not known.
  """

  enable_automatic_password_saving = (test_case_name == "SaveAndAutofillTest")
  environment = Environment(chrome_path, chromedriver_path, profile_path,
                            environment_passwords_path,
                            enable_automatic_password_saving)
  try:
    if website_test_name in all_tests:
      environment.AddWebsiteTest(all_tests[website_test_name])
    else:
      raise Exception("Test name {} is unknown.".format(website_test_name))

    environment.RunTestsOnSites(test_case_name)
    return environment.tests_results
  finally:
    environment.Quit()

def main():
  parser = argparse.ArgumentParser(
      description="Password Manager automated tests help.")

  parser.add_argument(
      "--chrome-path", action="store", dest="chrome_path",
      help="Set the chrome path (required).", required=True)
  parser.add_argument(
      "--chromedriver-path", action="store", dest="chromedriver_path",
      help="Set the chromedriver path (required).", required=True)
  parser.add_argument(
      "--profile-path", action="store", dest="profile_path",
      help="Set the profile path (required). You just need to choose a "
           "temporary empty folder. If the folder is not empty all its content "
           "is going to be removed.",
      required=True)

  parser.add_argument(
      "--passwords-path", action="store", dest="passwords_path",
      help="Set the usernames/passwords path (required).", required=True)
  parser.add_argument("--save-path", action="store", dest="save_path",
                      help="Write the results in a file.")
  parser.add_argument("--save-only-failures",
                      help="Only save logs for failing tests.",
                      dest="save_only_failures", action="store_true",
                      default=False)
  parser.add_argument("website", help="Website test name on which"
                      "tests should be run.")
  parser.add_argument("--test-cases-to-run", help="Names of test cases which"
                      "should be run. Currently supported test cases are:"
                      "PromptFailTest, PromptSuccessTest, SaveAndAutofillTest",
                      dest="test_cases_to_run", action="store", nargs="*")
  args = parser.parse_args()

  save_path = None
  if args.save_path:
    save_path = args.save_path

  test_cases_to_run = args.test_cases_to_run or TEST_CASES
  for test_case in test_cases_to_run:
    tests_results = RunTest(
        args.chrome_path, args.chromedriver_path, args.profile_path,
        args.passwords_path, args.website, test_case)


  SaveResults(tests_results, save_path,
              save_only_failures=args.save_only_failures)

if __name__ == "__main__":
  main()
