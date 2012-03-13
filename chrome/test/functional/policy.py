#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os

import pyauto_functional  # must come before pyauto.
import policy_base
import pyauto_errors
import pyauto


class PolicyTest(policy_base.PolicyTestBase):
  """Tests that the effects of policies are being enforced as expected."""

  def _VerifyPrefIsLocked(self, key, defaultval, newval):
    """Verify the managed preferences cannot be modified.

    Args:
      key: The preference key that you want to modify
      defaultval: Default value of the preference that we are trying to modify
      newval: New value that we are trying to set
    """
    # Check if the default value of the preference is set as expected.
    self.assertEqual(
        self.GetPrefsInfo().Prefs(key), defaultval,
        msg='Expected preference value "%s" does not match actual value "%s".' %
        (defaultval, self.GetPrefsInfo().Prefs(key)))
    self.assertRaises(pyauto.JSONInterfaceError,
                      lambda: self.SetPrefs(key, newval))

  # TODO(frankf): Move tests dependending on this to plugins.py.
  def _GetPluginPID(self, plugin_name):
    """Fetch the pid of the plugin process with name |plugin_name|."""
    child_processes = self.GetBrowserInfo()['child_processes']
    plugin_type = 'Plug-in'
    for x in child_processes:
      if x['type'] == plugin_type and plugin_name in x['name']:
        return x['pid']
    return None

  def _IsBlocked(self, url):
    """Returns true if navigating to |url| is blocked."""
    self.NavigateToURL(url)
    blocked = self.GetActiveTabTitle() == url + ' is not available'
    ret = self.ExecuteJavascript("""
        var hasError = false;
        var error = document.getElementById('errorDetails');
        if (error) {
          hasError = error.textContent.indexOf('Error 138') == 0;
        }
        domAutomationController.send(hasError.toString());
    """)
    ret = ret == 'true'
    self.assertEqual(blocked, ret)
    return blocked

  def _IsJavascriptEnabled(self):
    """Returns true if Javascript is enabled, false otherwise."""
    try:
      ret = self.ExecuteJavascript('domAutomationController.send("done");')
      return ret == 'done'
    except pyauto.JSONInterfaceError as e:
      if 'Javascript execution was blocked' == str(e):
        logging.debug('The previous failure was expected')
        return False
      else:
        raise e

  def _IsWebGLEnabled(self):
    """Returns true if WebGL is enabled, false otherwise."""
    ret = self.GetDOMValue("""
        document.createElement('canvas').
            getContext('experimental-webgl') ? 'ok' : ''
    """)
    return ret == 'ok'

  def _RestartRenderer(self, windex=0):
    """Kills the current renderer, and reloads it again."""
    info = self.GetBrowserInfo()
    tab = self.GetActiveTabIndex()
    pid = info['windows'][windex]['tabs'][tab]['renderer_pid']
    self.KillRendererProcess(pid)
    self.ReloadActiveTab()

  def testBlacklistPolicy(self):
    """Tests the URLBlacklist and URLWhitelist policies."""
    # This is an end to end test and not an exaustive test of the filter format.
    policy = {
      'URLBlacklist': [
        'news.google.com',
        'chromium.org',
      ],
      'URLWhitelist': [
        'dev.chromium.org',
        'chromium.org/chromium-os',
      ]
    }
    self.SetPolicies(policy)

    self.assertTrue(self._IsBlocked('http://news.google.com/'))
    self.assertFalse(self._IsBlocked('http://www.google.com/'))
    self.assertFalse(self._IsBlocked('http://google.com/'))

    self.assertTrue(self._IsBlocked('http://chromium.org/'))
    self.assertTrue(self._IsBlocked('http://www.chromium.org/'))
    self.assertFalse(self._IsBlocked('http://dev.chromium.org/'))
    self.assertFalse(self._IsBlocked('http://chromium.org/chromium-os/testing'))

  def testBookmarkBarPolicy(self):
    """Tests the BookmarkBarEnabled policy."""
    self.NavigateToURL('about:blank')
    self.assertFalse(self.GetBookmarkBarVisibility())
    self.assertFalse(self.IsBookmarkBarDetached())

    # It should be visible in detached state, in the NTP.
    self.NavigateToURL('chrome://newtab')
    self.assertFalse(self.GetBookmarkBarVisibility())
    self.assertTrue(self.IsBookmarkBarDetached())

    policy = {
      'BookmarkBarEnabled': True
    }
    self.SetPolicies(policy)

    self.assertTrue(self.WaitForBookmarkBarVisibilityChange(True))
    self.assertTrue(self.GetBookmarkBarVisibility())
    self.assertFalse(self.IsBookmarkBarDetached())
    # The accelerator should be disabled by the policy.
    self.ApplyAccelerator(pyauto.IDC_SHOW_BOOKMARK_BAR)
    self.assertTrue(self.WaitForBookmarkBarVisibilityChange(True))
    self.assertTrue(self.GetBookmarkBarVisibility())
    self.assertFalse(self.IsBookmarkBarDetached())

    policy['BookmarkBarEnabled'] = False
    self.SetPolicies(policy)

    self.assertTrue(self.WaitForBookmarkBarVisibilityChange(False))
    self.assertFalse(self.GetBookmarkBarVisibility())
    self.ApplyAccelerator(pyauto.IDC_SHOW_BOOKMARK_BAR)
    self.assertTrue(self.WaitForBookmarkBarVisibilityChange(False))
    self.assertFalse(self.GetBookmarkBarVisibility())
    # When disabled by policy, it should never be displayed at all,
    # not even on the NTP.
    self.assertFalse(self.IsBookmarkBarDetached())

  def testJavascriptPolicies(self):
    """Tests the Javascript policies."""
    # The navigation to about:blank after each policy reset is to reset the
    # content settings state.
    policy = {}
    self.SetPolicies(policy)
    self.assertTrue(self._IsJavascriptEnabled())
    self.assertTrue(self.IsMenuCommandEnabled(pyauto.IDC_DEV_TOOLS))
    self.assertTrue(self.IsMenuCommandEnabled(pyauto.IDC_DEV_TOOLS_CONSOLE))

    policy['DeveloperToolsDisabled'] = True
    self.SetPolicies(policy)
    self.assertTrue(self._IsJavascriptEnabled())
    self.assertFalse(self.IsMenuCommandEnabled(pyauto.IDC_DEV_TOOLS))
    self.assertFalse(self.IsMenuCommandEnabled(pyauto.IDC_DEV_TOOLS_CONSOLE))

    policy['DeveloperToolsDisabled'] = False
    self.SetPolicies(policy)
    self.assertTrue(self._IsJavascriptEnabled())
    self.assertTrue(self.IsMenuCommandEnabled(pyauto.IDC_DEV_TOOLS))
    self.assertTrue(self.IsMenuCommandEnabled(pyauto.IDC_DEV_TOOLS_CONSOLE))

    # The Developer Tools still work when javascript is disabled.
    policy['JavascriptEnabled'] = False
    self.SetPolicies(policy)
    self.NavigateToURL('about:blank')
    self.assertFalse(self._IsJavascriptEnabled())
    self.assertTrue(self.IsMenuCommandEnabled(pyauto.IDC_DEV_TOOLS))
    self.assertTrue(self.IsMenuCommandEnabled(pyauto.IDC_DEV_TOOLS_CONSOLE))
    # Javascript is always enabled for internal Chrome pages.
    self.NavigateToURL('chrome://settings')
    self.assertTrue(self._IsJavascriptEnabled())

    # The Developer Tools can be explicitly disabled.
    policy['DeveloperToolsDisabled'] = True
    self.SetPolicies(policy)
    self.NavigateToURL('about:blank')
    self.assertFalse(self._IsJavascriptEnabled())
    self.assertFalse(self.IsMenuCommandEnabled(pyauto.IDC_DEV_TOOLS))
    self.assertFalse(self.IsMenuCommandEnabled(pyauto.IDC_DEV_TOOLS_CONSOLE))

    # Javascript can also be disabled with content settings policies.
    policy = {
      'DefaultJavaScriptSetting': 2,
    }
    self.SetPolicies(policy)
    self.NavigateToURL('about:blank')
    self.assertFalse(self._IsJavascriptEnabled())
    self.assertTrue(self.IsMenuCommandEnabled(pyauto.IDC_DEV_TOOLS))
    self.assertTrue(self.IsMenuCommandEnabled(pyauto.IDC_DEV_TOOLS_CONSOLE))

    # The content setting overrides JavascriptEnabled.
    policy = {
      'DefaultJavaScriptSetting': 1,
      'JavascriptEnabled': False,
    }
    self.SetPolicies(policy)
    self.NavigateToURL('about:blank')
    self.assertTrue(self._IsJavascriptEnabled())

  def testDisable3DAPIs(self):
    """Tests the policy that disables the 3D APIs."""
    self.assertFalse(self.GetPrefsInfo().Prefs(pyauto.kDisable3DAPIs))
    self.assertTrue(self._IsWebGLEnabled())

    self.SetPolicies({
        'Disable3DAPIs': True
    })
    self.assertTrue(self.GetPrefsInfo().Prefs(pyauto.kDisable3DAPIs))
    # The Disable3DAPIs policy only applies updated values to new renderers.
    self._RestartRenderer()
    self.assertFalse(self._IsWebGLEnabled())

  def testStartupOptions(self):
    """Verify that user cannot modify the startup page options."""
    policy = {
      'RestoreOnStartup': 4,
      'RestoreOnStartupURLs': ['http://chromium.org']
    }
    self.SetPolicies(policy)
    # Verify startup option
    self.assertEquals(4, self.GetPrefsInfo().Prefs(pyauto.kRestoreOnStartup))
    self.assertRaises(
        pyauto.JSONInterfaceError,
        lambda: self.SetPrefs(pyauto.kRestoreOnStartup, 1))
    policy['RestoreOnStartup'] = 0
    self.SetPolicies(policy)
    self.assertEquals(0, self.GetPrefsInfo().Prefs(pyauto.kRestoreOnStartup))
    self.assertRaises(
        pyauto.JSONInterfaceError,
        lambda: self.SetPrefs(pyauto.kRestoreOnStartup, 1))
    # Verify URLs to open on startup
    self.assertEquals(
        ['http://chromium.org'],
        self.GetPrefsInfo().Prefs(pyauto.kURLsToRestoreOnStartup))
    self.assertRaises(
        pyauto.JSONInterfaceError,
        lambda: self.SetPrefs(pyauto.kURLsToRestoreOnStartup,
                              ['http://www.google.com']))

  def testHomePageOptions(self):
    """Verify that we cannot modify Homepage URL."""
    policy = {
      'HomepageLocation': 'http://chromium.org',
      'HomepageIsNewTabPage': True
    }
    self.SetPolicies(policy)
    # Try to configure home page URL
    self.assertEquals('http://chromium.org',
                      self.GetPrefsInfo().Prefs('homepage'))
    self.assertRaises(
        pyauto.JSONInterfaceError,
        lambda: self.SetPrefs('homepage', 'http://www.google.com'))
    # Try to remove NTP as home page
    self.assertTrue(self.GetPrefsInfo().Prefs(pyauto.kHomePageIsNewTabPage))
    self.assertRaises(
        pyauto.JSONInterfaceError,
        lambda: self.SetPrefs(pyauto.kHomePageIsNewTabPage, False))

  def testShowHomeButton(self):
    """Verify that home button option cannot be modified when it's managed."""
    policy = {'ShowHomeButton': True}
    self.SetPolicies(policy)
    self._VerifyPrefIsLocked(pyauto.kShowHomeButton, True, False)
    policy = {'ShowHomeButton': False}
    self.SetPolicies(policy)
    self._VerifyPrefIsLocked(pyauto.kShowHomeButton, False, True)

  def testInstantEnabled(self):
    """Verify that Instant option cannot be modified."""
    policy = {'InstantEnabled': True}
    self.SetPolicies(policy)
    self._VerifyPrefIsLocked(pyauto.kInstantEnabled, True, False)
    policy = {'InstantEnabled': False}
    self.SetPolicies(policy)
    self._VerifyPrefIsLocked(pyauto.kInstantEnabled, False, True)

  def testPasswordManagerEnabled(self):
    """Verify that password manager preference cannot be modified."""
    policy = {'PasswordManagerEnabled': True}
    self.SetPolicies(policy)
    self._VerifyPrefIsLocked(pyauto.kPasswordManagerEnabled, True, False)
    policy = {'PasswordManagerEnabled': False}
    self.SetPolicies(policy)
    self._VerifyPrefIsLocked(pyauto.kPasswordManagerEnabled, False, True)

  def testPasswordManagerNotAllowShowPasswords(self):
    """Verify that the preference not to show passwords cannot be modified."""
    policy = {'PasswordManagerAllowShowPasswords': False}
    self.SetPolicies(policy)
    self._VerifyPrefIsLocked(pyauto.kPasswordManagerAllowShowPasswords,
                                   False, True)
    policy = {'PasswordManagerAllowShowPasswords': True}
    self.SetPolicies(policy)
    self._VerifyPrefIsLocked(pyauto.kPasswordManagerAllowShowPasswords,
                                   True, False)

  def testPrivacyPrefs(self):
    """Verify that the managed preferences cannot be modified."""
    policy = {
      'AlternateErrorPagesEnabled': True,
      'AutofillEnabled': False,
      'DnsPrefetchingEnabled': True,
      'SafeBrowsingEnabled': True,
      'SearchSuggestEnabled': True,
    }
    self.SetPolicies(policy)
    prefs_list = [
        # (preference key, default value, new value)
        (pyauto.kAlternateErrorPagesEnabled, True, False),
        (pyauto.kAutofillEnabled, False, True),
        (pyauto.kNetworkPredictionEnabled, True, False),
        (pyauto.kSafeBrowsingEnabled, True, False),
        (pyauto.kSearchSuggestEnabled, True, False),
        ]
    # Check if the policies got applied by trying to modify
    for key, defaultval, newval in prefs_list:
      logging.info('Checking pref %s', key)
      self._VerifyPrefIsLocked(key, defaultval, newval)

  def testClearSiteDataOnExit(self):
    """Verify that clear data on exit preference cannot be modified."""
    policy = {'ClearSiteDataOnExit': True}
    self.SetPolicies(policy)
    self._VerifyPrefIsLocked(pyauto.kClearSiteDataOnExit, True, False)
    policy = {'ClearSiteDataOnExit': False}
    self.SetPolicies(policy)
    self._VerifyPrefIsLocked(pyauto.kClearSiteDataOnExit, False, True)

  def testBlockThirdPartyCookies(self):
    """Verify that third party cookies can be disabled."""
    policy = {'BlockThirdPartyCookies': True}
    self.SetPolicies(policy)
    self._VerifyPrefIsLocked(pyauto.kBlockThirdPartyCookies, True, False)
    policy = {'BlockThirdPartyCookies': False}
    self.SetPolicies(policy)
    self._VerifyPrefIsLocked(pyauto.kBlockThirdPartyCookies, False, True)

  def testApplicationLocaleValue(self):
    """Verify that Chrome can be launched only in a specific locale."""
    if self.IsWin():
      policy = {'ApplicationLocaleValue': 'hi'}
      self.SetPolicies(policy)
      self.assertTrue(
          'hi' in self.GetPrefsInfo().Prefs()['intl']['accept_languages'],
          msg='Chrome locale is not Hindi.')
      # TODO(sunandt): Try changing the application locale to another language.
    else:
      raise NotImplementedError()

  def testDeveloperToolsDisabled(self):
    """Verify that devtools window cannot be launched."""
    # DevTools process can be seen by PyAuto only when it's undocked.
    policy = {'DeveloperToolsDisabled': True}
    self.SetPolicies(policy)
    self.SetPrefs(pyauto.kDevToolsOpenDocked, False)
    self.ApplyAccelerator(pyauto.IDC_DEV_TOOLS)
    self.assertEquals(1, len(self.GetBrowserInfo()['windows']),
                      msg='Devtools window launched.')
    policy = {'DeveloperToolsDisabled': False}
    self.SetPolicies(policy)
    self.ApplyAccelerator(pyauto.IDC_DEV_TOOLS)
    self.assertEquals(2, len(self.GetBrowserInfo()['windows']),
                      msg='Devtools window not launched.')

  def testDisableSPDY(self):
    """Verify that SPDY is disabled."""
    policy = {'DisableSpdy': True}
    self.SetPolicies(policy)
    self.NavigateToURL('chrome://net-internals/#spdy')
    self.assertEquals(0, self.FindInPage('SPDY Enabled: true')['match_count'])
    self.assertEquals(
        1,
        self.FindInPage('SPDY Enabled: false', tab_index=0)['match_count'],
        msg='SPDY is not disabled.')
    policy = {'DisableSpdy': False}
    self.SetPolicies(policy)
    self.GetBrowserWindow(0).GetTab(0).Reload()
    self.assertEquals(
        1,
        self.FindInPage('SPDY Enabled: true', tab_index=0)['match_count'],
        msg='SPDY is not disabled.')

  def testDisabledPlugins(self):
    """Verify that disabled plugins cannot be enabled."""
    policy = {'DisabledPlugins': ['Shockwave Flash']}
    self.SetPolicies(policy)
    for plugin in self.GetPluginsInfo().Plugins():
      if 'Flash' in plugin['name']:
        self.assertRaises(pyauto.JSONInterfaceError,
                          lambda: self.EnablePlugin(plugin['path']))
        return

  def testDisabledPluginsException(self):
    """Verify that plugins given exceptions can be managed by users.

    Chrome PDF Viewer is disabled using DisabledPlugins policy.
    User can still toggle the plugin setting when an exception is given for a
    plugin. So we are trying to enable Chrome PDF Viewer.
    """
    policy = {
      'DisabledPlugins': ['Chrome PDF Viewer'],
      'DisabledPluginsExceptions': ['Chrome PDF Viewer']
    }
    self.SetPolicies(policy)
    for plugin in self.GetPluginsInfo().Plugins():
      if 'Chrome PDF Viewer' in plugin['name']:
        self.EnablePlugin(plugin['path'])
        return

  def testEnabledPlugins(self):
    """Verify that enabled plugins cannot be disabled."""
    policy = {'EnabledPlugins': 'Java'}
    self.SetPolicies(policy)
    for plugin in self.GetPluginsInfo().Plugins():
      if 'Java' in plugin['name']:
        self.assertRaises(pyauto.JSONInterfaceError,
                          lambda: self.DisablePlugin(plugin['path']))
        return
    logging.debug('Java is not present.')

  def testAlwaysAuthorizePlugins(self):
    """Verify plugins are always allowed to run when policy is set."""
    policy = {'AlwaysAuthorizePlugins': True}
    self.SetPolicies(policy)
    url = self.GetFileURLForDataPath('plugin', 'java_new.html')
    self.NavigateToURL(url)
    self.assertFalse(self.WaitForInfobarCount(1))
    pid = self._GetPluginPID('Java')
    self.assertTrue(pid, 'No plugin process for java')
    policy = {'AlwaysAuthorizePlugins': False}
    self.NavigateToURL(url)
    self.assertFalse(self.WaitForInfobarCount(1))
    pid = self._GetPluginPID('Java')
    self.assertTrue(pid, 'No plugin process for java')

  def testSetDownloadDirectory(self):
    """Verify download directory and prompt cannot be modified."""
    # Check for changing the download directory location
    download_default_dir = self.GetDownloadDirectory().value()
    self.assertEqual(
        download_default_dir,
        self.GetPrefsInfo().Prefs()['download']['default_directory'],
        msg='Downloads directory is not set correctly.')
    self.SetPrefs(pyauto.kDownloadDefaultDirectory, 'download')
    new_download_dir = os.path.abspath(os.path.join(download_default_dir,
                                                    os.pardir))
    policy = {'DownloadDirectory': new_download_dir}
    self.SetPolicies(policy)
    self.assertEqual(
        new_download_dir,
        self.GetPrefsInfo().Prefs()['download']['default_directory'],
        msg='Downloads directory is not set correctly.')
    self.assertRaises(
        pyauto.JSONInterfaceError,
        lambda: self.SetPrefs(pyauto.kDownloadDefaultDirectory,
                              'download'))
    # Check for changing the option 'Ask for each download'
    self.assertRaises(
        pyauto.JSONInterfaceError,
        lambda: self.SetPrefs(pyauto.kPromptForDownload, True))

  def testIncognitoEnabled(self):
    """Verify that incognito window can be launched."""
    policy = {'IncognitoEnabled': False}
    self.SetPolicies(policy)
    self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
    self.assertEquals(1, self.GetBrowserWindowCount())
    policy = {'IncognitoEnabled': True}
    self.SetPolicies(policy)
    self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
    self.assertEquals(2, self.GetBrowserWindowCount())

  def testSavingBrowserHistoryDisabled(self):
    """Verify that browsing history is not being saved."""
    policy = {'SavingBrowserHistoryDisabled': True}
    self.SetPolicies(policy)
    url = self.GetFileURLForPath(os.path.join(self.DataDir(), 'empty.html'))
    self.NavigateToURL(url)
    self.assertFalse(self.GetHistoryInfo().History(),
                     msg='History is being saved.')
    policy = {'SavingBrowserHistoryDisabled': False}
    self.SetPolicies(policy)
    self.NavigateToURL(url)
    self.assertTrue(self.GetHistoryInfo().History(),
                     msg='History not is being saved.')

  def testTranslateEnabled(self):
    """Verify that translate happens if policy enables it."""
    policy = {'TranslateEnabled': True}
    self.SetPolicies(policy)
    self.assertTrue(self.GetPrefsInfo().Prefs(pyauto.kEnableTranslate))
    url = self.GetFileURLForDataPath('translate', 'es', 'google.html')
    self.NavigateToURL(url)
    self.assertTrue(self.WaitForInfobarCount(1))
    translate_info = self.GetTranslateInfo()
    self.assertEqual('es', translate_info['original_language'])
    self.assertFalse(translate_info['page_translated'])
    self.assertTrue(translate_info['can_translate_page'])
    self.assertTrue('translate_bar' in translate_info)
    self._VerifyPrefIsLocked(pyauto.kEnableTranslate, True, False)
    policy = {'TranslateEnabled': False}
    self.SetPolicies(policy)
    self.assertFalse(self.GetPrefsInfo().Prefs(pyauto.kEnableTranslate))
    self.NavigateToURL(url)
    self.assertFalse(self.WaitForInfobarCount(1))
    self._VerifyPrefIsLocked(pyauto.kEnableTranslate, False, True)

  def testEditBookmarksEnabled(self):
    """Verify that bookmarks can be edited if policy sets it."""
    policy = {'EditBookmarksEnabled': True}
    self.SetPolicies(policy)
    self._VerifyPrefIsLocked(pyauto.kEditBookmarksEnabled, True, False)
    policy = {'EditBookmarksEnabled': False}
    self.SetPolicies(policy)
    self._VerifyPrefIsLocked(pyauto.kEditBookmarksEnabled, False, True)

  def testDefaultSearchProviderOptions(self):
    """Verify a default search is performed when using omnibox."""
    policy = {
      'DefaultSearchProviderEnabled': True,
      'DefaultSearchProviderEncodings': ['UTF-8', 'UTF-16', 'GB2312',
                                         'ISO-8859-1'],
      'DefaultSearchProviderIconURL': 'http://search.my.company/favicon.ico',
      'DefaultSearchProviderInstantURL': ('http://search.my.company/'
                                          'suggest?q={searchTerms}'),
      'DefaultSearchProviderKeyword': 'mis',
      'DefaultSearchProviderName': 'My Intranet Search',
      'DefaultSearchProviderSearchURL': ('http://search.my.company/'
                                         'search?q={searchTerms}'),
      'DefaultSearchProviderSuggestURL': ('http://search.my.company/'
                                          'suggest?q={searchTerms}'),
    }
    self.SetPolicies(policy)
    self._VerifyPrefIsLocked(pyauto.kDefaultSearchProviderEnabled, True,
                             False)
    intranet_engine = [x for x in self.GetSearchEngineInfo()
                       if x['keyword'] == 'mis']
    self.assertTrue(intranet_engine)
    self.assertTrue(intranet_engine[0]['is_default'])
    self.SetOmniboxText('google chrome')
    self.WaitUntilOmniboxQueryDone()
    self.OmniboxAcceptInput()
    self.assertTrue('search.my.company' in self.GetActiveTabURL().spec())
    policy = {
      'DefaultSearchProviderEnabled': False,
    }
    self.SetPolicies(policy)
    self._VerifyPrefIsLocked(pyauto.kDefaultSearchProviderEnabled, False,
                             True)
    self.SetOmniboxText('deli')
    self.WaitUntilOmniboxQueryDone()
    self.assertRaises(pyauto.JSONInterfaceError,
                      lambda: self.OmniboxAcceptInput())


if __name__ == '__main__':
  pyauto_functional.Main()
