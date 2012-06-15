#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import glob
import logging
import os

import pyauto_functional  # Must be imported before pyauto
import pyauto


class ThemesTest(pyauto.PyUITest):
  """TestCase for Themes."""

  def Debug(self):
    """Test method for experimentation.

    This method will not run automatically.
    """
    while True:
      raw_input('Hit <enter> to dump info.. ')
      self.pprint(self.GetThemeInfo())

  def _SetThemeAndVerify(self, crx_file, theme_name):
    """Set theme and verify infobar appears and the theme name is correct.

    Args:
      crx_file: Path to .crx file to be set as theme.
      theme_name: String to be compared to GetThemeInfo()['name'].
    """
    # Starting infobar count is the number of non-themes infobars.
    infobars = self.GetBrowserInfo()['windows'][0]['tabs'][0]['infobars']
    infobar_count = 0
    for infobar in infobars:
      if not (('text' in infobar) and
              infobar['text'].startswith('Installed theme')):
        infobar_count += 1
    self.SetTheme(crx_file)
    # Verify infobar shows up.
    self.assertTrue(self.WaitForInfobarCount(infobar_count + 1))
    self.assertTrue(self.GetBrowserInfo()['windows'][0]['tabs'][0]['infobars'])
    # Verify theme name is correct.
    self.assertEqual(theme_name, self.GetThemeInfo()['name'])

  def testSetTheme(self):
    """Verify theme install."""
    self.assertFalse(self.GetThemeInfo())  # Verify there's no theme at startup
    crx_file = os.path.abspath(
        os.path.join(self.DataDir(), 'extensions', 'theme.crx'))
    self._SetThemeAndVerify(crx_file, 'camo theme')

  def testThemeInFullScreen(self):
    """Verify theme can be installed in FullScreen mode."""
    self.ApplyAccelerator(pyauto.IDC_FULLSCREEN)
    self.assertFalse(self.GetThemeInfo())  # Verify there's no theme at startup
    crx_file = os.path.abspath(
        os.path.join(self.DataDir(), 'extensions', 'theme.crx'))
    self._SetThemeAndVerify(crx_file, 'camo theme')

  def testThemeReset(self):
    """Verify theme reset."""
    crx_file = os.path.abspath(
        os.path.join(self.DataDir(), 'extensions', 'theme.crx'))
    self.SetTheme(crx_file)
    self.assertTrue(self.ResetToDefaultTheme())
    self.assertFalse(self.GetThemeInfo())

  def testThemeUndo(self):
    """Verify theme undo."""
    crx_file = os.path.abspath(
        os.path.join(self.DataDir(), 'extensions', 'theme.crx'))
    self._SetThemeAndVerify(crx_file, 'camo theme')
    # Undo theme install.
    infobars = self.GetBrowserInfo()['windows'][0]['tabs'][0]['infobars']
    for index, infobar in enumerate(infobars):
      if (('text' in infobar) and
          infobar['text'].startswith('Installed theme')):
        theme_index = index
        break
    self.PerformActionOnInfobar('cancel', infobar_index=theme_index)
    self.assertFalse(self.GetThemeInfo())

  def testThemeOverInstall(self):
    """Verify that can install a theme over an existing theme."""
    crx_file = os.path.abspath(
        os.path.join(self.DataDir(), 'extensions', 'theme.crx'))
    self._SetThemeAndVerify(crx_file, 'camo theme')
    # Install a different theme.
    crx_file = os.path.abspath(
        os.path.join(self.DataDir(), 'extensions', 'theme2.crx'))
    self._SetThemeAndVerify(crx_file, 'snowflake theme')

  def _ReturnCrashingThemes(self, themes, group_size, urls):
    """Install the given themes in groups of group_size and return the
       group of themes that crashes (if any).

    Note: restarts the browser at the beginning of the function.

    Args:
      themes: A list of themes to install.
      group_size: The number of themes to install at one time.
      urls: The list of urls to visit.

    Returns:
      Group of themes that crashed (if any).
    """
    self.RestartBrowser()
    curr_theme = 0
    num_themes = len(themes)

    while curr_theme < num_themes:
      logging.debug('New group of %d themes.' % group_size)
      group_end = curr_theme + group_size
      this_group = themes[curr_theme:group_end]

      # Apply each theme in this group.
      for theme in this_group:
        logging.debug('Applying theme: %s' % theme)
        self.SetTheme(theme)

      for url in urls:
        self.NavigateToURL(url)

      def _LogAndReturnCrashing():
        logging.debug('Crashing themes: %s' % this_group)
        return this_group

      # Assert that there is at least 1 browser window.
      try:
        num_browser_windows = self.GetBrowserWindowCount()
      except:
        return _LogAndReturnCrashing()
      else:
        if not num_browser_windows:
          return _LogAndReturnCrashing()

      curr_theme = group_end

    # None of the themes crashed.
    return None

  def Runner(self):
    """Apply themes; verify that theme has been applied and browser doesn't
       crash.

    This does not get run automatically. To run:
    python themes.py themes.ThemesTest.Runner

    Note: this test requires that a directory of crx files called 'themes'
    exists in the data directory.
    """
    themes_dir = os.path.join(self.DataDir(), 'themes')
    urls_file = os.path.join(self.DataDir(), 'urls.txt')

    assert os.path.exists(themes_dir), \
           'The dir "%s" must exist' % os.path.abspath(themes_dir)

    group_size = 20
    num_urls_to_visit = 100

    urls = [l.rstrip() for l in
            open(urls_file).readlines()[:num_urls_to_visit]]
    failed_themes = glob.glob(os.path.join(themes_dir, '*.crx'))

    while failed_themes and group_size:
      failed_themes = self._ReturnCrashingThemes(failed_themes, group_size,
                                                 urls)
      group_size = group_size // 2

    self.assertFalse(failed_themes,
                     'Theme(s) in failing group: %s' % failed_themes)


if __name__ == '__main__':
  pyauto_functional.Main()
