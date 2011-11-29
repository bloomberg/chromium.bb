#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import os

import pyauto_functional  # Must be imported before pyauto
import pyauto
import test_utils


class NTPTest(pyauto.PyUITest):
  """Test of the NTP."""

  # Default apps are registered in ProfileImpl::RegisterComponentExtensions().
  _EXPECTED_DEFAULT_APPS = [
    {u'title': u'Chrome Web Store'},
  ]
  if pyauto.PyUITest.IsChromeOS():
    _EXPECTED_DEFAULT_APPS.append({u'title': u'File Manager'})
  else:
    _EXPECTED_DEFAULT_APPS.append({u'title': u'Cloud Print'})

  # Default menu and thumbnail mode preferences are set in
  # ShownSectionsHandler::RegisterUserPrefs.
  if pyauto.PyUITest.IsChromeOS():
    _EXPECTED_DEFAULT_THUMB_INFO = {
      u'apps': True,
      u'most_visited': False
    }
    _EXPECTED_DEFAULT_MENU_INFO = {
      u'apps': False,
      u'most_visited': True,
      u'recently_closed': True
    }
  else:
    _EXPECTED_DEFAULT_THUMB_INFO = {
      u'apps': False,
      u'most_visited': True
    }
    _EXPECTED_DEFAULT_MENU_INFO = {
      u'apps': False,
      u'most_visited': False,
      u'recently_closed': False
    }

  def Debug(self):
    """Test method for experimentation.

    This method is not run automatically.
    """
    while True:
      raw_input('Interact with the browser and hit <enter> to dump NTP info...')
      print '*' * 20
      self.pprint(self._GetNTPInfo())

  def __init__(self, methodName='runTest'):
    super(NTPTest, self).__init__(methodName)

    # Create some dummy file urls we can use in the tests.
    filenames = ['title1.html', 'title2.html']
    titles = [u'', u'Title Of Awesomeness']
    urls = map(lambda name: self.GetFileURLForDataPath(name), filenames)
    self.PAGES = map(lambda url, title: {'url': url, 'title': title},
                     urls, titles)

  def _NTPContainsThumbnail(self, check_thumbnail):
    """Returns whether the NTP's Most Visited section contains the given
    thumbnail."""
    for thumbnail in self.GetNTPThumbnails():
      if check_thumbnail['url'] == thumbnail['url']:
        return True
    return False

  def testFreshProfile(self):
    """Tests that the NTP with a fresh profile is correct"""
    thumbnails = self.GetNTPThumbnails()
    default_sites = self.GetNTPDefaultSites()
    self.assertEqual(len(default_sites), len(thumbnails))
    for thumbnail, default_site in zip(thumbnails, default_sites):
      self.assertEqual(thumbnail['url'], default_site)
    self.assertEqual(0, len(self.GetNTPRecentlyClosed()))

  def testRemoveDefaultThumbnails(self):
    """Tests that the default thumbnails can be removed"""
    self.RemoveNTPDefaultThumbnails()
    self.assertFalse(self.GetNTPThumbnails())
    self.RestoreAllNTPThumbnails()
    self.assertEqual(len(self.GetNTPDefaultSites()),
                     len(self.GetNTPThumbnails()))
    self.RemoveNTPDefaultThumbnails()
    self.assertFalse(self.GetNTPThumbnails())

  def testOneMostVisitedSite(self):
    """Tests that a site is added to the most visited sites"""
    self.RemoveNTPDefaultThumbnails()
    self.NavigateToURL(self.PAGES[1]['url'])
    thumbnail = self.GetNTPThumbnails()[0]
    self.assertEqual(self.PAGES[1]['url'], thumbnail['url'])
    self.assertEqual(self.PAGES[1]['title'], thumbnail['title'])
    self.assertFalse(thumbnail['is_pinned'])

  def testMoveThumbnailBasic(self):
    """Tests moving a thumbnail to a different index"""
    self.RemoveNTPDefaultThumbnails()
    self.NavigateToURL(self.PAGES[0]['url'])
    self.NavigateToURL(self.PAGES[1]['url'])
    thumbnails = self.GetNTPThumbnails()
    self.MoveNTPThumbnail(thumbnails[0], 1)
    self.assertTrue(self.IsNTPThumbnailPinned(thumbnails[0]))
    self.assertFalse(self.IsNTPThumbnailPinned(thumbnails[1]))
    self.assertEqual(self.PAGES[0]['url'], self.GetNTPThumbnails()[1]['url'])
    self.assertEqual(1, self.GetNTPThumbnailIndex(thumbnails[0]))

  def testPinningThumbnailBasic(self):
    """Tests that we can pin/unpin a thumbnail"""
    self.RemoveNTPDefaultThumbnails()
    self.NavigateToURL(self.PAGES[0]['url'])
    thumbnail1 = self.GetNTPThumbnails()[0]
    self.assertFalse(self.IsNTPThumbnailPinned(thumbnail1))
    self.PinNTPThumbnail(thumbnail1)
    self.assertTrue(self.IsNTPThumbnailPinned(thumbnail1))
    self.UnpinNTPThumbnail(thumbnail1)
    self.assertFalse(self.IsNTPThumbnailPinned(thumbnail1))

  def testRemoveThumbnail(self):
    """Tests removing a thumbnail works"""
    self.RemoveNTPDefaultThumbnails()
    for page in self.PAGES:
      self.AppendTab(pyauto.GURL(page['url']))

    thumbnails = self.GetNTPThumbnails()
    for thumbnail in thumbnails:
      self.assertEquals(thumbnail, self.GetNTPThumbnails()[0])
      self.RemoveNTPThumbnail(thumbnail)
      self.assertFalse(self._NTPContainsThumbnail(thumbnail))
    self.assertFalse(self.GetNTPThumbnails())

  def testIncognitoNotAppearInMostVisited(self):
    """Tests that visiting a page in incognito mode does cause it to appear in
    the Most Visited section"""
    self.RemoveNTPDefaultThumbnails()
    self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
    self.NavigateToURL(self.PAGES[0]['url'], 1, 0)
    self.assertFalse(self.GetNTPThumbnails())

  def testDifferentProfileNotAppearInMostVisited(self):
    """Tests that visiting a page in one profile does not cause it to appear in
    the Most Visited section of another."""
    self.RemoveNTPDefaultThumbnails()
    self.OpenNewBrowserWindowWithNewProfile()
    self.NavigateToURL(self.PAGES[0]['url'], 1, 0)
    self.assertFalse(self.GetNTPThumbnails())

  def testRestoreOncePinnedThumbnail(self):
    """Tests that after restoring a once pinned thumbnail, the thumbnail is
    not pinned"""
    self.RemoveNTPDefaultThumbnails()
    self.NavigateToURL(self.PAGES[0]['url'])
    thumbnail1 = self.GetNTPThumbnails()[0]
    self.PinNTPThumbnail(thumbnail1)
    self.RemoveNTPThumbnail(thumbnail1)
    self.RestoreAllNTPThumbnails()
    self.RemoveNTPDefaultThumbnails()
    self.assertFalse(self.IsNTPThumbnailPinned(thumbnail1))

  def testThumbnailPersistence(self):
    """Tests that thumbnails persist across Chrome restarts"""
    self.RemoveNTPDefaultThumbnails()
    for page in self.PAGES:
      self.AppendTab(pyauto.GURL(page['url']))
    thumbnails = self.GetNTPThumbnails()
    self.MoveNTPThumbnail(thumbnails[0], 1)
    thumbnails = self.GetNTPThumbnails()

    self.RestartBrowser(clear_profile=False)
    self.assertEqual(thumbnails, self.GetNTPThumbnails())

  def testRestoreAllRemovedThumbnails(self):
    """Tests restoring all removed thumbnails"""
    for page in self.PAGES:
      self.AppendTab(pyauto.GURL(page['url']))

    thumbnails = self.GetNTPThumbnails()
    for thumbnail in thumbnails:
      self.RemoveNTPThumbnail(thumbnail)

    self.RestoreAllNTPThumbnails()
    self.assertEquals(thumbnails, self.GetNTPThumbnails())

  def testThumbnailRanking(self):
    """Tests that the thumbnails are ordered according to visit count"""
    self.RemoveNTPDefaultThumbnails()
    for page in self.PAGES:
      self.AppendTab(pyauto.GURL(page['url']))
    thumbnails = self.GetNTPThumbnails()
    self.assertEqual(self.PAGES[0]['url'], self.GetNTPThumbnails()[0]['url'])
    self.AppendTab(pyauto.GURL(self.PAGES[1]['url']))
    self.assertEqual(self.PAGES[1]['url'], self.GetNTPThumbnails()[0]['url'])
    self.AppendTab(pyauto.GURL(self.PAGES[0]['url']))
    self.AppendTab(pyauto.GURL(self.PAGES[0]['url']))
    self.assertEqual(self.PAGES[0]['url'], self.GetNTPThumbnails()[0]['url'])

  def testPinnedThumbnailNeverMoves(self):
    """Tests that once a thumnail is pinned it never moves"""
    self.RemoveNTPDefaultThumbnails()
    for page in self.PAGES:
      self.AppendTab(pyauto.GURL(page['url']))
    self.PinNTPThumbnail(self.GetNTPThumbnails()[0])
    thumbnails = self.GetNTPThumbnails()
    self.AppendTab(pyauto.GURL(self.PAGES[1]['url']))
    self.assertEqual(thumbnails, self.GetNTPThumbnails())

  def testThumbnailTitleChangeAfterPageTitleChange(self):
    """Tests that once a page title changes, the thumbnail title changes too"""
    self.RemoveNTPDefaultThumbnails()
    self.NavigateToURL(self.PAGES[0]['url'])
    self.assertEqual(self.PAGES[0]['title'],
                     self.GetNTPThumbnails()[0]['title'])
    self.ExecuteJavascript('window.domAutomationController.send(' +
                           'document.title = "new title")')
    self.assertEqual('new title', self.GetNTPThumbnails()[0]['title'])

  def testCloseOneTab(self):
    """Tests that closing a tab populates the recently closed list"""
    self.RemoveNTPDefaultThumbnails()
    self.AppendTab(pyauto.GURL(self.PAGES[1]['url']))
    self.GetBrowserWindow(0).GetTab(1).Close(True)
    self.assertEqual(self.PAGES[1]['url'],
                     self.GetNTPRecentlyClosed()[0]['url'])
    self.assertEqual(self.PAGES[1]['title'],
                     self.GetNTPRecentlyClosed()[0]['title'])

  def testCloseOneWindow(self):
    """Tests that closing a window populates the recently closed list"""
    self.RemoveNTPDefaultThumbnails()
    self.OpenNewBrowserWindow(True)
    self.NavigateToURL(self.PAGES[0]['url'], 1, 0)
    self.AppendTab(pyauto.GURL(self.PAGES[1]['url']), 1)
    self.CloseBrowserWindow(1)
    expected = [{ u'type': u'window',
                  u'tabs': [
                  { u'type': u'tab',
                    u'url': self.PAGES[0]['url'],
                    u'direction': u'ltr' },
                  { u'type': u'tab',
                    u'url': self.PAGES[1]['url']}]
                }]
    self.assertEquals(expected, test_utils.StripUnmatchedKeys(
        self.GetNTPRecentlyClosed(), expected))

  def testCloseMultipleTabs(self):
    """Tests closing multiple tabs populates the Recently Closed section in
    order"""
    self.RemoveNTPDefaultThumbnails()
    self.AppendTab(pyauto.GURL(self.PAGES[0]['url']))
    self.AppendTab(pyauto.GURL(self.PAGES[1]['url']))
    self.GetBrowserWindow(0).GetTab(2).Close(True)
    self.GetBrowserWindow(0).GetTab(1).Close(True)
    expected = [{ u'type': u'tab',
                  u'url': self.PAGES[0]['url']
                },
                { u'type': u'tab',
                  u'url': self.PAGES[1]['url']
                }]
    self.assertEquals(expected, test_utils.StripUnmatchedKeys(
        self.GetNTPRecentlyClosed(), expected))

  def testCloseWindowWithOneTab(self):
    """Tests that closing a window with only one tab only shows up as a tab in
    the Recently Closed section"""
    self.RemoveNTPDefaultThumbnails()
    self.OpenNewBrowserWindow(True)
    self.NavigateToURL(self.PAGES[0]['url'], 1, 0)
    self.CloseBrowserWindow(1)
    expected = [{ u'type': u'tab',
                  u'url': self.PAGES[0]['url']
                }]
    self.assertEquals(expected, test_utils.StripUnmatchedKeys(
        self.GetNTPRecentlyClosed(), expected))

  def testCloseMultipleWindows(self):
    """Tests closing multiple windows populates the Recently Closed list"""
    self.RemoveNTPDefaultThumbnails()
    self.OpenNewBrowserWindow(True)
    self.NavigateToURL(self.PAGES[0]['url'], 1, 0)
    self.AppendTab(pyauto.GURL(self.PAGES[1]['url']), 1)
    self.OpenNewBrowserWindow(True)
    self.NavigateToURL(self.PAGES[1]['url'], 2, 0)
    self.AppendTab(pyauto.GURL(self.PAGES[0]['url']), 2)
    self.CloseBrowserWindow(2)
    self.CloseBrowserWindow(1)
    expected = [{ u'type': u'window',
                  u'tabs': [
                  { u'type': u'tab',
                    u'url': self.PAGES[0]['url'],
                    u'direction': u'ltr' },
                  { u'type': u'tab',
                    u'url': self.PAGES[1]['url']}]
                },
                { u'type': u'window',
                  u'tabs': [
                  { u'type': u'tab',
                    u'url': self.PAGES[1]['url'],
                    u'direction': u'ltr' },
                  { u'type': u'tab',
                    u'url': self.PAGES[0]['url']}]
                }]
    self.assertEquals(expected, test_utils.StripUnmatchedKeys(
        self.GetNTPRecentlyClosed(), expected))

  def testRecentlyClosedShowsUniqueItems(self):
    """Tests that the Recently Closed section does not show duplicate items"""
    self.RemoveNTPDefaultThumbnails()
    self.AppendTab(pyauto.GURL(self.PAGES[0]['url']))
    self.AppendTab(pyauto.GURL(self.PAGES[0]['url']))
    self.GetBrowserWindow(0).GetTab(1).Close(True)
    self.GetBrowserWindow(0).GetTab(1).Close(True)
    self.assertEquals(1, len(self.GetNTPRecentlyClosed()))

  def testRecentlyClosedIncognito(self):
    """Tests that we don't record closure of Incognito tabs or windows"""
    #self.RemoveNTPDefaultThumbnails()
    self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
    self.NavigateToURL(self.PAGES[0]['url'], 1, 0)
    self.AppendTab(pyauto.GURL(self.PAGES[0]['url']), 1)
    self.AppendTab(pyauto.GURL(self.PAGES[1]['url']), 1)
    self.GetBrowserWindow(1).GetTab(0).Close(True)
    self.assertFalse(self.GetNTPRecentlyClosed())
    self.CloseBrowserWindow(1)
    self.assertFalse(self.GetNTPRecentlyClosed())

  def _VerifyAppInfo(self, actual_info, expected_info):
    """Ensures that the actual app info contains the expected app info.

    This method assumes that both the actual and expected information for each
    app contains at least the 'title' attribute.  Both sets of info are
    considered to match if the actual info contains at least the specified
    expected info (if the actual info contains additional values that are not
    specified in the expected info, that's ok).  This function will fail the
    current test if both sets of info don't match.

    Args:
      actual_info: A list of dictionaries representing the information from
                   all apps that would currently be displayed on the NTP.
      expected_info: A corrresponding list of dictionaries representing the
                     information that is expected.
    """
    # Ensure all app info dictionaries contain at least the 'title' attribute.
    self.assertTrue(all(map(lambda app: 'title' in app, actual_info)) and
                    all(map(lambda app: 'title' in app, expected_info)),
                    msg='At least one app is missing the "title" attribute.')

    # Sort both app lists by title to ensure they're in a known order.
    actual_info = sorted(actual_info, key=lambda app: app['title'])
    expected_info = sorted(expected_info, key=lambda app: app['title'])

    # Ensure the expected info matches the actual info.
    self.assertTrue(len(actual_info) == len(expected_info),
                    msg='Expected %d app(s) on NTP, but got %d instead.' % (
                        len(expected_info), len(actual_info)))
    for i, expected_app in enumerate(expected_info):
      for attribute in expected_app:
        self.assertTrue(attribute in actual_info[i],
                        msg='Expected attribute "%s" not found in app info.' % (
                            attribute))
        self.assertTrue(expected_app[attribute] == actual_info[i][attribute],
                        msg='For attribute "%s", expected value "%s", but got '
                            '"%s".' % (attribute, expected_app[attribute],
                                       actual_info[i][attribute]))

  def _InstallAndVerifySamplePackagedApp(self):
    """Installs a sample packaged app and verifies the install is successful.

    Returns:
      The string ID of the installed app.
    """
    app_crx_file = os.path.abspath(os.path.join(
        self.DataDir(), 'pyauto_private', 'apps', 'countdown.crx'))
    return self.InstallExtension(app_crx_file)

  def testGetAppsInNewProfile(self):
    """Ensures that the only app in a new profile is the Web Store app."""
    app_info = self.GetNTPApps()
    self._VerifyAppInfo(app_info, self._EXPECTED_DEFAULT_APPS)

  def testGetAppsWhenInstallApp(self):
    """Ensures that an installed app is reflected in the app info in the NTP."""
    self._InstallAndVerifySamplePackagedApp()
    app_info = self.GetNTPApps()
    expected_app_info = [
      {
        u'title': u'Countdown'
      }
    ]
    expected_app_info.extend(self._EXPECTED_DEFAULT_APPS)
    self._VerifyAppInfo(app_info, expected_app_info)

  def testGetAppsWhenInstallNonApps(self):
    """Ensures installed non-apps are not reflected in the NTP app info."""
    # Install a regular extension and a theme.
    ext_crx_file = os.path.abspath(os.path.join(self.DataDir(), 'extensions',
                                                'page_action.crx'))
    self.InstallExtension(ext_crx_file)
    theme_crx_file = os.path.abspath(os.path.join(self.DataDir(), 'extensions',
                                                  'theme.crx'))
    self.SetTheme(theme_crx_file)
    # Verify that no apps are listed on the NTP except for the Web Store.
    app_info = self.GetNTPApps()
    self._VerifyAppInfo(app_info, self._EXPECTED_DEFAULT_APPS)

  def testUninstallApp(self):
    """Ensures that an uninstalled app is reflected in the NTP app info."""
    # First, install an app and verify that it exists in the NTP app info.
    installed_app_id = self._InstallAndVerifySamplePackagedApp()
    app_info = self.GetNTPApps()
    expected_app_info = [
      {
        u'title': u'Countdown'
      }
    ]
    expected_app_info.extend(self._EXPECTED_DEFAULT_APPS)
    self._VerifyAppInfo(app_info, expected_app_info)

    # Next, uninstall the app and verify that it is removed from the NTP.
    self.assertTrue(self.UninstallExtensionById(installed_app_id),
                    msg='Call to UninstallExtensionById() returned False.')
    app_info = self.GetNTPApps()
    self._VerifyAppInfo(app_info, self._EXPECTED_DEFAULT_APPS)

  def testCannotUninstallWebStore(self):
    """Ensures that the WebStore app cannot be uninstalled."""
    # Verify that the WebStore app is already installed in a fresh profile.
    app_info = self.GetNTPApps()
    self._VerifyAppInfo(app_info, self._EXPECTED_DEFAULT_APPS)
    self.assertTrue(app_info and 'id' in app_info[0],
                    msg='Cannot identify ID of WebStore app.')
    webstore_id = app_info[0]['id']

    # Attempt to uninstall the WebStore app and verify that it still exists
    # in the App info of the NTP even after we try to uninstall it.
    self.assertFalse(self.UninstallExtensionById(webstore_id),
                     msg='Call to UninstallExtensionById() returned True.')
    self._VerifyAppInfo(self.GetNTPApps(), self._EXPECTED_DEFAULT_APPS)

  def testLaunchAppWithDefaultSettings(self):
    """Verifies that an app can be launched with the default settings."""
    # Install an app.
    installed_app_id = self._InstallAndVerifySamplePackagedApp()

    # Launch the app from the NTP.
    self.LaunchApp(installed_app_id)

    # Verify that the second tab in the first window is the app launch URL.
    # It should be the second tab, not the first, since the call to LaunchApp
    # should have first opened the NTP in a new tab, and then launched the app
    # from there.
    info = self.GetBrowserInfo()
    actual_tab_url = info['windows'][0]['tabs'][1]['url']
    expected_app_url_start = 'chrome-extension://' + installed_app_id
    self.assertTrue(actual_tab_url.startswith(expected_app_url_start),
                    msg='The app was not launched.')

  def testLaunchAppRegularTab(self):
    """Verifies that an app can be launched in a regular tab."""
    installed_app_id = self._InstallAndVerifySamplePackagedApp()

    self.SetAppLaunchType(installed_app_id, 'regular', windex=0)
    self.LaunchApp(installed_app_id)

    # Verify that the second tab in the first window is the app launch URL.
    info = self.GetBrowserInfo()
    actual_tab_url = info['windows'][0]['tabs'][1]['url']
    expected_app_url_start = 'chrome-extension://' + installed_app_id
    self.assertTrue(actual_tab_url.startswith(expected_app_url_start),
                    msg='The app was not launched in a regular tab.')

  def testLaunchAppPinnedTab(self):
    """Verifies that an app can be launched in a pinned tab."""
    installed_app_id = self._InstallAndVerifySamplePackagedApp()

    self.SetAppLaunchType(installed_app_id, 'pinned', windex=0)
    self.LaunchApp(installed_app_id)

    # Verify that the first tab in the first window is the app launch URL, and
    # that it is a pinned tab.
    info = self.GetBrowserInfo()
    actual_tab_url = info['windows'][0]['tabs'][0]['url']
    expected_app_url_start = 'chrome-extension://' + installed_app_id
    self.assertTrue(actual_tab_url.startswith(expected_app_url_start) and
                    info['windows'][0]['tabs'][0]['pinned'],
                    msg='The app was not launched in a pinned tab.')

  def testLaunchAppFullScreen(self):
    """Verifies that an app can be launched in fullscreen mode."""
    installed_app_id = self._InstallAndVerifySamplePackagedApp()

    self.SetAppLaunchType(installed_app_id, 'fullscreen', windex=0)
    self.LaunchApp(installed_app_id)

    # Verify that the second tab in the first window is the app launch URL, and
    # that the window is fullscreen.
    info = self.GetBrowserInfo()
    actual_tab_url = info['windows'][0]['tabs'][1]['url']
    expected_app_url_start = 'chrome-extension://' + installed_app_id
    self.assertTrue(actual_tab_url.startswith(expected_app_url_start) and
                    info['windows'][0]['fullscreen'],
                    msg='The app was not launched in fullscreen mode.')

  def testLaunchAppNewWindow(self):
    """Verifies that an app can be launched in a new window."""
    installed_app_id = self._InstallAndVerifySamplePackagedApp()

    self.SetAppLaunchType(installed_app_id, 'window', windex=0)
    self.LaunchApp(installed_app_id)

    # Verify that a second window exists (at index 1), and that its first tab
    # is the app launch URL.
    info = self.GetBrowserInfo()
    self.assertTrue(len(info['windows']) == 2,
                    msg='A second window does not exist.')
    actual_tab_url = info['windows'][1]['tabs'][0]['url']
    expected_app_url_start = 'chrome-extension://' + installed_app_id
    self.assertTrue(actual_tab_url.startswith(expected_app_url_start),
                    msg='The app was not launched in the new window.')

if __name__ == '__main__':
  pyauto_functional.Main()
