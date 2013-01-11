# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Update tests for themes."""
import os

from common import util

import chrome_options
import install_test


class ThemeUpdater(install_test.InstallTest):
  """Theme update tests."""
  _DIRECTORY = os.path.dirname(os.path.abspath(__file__))
  _EXTENSIONS_DIR = os.path.join(_DIRECTORY, os.path.pardir, 'data',
                                 'extensions')
  camo_theme = os.path.join(_EXTENSIONS_DIR, 'theme.crx')
  camo_img = ('chrome://theme/IDR_THEME_NTP_BACKGROUND?'
              'iamefpfkojoapidjnbafmgkgncegbkad')

  def setUp(self):
    super(ThemeUpdater, self).setUp()
    self._user_data_dir = util.MakeTempDir()

  def _CheckThemeApplied(self):
    """Loads the New Tab Page and asserts that the theme is applied."""
    self._driver.get('chrome://newtab')
    html = self._driver.find_element_by_xpath('html')
    html_background = html.value_of_css_property('background-image')
    self.assertTrue(self.camo_img in html_background,
                    msg='Did not find expected theme background-image')

  def _StartChromeProfile(self, incognito=False):
    """Start Chrome with a temp profile.

    Args:
      incognito: Boolean flag for starting Chrome in incognito.
    """
    options = chrome_options.ChromeOptions()
    options.SetUserDataDir(self._user_data_dir)
    if incognito:
      options.AddSwitch('incognito')
    self.StartChrome(options.GetCapabilities())

  def _StartChromeProfileExtension(self, extension):
    """Start Chrome with a temp profile and with specified extension.

    Args:
      extension: Paths to extension to be installed.
    """
    options = chrome_options.ChromeOptions()
    options.AddExtension(extension)
    options.SetUserDataDir(self._user_data_dir)
    self.StartChrome(options.GetCapabilities())

  def testInstallTheme(self):
    """Install a theme and check it is still applied after update."""
    self.Install(self.GetUpdateBuilds()[0])
    self._StartChromeProfileExtension(self.camo_theme)
    self._CheckThemeApplied()

    # Update and relaunch without extension.
    self.Install(self.GetUpdateBuilds()[1])
    self._StartChromeProfile()
    self._CheckThemeApplied()

  def testInstallThemeIncognito(self):
    """Install a theme and check it still applies to incognito after update."""
    self.Install(self.GetUpdateBuilds()[0])
    self._StartChromeProfileExtension(self.camo_theme)
    self._CheckThemeApplied()

    # Relaunch without extension in incognito.
    self._driver.quit()
    self._StartChromeProfile(incognito=True)
    self._CheckThemeApplied()

    # Update and relaunch without extension in incognito.
    self.Install(self.GetUpdateBuilds()[1])
    self._StartChromeProfile(incognito=True)
    self._CheckThemeApplied()
