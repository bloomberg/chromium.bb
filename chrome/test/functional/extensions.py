#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
This module is a simple qa tool that installs extensions and tests whether the
browser crashes while visiting a list of urls.

Usage: python extensions.py -v

Note: This assumes that there is a directory of extensions called
'extensions-tool' and that there is a file of newline-separated urls to visit
called 'urls.txt' in the data directory.
"""

import glob
import logging
import os
import sys

import pyauto_functional # must be imported before pyauto
import pyauto


class ExtensionsPage(object):
  """Access options in extensions page (chrome://extensions-frame)."""

  _URL = 'chrome://extensions-frame'

  def __init__(self, driver):
    self._driver = driver
    self._driver.get(ExtensionsPage._URL)

  def CheckExtensionVisible(self, ext_id):
    """Returns True if |ext_id| exists on page."""
    return len(self._driver.find_elements_by_id(ext_id)) == 1

  def SetEnabled(self, ext_id, enabled):
    """Clicks on 'Enabled' checkbox for specified extension.

    Args:
      ext_id: Extension ID to be enabled or disabled.
      enabled: Boolean indicating whether |ext_id| is to be enabled or disabled.
    """
    checkbox = self._driver.find_element_by_xpath(
        '//*[@id="%s"]//*[@class="enable-controls"]//*[@type="checkbox"]' %
        ext_id)
    if checkbox != enabled:
      checkbox.click()
    # Reload page to ensure that the UI is recreated.
    self._driver.get(ExtensionsPage._URL)

  def SetAllowInIncognito(self, ext_id, allowed):
    """Clicks on 'Allow in incognito' checkbox for specified extension.

    Args:
      ext_id: Extension ID to be enabled or disabled.
      allowed: Boolean indicating whether |ext_id| is to be allowed or
          disallowed in incognito.
    """
    checkbox = self._driver.find_element_by_xpath(
        '//*[@id="%s"]//*[@class="incognito-control"]//*[@type="checkbox"]' %
        ext_id)
    if checkbox.is_selected() != allowed:
      checkbox.click()
    # Reload page to ensure that the UI is recreated.
    self._driver.get(ExtensionsPage._URL)

  def SetAllowAccessFileURLs(self, ext_id, allowed):
    """Clicks on 'Allow access to file URLs' checkbox for specified extension.

    Args:
      ext_id: Extension ID to be enabled or disabled.
      allowed: Boolean indicating whether |ext_id| is to be allowed access to
          file URLs.
    """
    checkbox = self._driver.find_element_by_xpath(
        '//*[@id="%s"]//*[@class="file-access-control"]//*[@type="checkbox"]' %
        ext_id)
    if checkbox.is_selected() != allowed:
      checkbox.click()


class ExtensionsTest(pyauto.PyUITest):
  """Test of extensions."""

  def Debug(self):
    """Test method for experimentation.

    This method is not run automatically.
    """
    while True:
      raw_input('Interact with the browser and hit <enter> to dump history.')
      print '*' * 20
      self.pprint(self.GetExtensionsInfo())

  def _GetInstalledExtensionIds(self):
    return [extension['id'] for extension in self.GetExtensionsInfo()]

  def _ReturnCrashingExtensions(self, extensions, group_size, top_urls):
    """Returns the group of extensions that crashes (if any).

    Install the given extensions in groups of group_size and return the
    group of extensions that crashes (if any).

    Args:
      extensions: A list of extensions to install.
      group_size: The number of extensions to install at one time.
      top_urls: The list of top urls to visit.

    Returns:
      The extensions in the crashing group or None if there is no crash.
    """
    curr_extension = 0
    num_extensions = len(extensions)
    self.RestartBrowser()
    orig_extension_ids = self._GetInstalledExtensionIds()

    while curr_extension < num_extensions:
      logging.debug('New group of %d extensions.', group_size)
      group_end = curr_extension + group_size
      for extension in extensions[curr_extension:group_end]:
        logging.debug('Installing extension: %s', extension)
        self.InstallExtension(extension)

      for url in top_urls:
        self.NavigateToURL(url)

      def _LogAndReturnCrashing():
        crashing_extensions = extensions[curr_extension:group_end]
        logging.debug('Crashing extensions: %s', crashing_extensions)
        return crashing_extensions

      # If the browser has crashed, return the extensions in the failing group.
      try:
        num_browser_windows = self.GetBrowserWindowCount()
      except:
        return _LogAndReturnCrashing()
      else:
        if not num_browser_windows:
          return _LogAndReturnCrashing()
        else:
          # Uninstall all extensions that aren't installed by default.
          new_extension_ids = [id for id in self._GetInstalledExtensionIds()
                               if id not in orig_extension_ids]
          for extension_id in new_extension_ids:
            self.UninstallExtensionById(extension_id)

      curr_extension = group_end

    # None of the extensions crashed.
    return None

  def _GetExtensionInfoById(self, extensions, id):
    for x in extensions:
      if x['id'] == id:
        return x
    return None

  def ExtensionCrashes(self):
    """Add top extensions; confirm browser stays up when visiting top urls."""
    # TODO: provide a way in pyauto to pass args to a test - take these as args
    extensions_dir = os.path.join(self.DataDir(), 'extensions-tool')
    urls_file = os.path.join(self.DataDir(), 'urls.txt')

    error_msg = 'The dir "%s" must exist' % os.path.abspath(extensions_dir)
    assert os.path.exists(extensions_dir), error_msg
    error_msg = 'The file "%s" must exist' % os.path.abspath(urls_file)
    assert os.path.exists(urls_file), error_msg

    num_urls_to_visit = 100
    extensions_group_size = 20

    top_urls = [l.rstrip() for l in
                open(urls_file).readlines()[:num_urls_to_visit]]

    failed_extensions = glob.glob(os.path.join(extensions_dir, '*.crx'))
    group_size = extensions_group_size

    while (group_size and failed_extensions):
      failed_extensions = self._ReturnCrashingExtensions(
          failed_extensions, group_size, top_urls)
      group_size = group_size // 2

    self.assertFalse(failed_extensions,
                     'Extension(s) in failing group: %s' % failed_extensions)

  def _InstallExtensionCheckDefaults(self, crx_file):
    """Installs extension at extensions/|crx_file| and checks default status.

    Checks that the installed extension is enabled and not allowed in incognito.

    Args:
      crx_file: Relative path from self.DataDir()/extensions to .crx extension
                to be installed.

    Returns:
      The extension ID.
    """
    crx_file_path = os.path.abspath(
        os.path.join(self.DataDir(), 'extensions', crx_file))
    ext_id = self.InstallExtension(crx_file_path)
    extension = self._GetExtensionInfoById(self.GetExtensionsInfo(), ext_id)
    self.assertTrue(extension['is_enabled'],
                    msg='Extension was not enabled on installation')
    self.assertFalse(extension['allowed_in_incognito'],
                     msg='Extension was allowed in incognito on installation.')

    return ext_id

  def _ExtensionValue(self, ext_id, key):
    """Returns the value of |key| for |ext_id|.

    Args:
      ext_id: The extension ID.
      key: The key for which the extensions info value is required.

    Returns:
      The value of extensions info |key| for |ext_id|.
    """
    return self._GetExtensionInfoById(self.GetExtensionsInfo(), ext_id)[key]

  def _FileAccess(self, ext_id):
    """Returns the value of newAllowFileAccess for |ext_id|.

    Args:
      ext_id: The extension ID.

    Returns:
      The value of extensions settings newAllowFileAccess for |ext_id|.
    """
    extension_settings = self.GetPrefsInfo().Prefs()['extensions']['settings']
    return extension_settings[ext_id]['newAllowFileAccess']

  def testGetExtensionPermissions(self):
    """Ensures we can retrieve the host/api permissions for an extension.

    This test assumes that the 'Bookmark Manager' extension exists in a fresh
    profile.
    """
    extensions_info = self.GetExtensionsInfo()
    bm_exts = [x for x in extensions_info if x['name'] == 'Bookmark Manager']
    self.assertTrue(bm_exts,
                    msg='Could not find info for the Bookmark Manager '
                    'extension.')
    ext = bm_exts[0]

    permissions_host = ext['host_permissions']
    self.assertTrue(len(permissions_host) == 2 and
                    'chrome://favicon/*' in permissions_host and
                    'chrome://resources/*' in permissions_host,
                    msg='Unexpected host permissions information.')

    permissions_api = ext['api_permissions']
    print permissions_api
    self.assertTrue(len(permissions_api) == 5 and
                    'bookmarks' in permissions_api and
                    'bookmarkManagerPrivate' in permissions_api and
                    'metricsPrivate' in permissions_api and
                    'systemPrivate' in permissions_api and
                    'tabs' in permissions_api,
                    msg='Unexpected API permissions information.')

  def testDisableEnableExtension(self):
    """Tests that an extension can be disabled and enabled with the UI."""
    ext_id = self._InstallExtensionCheckDefaults('good.crx')

    # Disable extension.
    driver = self.NewWebDriver()
    ext_page = ExtensionsPage(driver)
    self.WaitUntil(ext_page.CheckExtensionVisible, args=[ext_id])
    ext_page.SetEnabled(ext_id, False)
    self.WaitUntil(self._ExtensionValue, args=[ext_id, 'is_enabled'],
                   expect_retval=False)
    self.assertFalse(self._ExtensionValue(ext_id, 'is_enabled'),
                     msg='Extension did not get disabled.')

    # Enable extension.
    self.WaitUntil(ext_page.CheckExtensionVisible, args=[ext_id])
    ext_page.SetEnabled(ext_id, True)
    self.WaitUntil(self._ExtensionValue, args=[ext_id, 'is_enabled'],
                   expect_retval=True)
    self.assertTrue(self._ExtensionValue(ext_id, 'is_enabled'),
                    msg='Extension did not get enabled.')

  def testAllowIncognitoExtension(self):
    """Tests allowing and disallowing an extension in incognito mode."""
    ext_id = self._InstallExtensionCheckDefaults('good.crx')

    # Allow in incognito.
    driver = self.NewWebDriver()
    ext_page = ExtensionsPage(driver)
    self.WaitUntil(ext_page.CheckExtensionVisible, args=[ext_id])
    ext_page.SetAllowInIncognito(ext_id, True)

    # Check extension now allowed in incognito.
    self.WaitUntil(self._ExtensionValue, args=[ext_id, 'allowed_in_incognito'],
                   expect_retval=True)
    self.assertTrue(self._ExtensionValue(ext_id, 'allowed_in_incognito'),
                    msg='Extension did not get allowed in incognito.')

    # Disallow in incognito.
    self.WaitUntil(ext_page.CheckExtensionVisible, args=[ext_id])
    ext_page.SetAllowInIncognito(ext_id, False)

    # Check extension now disallowed in incognito.
    self.WaitUntil(self._ExtensionValue, args=[ext_id, 'allowed_in_incognito'],
                   expect_retval=False)
    self.assertFalse(self._ExtensionValue(ext_id, 'allowed_in_incognito'),
                     msg='Extension did not get disallowed in incognito.')

  def testAllowAccessFileURLs(self):
    """Tests disallowing and allowing and extension access to file URLs."""
    ext_id = self._InstallExtensionCheckDefaults(os.path.join('permissions',
                                                              'files'))

    # Check extension allowed access to file URLs by default.
    extension_settings = self.GetPrefsInfo().Prefs()['extensions']['settings']
    self.assertTrue(extension_settings[ext_id]['newAllowFileAccess'],
                    msg='Extension was not allowed access to file URLs on '
                    'installation')

    # Disallow access to file URLs.
    driver = self.NewWebDriver()
    ext_page = ExtensionsPage(driver)
    self.WaitUntil(ext_page.CheckExtensionVisible, args=[ext_id])
    ext_page.SetAllowAccessFileURLs(ext_id, False)

    # Check that extension does not have access to file URLs.
    self.WaitUntil(self._FileAccess, args=[ext_id], expect_retval=False)
    self.assertFalse(self._FileAccess(ext_id),
                     msg='Extension did not have access to file URLs denied.')

    # Allow access to file URLs.
    self.WaitUntil(ext_page.CheckExtensionVisible, args=[ext_id])
    ext_page.SetAllowAccessFileURLs(ext_id, True)

    # Check that extension now has access to file URLs.
    self.WaitUntil(self._FileAccess, args=[ext_id], expect_retval=True)
    self.assertTrue(self._FileAccess(ext_id),
                    msg='Extension did not have access to file URLs granted.')


if __name__ == '__main__':
  pyauto_functional.Main()
