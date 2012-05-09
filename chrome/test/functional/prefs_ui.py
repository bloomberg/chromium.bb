#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pyauto_functional  # Must be imported before pyauto
import pyauto
from webdriver_pages import settings
from webdriver_pages.settings import Behaviors, ContentTypes


class PrefsUITest(pyauto.PyUITest):
  """TestCase for Preferences UI."""

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
        {'http://maps.google.com:80,http://maps.google.com:80': {'geolocation': 2}})
    self.SetPrefs(pyauto.kContentSettingsPatternPairs, geo_exception)
    self._VerifyContentExceptionUI(
        ContentTypes.GEOLOCATION, 'http://maps.google.com:80',
        Behaviors.BLOCK)
    geo_exception = (
        {'http://maps.google.com:80,http://maps.google.com:80': {'geolocation': 1}})
    self.SetPrefs(pyauto.kContentSettingsPatternPairs, geo_exception)
    self._VerifyContentExceptionUI(
        ContentTypes.GEOLOCATION, 'http://maps.google.com:80',
        Behaviors.ALLOW)
    geo_exception = (
        {'http://maps.google.com:80,http://maps.google.com:80': {'geolocation': 3}})
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


if __name__ == '__main__':
  pyauto_functional.Main()
