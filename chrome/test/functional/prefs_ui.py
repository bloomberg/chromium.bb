#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import time

import pyauto_functional  # Must be imported before pyauto
import pyauto
import test_utils

from webdriver_pages import settings
from webdriver_pages.settings import Behaviors, ContentTypes
from webdriver_pages.settings import RestoreOnStartupType


class PrefsUITest(pyauto.PyUITest):
  """TestCase for Preferences UI."""

  INFOBAR_TYPE = 'rph_infobar'

  def setUp(self):
    pyauto.PyUITest.setUp(self)
    self._driver = self.NewWebDriver()

  def Debug(self):
    """Test method for experimentation.

    This method will not run automatically.
    """
    driver = self.NewWebDriver()
    page = settings.ContentSettingsPage.FromNavigation(driver)
    import pdb
    pdb.set_trace()

  def _GetGeolocationContentSettingsBehavior(self):
    """Get the Content Settings behavior for Geolocation.

    Returns:
      The exceptions behavior for the specified content type.
      The content type is the available content setting available.
    """
    behavior_key = self.GetPrefsInfo().Prefs(
        pyauto.kGeolocationDefaultContentSetting)
    behaviors_dict = {1: 'ALLOW', 2: 'BLOCK', 3: 'ASK'}
    self.assertTrue(
        behavior_key in behaviors_dict,
        msg=('Invalid default behavior key "%s" for "geolocation" content' %
             behavior_key))
    return behaviors_dict[behavior_key]

  def _VerifyContentExceptionUI(self, content_type, hostname_pattern, behavior,
                                incognito=False):
    """Find hostname pattern and behavior within UI on content exceptions page.

    Args:
      content_type: The string content settings type to manage.
      hostname_pattern: The URL or pattern associated with the behavior.
      behavior: The exception to allow or block the hostname.
      incognito: Incognito list displayed on exceptions settings page.
                 Default to False.
    """
    page = settings.ManageExceptionsPage.FromNavigation(
        self._driver, content_type)
    self.assertTrue(page.GetExceptions(incognito).has_key(hostname_pattern),
                     msg=('No displayed host name matches pattern "%s"'
                          % hostname_pattern))
    self.assertEqual(behavior, page.GetExceptions(incognito)[hostname_pattern],
                     msg=('Displayed behavior "%s" does not match behavior "%s"'
                          % (page.GetExceptions(incognito)[hostname_pattern],
                             behavior)))

  def testLocationSettingOptionsUI(self):
    """Verify the location options setting UI.

    Set the options through the UI using webdriver and verify the settings in
    pyAuto.
    """
    page = settings.ContentSettingsPage.FromNavigation(self._driver)
    page.SetContentTypeOption(ContentTypes.GEOLOCATION, Behaviors.ALLOW)
    self.assertEqual(
        1, self.GetPrefsInfo().Prefs(pyauto.kGeolocationDefaultContentSetting))
    page.SetContentTypeOption(ContentTypes.GEOLOCATION, Behaviors.BLOCK)
    self.assertEqual(
        2, self.GetPrefsInfo().Prefs(pyauto.kGeolocationDefaultContentSetting))
    page.SetContentTypeOption(ContentTypes.GEOLOCATION, Behaviors.ASK)
    self.assertEqual(
        3, self.GetPrefsInfo().Prefs(pyauto.kGeolocationDefaultContentSetting))

  def testBehaviorValueCorrectlyDisplayed(self):
    """Verify the set behavior value is correctly displayed."""
    # Block all sites.
    self.SetPrefs(pyauto.kGeolocationDefaultContentSetting, 2)
    self.assertEqual(
        self._GetGeolocationContentSettingsBehavior(), Behaviors.BLOCK.upper(),
        msg='The behavior was incorrectly set.')
    # Allow all sites.
    self.SetPrefs(pyauto.kGeolocationDefaultContentSetting, 1)
    self.assertEqual(
        self._GetGeolocationContentSettingsBehavior(), Behaviors.ALLOW.upper(),
        msg='The behavior was incorrectly set.')
    # Ask for permission when site wants to track.
    self.SetPrefs(pyauto.kGeolocationDefaultContentSetting, 3)
    self.assertEqual(
        self._GetGeolocationContentSettingsBehavior(), Behaviors.ASK.upper(),
        msg='The behavior was incorrectly set.')

  def testExceptionsEntryCorrectlyDisplayed(self):
    """Verify the exceptions line entry is correctly displayed in the UI."""
    geo_exception = (
        {'http://maps.google.com:80,http://maps.google.com:80':
            {'geolocation': 2}})
    self.SetPrefs(pyauto.kContentSettingsPatternPairs, geo_exception)
    self._VerifyContentExceptionUI(
        ContentTypes.GEOLOCATION, 'http://maps.google.com:80',
        Behaviors.BLOCK)
    geo_exception = (
        {'http://maps.google.com:80,http://maps.google.com:80':
            {'geolocation': 1}})
    self.SetPrefs(pyauto.kContentSettingsPatternPairs, geo_exception)
    self._VerifyContentExceptionUI(
        ContentTypes.GEOLOCATION, 'http://maps.google.com:80',
        Behaviors.ALLOW)
    geo_exception = (
        {'http://maps.google.com:80,http://maps.google.com:80':
            {'geolocation': 3}})
    self.SetPrefs(pyauto.kContentSettingsPatternPairs, geo_exception)
    self._VerifyContentExceptionUI(
        ContentTypes.GEOLOCATION, 'http://maps.google.com:80', Behaviors.ASK)

  def testAddNewExceptionUI(self):
    """Verify new exception added for hostname pattern and behavior in UI."""
    content_type = ContentTypes.PLUGINS
    page = settings.ManageExceptionsPage.FromNavigation(
        self._driver, content_type)

    pattern, behavior = ('bing.com', Behaviors.BLOCK)
    page.AddNewException(pattern, behavior)
    self.assertEqual(page.GetExceptions()[pattern], Behaviors.BLOCK,
                     msg='The behavior "%s" was not added for pattern "%s"'
                     % (behavior, pattern))

  def testChangeExceptionBehaviorUI(self):
    """Verify behavior for hostname pattern is changed in the UI."""
    content_type = ContentTypes.PLUGINS
    page = settings.ManageExceptionsPage.FromNavigation(
        self._driver, content_type)

    pattern, behavior = ('bing.com', Behaviors.BLOCK)
    page.AddNewException(pattern, behavior)
    new_behavior = Behaviors.ALLOW
    page.SetBehaviorForPattern(pattern, new_behavior)
    self.assertEqual(page.GetExceptions()[pattern], Behaviors.ALLOW,
                     msg='The behavior for "%s" did not change: "%s"'
                     % (pattern, behavior))

  def testDeleteExceptionUI(self):
    """Verify exception deleted for hostname pattern and behavior in the UI."""
    content_type = ContentTypes.PLUGINS
    page = settings.ManageExceptionsPage.FromNavigation(
        self._driver, content_type)

    pattern, behavior = ('bing.com', Behaviors.BLOCK)
    page.AddNewException(pattern, behavior)
    self.assertEqual(page.GetExceptions()[pattern], Behaviors.BLOCK,
                     msg='The behavior "%s" was not added for pattern "%s"'
                     % (behavior, pattern))
    page.DeleteException(pattern)
    self.assertEqual(page.GetExceptions().get(pattern, KeyError), KeyError,
                     msg='Pattern "%s" was not deleted' % pattern)

  def testNoInitialLineEntryInUI(self):
    """Verify no initial line entry is displayed in UI."""
    # Ask for permission when site wants to track.
    self.SetPrefs(pyauto.kGeolocationDefaultContentSetting, 3)
    self.assertEqual(
        3, self.GetPrefsInfo().Prefs(pyauto.kGeolocationDefaultContentSetting))
    page = settings.ManageExceptionsPage.FromNavigation(
        self._driver, ContentTypes.GEOLOCATION)
    self.assertEqual(0, len(page.GetExceptions()))

  def testCorrectCookiesSessionInUI(self):
    """Verify exceptions for cookies in UI list entry."""
    # Block cookies for for a session for google.com.
    self.SetPrefs(pyauto.kContentSettingsPatternPairs,
                  {'http://google.com:80': {'cookies': 2}})
    self._VerifyContentExceptionUI(
        ContentTypes.COOKIES, 'http://google.com:80', Behaviors.BLOCK)

  def testInitialLineEntryInIncognitoUI(self):
    """Verify initial line entry is displayed in Incognito UI."""
    self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)  # Display incognito list.
    page = settings.ManageExceptionsPage.FromNavigation(
        self._driver, ContentTypes.PLUGINS)
    self.assertEqual(1, len(page.GetExceptions(incognito=True)))

  def testIncognitoExceptionsEntryCorrectlyDisplayed(self):
    """Verify exceptions entry is correctly displayed in the incognito UI."""
    self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)  # Display incognito list.
    page = settings.ManageExceptionsPage.FromNavigation(
        self._driver, ContentTypes.PLUGINS)
    pattern, behavior = ('http://maps.google.com:80', Behaviors.BLOCK)
    page.AddNewException(pattern, behavior, incognito=True)
    self._VerifyContentExceptionUI(
        ContentTypes.PLUGINS, 'http://maps.google.com:80',
        Behaviors.BLOCK, incognito=True)

  def testSetPasswordAndDelete(self):
    """Verify a password can be deleted in the Content Settings UI."""
    password_list = []
    # Add two passwords, 1 for example0.com and 1 for example1.com.
    for i in range(2):
      # Construct a password dictionary with required fields.
      password = {
          'username_value': 'user%s@example.com'  % i,
          'password_value': 'test.password',
          'signon_realm': 'https://www.example%s.com/'  % i,
          'time': 1279650942.0,
          'origin_url': 'https://www.example%s.com/login'  % i,
          'username_element': 'username%s',
          'password_element': 'password',
          'submit_element': 'submit',
          'action_target': 'https://www.example%s.com/login/'  % i,
          'blacklist': False,
      }

      password_list.append(password)
      self.AddSavedPassword(password)

    saved_passwords = self.GetSavedPasswords()
    self.assertEquals(len(password_list), len(saved_passwords),
                      msg='Not all Passwords were saved.')
    for password in password_list:
      self.assertTrue(password in saved_passwords,
                      msg='Passwords were not saved correctly.')

    page = settings.PasswordsSettings.FromNavigation(self._driver)
    # Delete one of the passwords.
    password = password_list[1]
    page.DeleteItem(password['origin_url'], password['username_value'])
    self.assertTrue(password not in self.GetSavedPasswords(),
                    msg='Password was not deleted.')

  def testSetCookieAndDeleteInContentSettings(self):
    """Verify a cookie can be deleted in the Content Settings UI."""
    # Create a cookie.
    cookie_dict = {
        'name': 'test_cookie',
        'value': 'test_value',
        'expiry': time.time() + 30,
    }
    site = '127.0.0.1'
    self.NavigateToURL(self.GetHttpURLForDataPath('google', 'google.html'))
    self._driver.add_cookie(cookie_dict)
    page = settings.CookiesAndSiteDataSettings.FromNavigation(self._driver)
    page.DeleteSiteData(site)
    self.assertTrue(site not in page.GetSiteNameList(),
                    'Site "%s" was not deleted.'  % site)

  def testRemoveMailProtocolHandler(self):
    """Verify the mail protocol handler is added and removed successfully."""
    url = self.GetHttpURLForDataPath('settings', 'protocol_handler.html')
    self.NavigateToURL(url)
    # Returns a dictionary with the mail handler that was asked for
    # registration.
    asked_handler_dict = self._driver.execute_script(
        'return registerMailClient()')
    self.PerformActionOnInfobar(
        'accept', infobar_index=test_utils.WaitForInfobarTypeAndGetIndex(
            self, self.INFOBAR_TYPE))
    self._driver.find_element_by_id('test_mail_protocol').click()

    protocol_handlers_list = (
        self.GetPrefsInfo().Prefs(pyauto.kRegisteredProtocolHandlers))
    registered_mail_handler = {}
    for handler_dict in protocol_handlers_list:
      if (handler_dict['protocol'] == 'mailto' and
          handler_dict['url'] == asked_handler_dict['url'] and
          handler_dict['title'] == asked_handler_dict['title'] and
          handler_dict.get('default')):
        registered_mail_handler = handler_dict
        break
      # Verify the mail handler is registered as asked.
      self.assertNotEqual(
      registered_mail_handler, {},
      msg='Mail protocol handler was not registered correctly.')
      # Verify the registered mail handler works as expected.
      self.assertTrue(
          self._driver.execute_script(
              'return doesQueryConformsToProtocol("%s", "%s")'
              % (asked_handler_dict['query_key'],
                 asked_handler_dict['query_value'])),
              msg='Mail protocol did not register correctly.')

    self._driver.get('chrome://settings-frame/handlers')
    # There are 3 DIVs in a handler entry. The last one acts as a remove button.
    # The remove button is also equivalent to setting the site to NONE.
    self._driver.find_element_by_id('handlers-list').\
        find_element_by_xpath('.//div[@role="listitem"]').\
        find_element_by_xpath('.//div[@class="handlers-site-column"]').\
        find_element_by_xpath('.//option[@value="-1"]').click()

    self._driver.get(url)
    self._driver.find_element_by_id('test_mail_protocol').click()
    self.assertEqual(url, self._driver.current_url,
                     msg='Mail protocol still registered.')

class BasicSettingsUITest(pyauto.PyUITest):
  """Testcases for uber page basic settings UI."""

  def setUp(self):
    pyauto.PyUITest.setUp(self)
    self._driver = self.NewWebDriver()

  def Debug(self):
    """chrome://plugins test debug method.

    This method will not run automatically.
    """
    driver = self.NewWebDriver()
    page = settings.BasicSettingsPage.FromNavigation(driver)
    import pdb
    pdb.set_trace()

  def testOnStartupSettings(self):
    """Verify user can set startup options."""
    page = settings.BasicSettingsPage.FromNavigation(self._driver)
    page.SetOnStartupOptions(RestoreOnStartupType.NEW_TAB_PAGE)
    self.assertEqual(RestoreOnStartupType.NEW_TAB_PAGE,
        self.GetPrefsInfo().Prefs(pyauto.kRestoreOnStartup))
    page.SetOnStartupOptions(RestoreOnStartupType.RESTORE_SESSION)
    self.assertEqual(RestoreOnStartupType.RESTORE_SESSION,
        self.GetPrefsInfo().Prefs(pyauto.kRestoreOnStartup))
    page.SetOnStartupOptions(RestoreOnStartupType.RESTORE_URLS)
    self.assertEqual(RestoreOnStartupType.RESTORE_URLS,
        self.GetPrefsInfo().Prefs(pyauto.kRestoreOnStartup))

  def testSetStartupPages(self):
    """Verify user can add urls for startup pages."""
    page = settings.BasicSettingsPage.FromNavigation(self._driver)
    for url in ['www.google.com', 'http://www.amazon.com', 'ebay.com']:
      page.AddStartupPage(url)
    self.assertEqual(RestoreOnStartupType.RESTORE_URLS,
        self.GetPrefsInfo().Prefs(pyauto.kRestoreOnStartup))
    startup_urls = self.GetPrefsInfo().Prefs(pyauto.kURLsToRestoreOnStartup)
    self.assertEqual(startup_urls[0], 'http://www.google.com/')
    self.assertEqual(startup_urls[1], 'http://www.amazon.com/')
    self.assertEqual(startup_urls[2], 'http://ebay.com/')

  def testUseCurrentPagesForStartup(self):
    """Verify user can start up browser using current pages."""
    page = settings.BasicSettingsPage.FromNavigation(self._driver)
    self.OpenNewBrowserWindow(True)
    url1 = self.GetHttpURLForDataPath('title2.html')
    url2 = self.GetHttpURLForDataPath('title3.html')
    self.NavigateToURL(url1, 1, 0)
    self.AppendTab(pyauto.GURL(url2), 1)
    title_list = ['Title Of Awesomeness',
                  'Title Of More Awesomeness']
    page.UseCurrentPageForStartup(title_list)
    page.VerifyStartupURLs(title_list)
    self.assertEqual(RestoreOnStartupType.RESTORE_URLS,
        self.GetPrefsInfo().Prefs(pyauto.kRestoreOnStartup))
    startup_urls = self.GetPrefsInfo().Prefs(pyauto.kURLsToRestoreOnStartup)
    self.assertEqual(len(startup_urls), 3)
    self.assertEqual(startup_urls[1], url1)
    self.assertEqual(startup_urls[2], url2)

  def testCancelStartupURLSetting(self):
    """Verify canceled start up URLs settings are not saved."""
    page = settings.BasicSettingsPage.FromNavigation(self._driver)
    for url in ['www.google.com', 'http://www.amazon.com']:
      page.CancelStartupURLSetting(url)
    startup_urls = self.GetPrefsInfo().Prefs(pyauto.kURLsToRestoreOnStartup)
    self.assertEqual(len(startup_urls), 0)


if __name__ == '__main__':
  pyauto_functional.Main()
