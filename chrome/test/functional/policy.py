#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
    self.assertRaises(pyauto_errors.JSONInterfaceError,
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
    except pyauto_errors.JSONInterfaceError as e:
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
    self.NavigateToURL('chrome://settings-frame')
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
    policy = {'EnabledPlugins': ['Shockwave Flash']}
    self.SetPolicies(policy)
    for plugin in self.GetPluginsInfo().Plugins():
      if 'Flash' in plugin['name']:
        self.assertRaises(pyauto.JSONInterfaceError,
                          lambda: self.DisablePlugin(plugin['path']))
        return
    logging.debug('Flash is not present.')

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

  # Needed for extension tests
  _GOOD_CRX_ID = 'ldnnhddmnhbkjipkidpdiheffobcpfmf'
  _ADBLOCK_CRX_ID = 'dojnnbeimaimaojcialkkgajdnefpgcn'
  _SCREEN_CAPTURE_CRX_ID = 'cpngackimfmofbokmjmljamhdncknpmg'

  def _BuildCRXPath(self, crx_file_name):
    """Returns the complete path to a crx_file in the data directory.

    Args:
      crx_file_name: The file name of the extension

    Returns:
      The full path to the crx in the data directory
    """
    return os.path.abspath(os.path.join(self.DataDir(), 'extensions',
                                        crx_file_name))

  def _AttemptExtensionInstallThatShouldFail(self, crx_file_name):
    """Attempts to install an extension, raises an exception if installed.

    Args:
      crx_file_name: The file name of the extension
    """
    install_failed = True
    try:
      self.InstallExtension(self._BuildCRXPath(crx_file_name))
      install_failed = False
    except pyauto_errors.JSONInterfaceError as e:
      self.assertEqual(e[0], 'Extension could not be installed',
                       msg='The extension failed to install which is expected. '
                       ' However it failed due to an unexpected reason: %s'
                       % e[0])
    self.assertTrue(install_failed, msg='The extension %s did not throw an '
                    'exception when installation was attempted.  This most '
                    'likely means it succeeded, which it should not.')

  def _AttemptExtensionInstallThatShouldPass(self, crx_file_name):
    """Attempts to install an extension, raises an exception if not installed.

    Args:
      crx_file_name: The file name of the extension
    """
    ext_id = self.InstallExtension(self._BuildCRXPath(crx_file_name))
    self.assertTrue(self._CheckForExtensionByID(ext_id),
                    msg='The %s extension was not install even though '
                    'it should have been.' % crx_file_name)

  def _CheckForExtensionByID(self, extension_id):
    """Returns if an extension is installed.

    Args:
      extension_id: The id of the extension

    Returns:
      True if the extension is installed; False otherwise
    """
    all_extensions = [extension['id'] for extension in self.GetExtensionsInfo()]
    return extension_id in all_extensions

  def _RemoveTestingExtensions(self):
    """Temporary method that cleans state for extension test.

    Currently the tear down method for policy_base does not clear the user
    state.  See crosbug.com/27227.
    """
    if self._CheckForExtensionByID(self._SCREEN_CAPTURE_CRX_ID):
      self.UninstallExtensionById(self._SCREEN_CAPTURE_CRX_ID)
    if self._CheckForExtensionByID(self._GOOD_CRX_ID):
      self.UninstallExtensionById(self._GOOD_CRX_ID)
      self.NavigateToURL('chrome:extensions')
    if self._CheckForExtensionByID(self._ADBLOCK_CRX_ID):
      self.UninstallExtensionById(self._ADBLOCK_CRX_ID)
    # There is an issue where if you uninstall and reinstall and extension
    # quickly with self.InstallExtension, the reinstall fails.  This is a hack
    # to fix it.  Bug coming soon.
    self.NavigateToURL('chrome:extensions')

  def testExtensionInstallPopulatedBlacklist(self):
    """Verify blacklisted extensions cannot be installed."""
    # TODO(krisr): Remove this when crosbug.com/27227 is fixed.
    self._RemoveTestingExtensions()
    # Blacklist good.crx
    self.SetPolicies({
      'ExtensionInstallBlacklist': [self._GOOD_CRX_ID]
    })
    self._AttemptExtensionInstallThatShouldFail('good.crx')
    # Check adblock is installed.
    self._AttemptExtensionInstallThatShouldPass('adblock.crx')

  def testExtensionInstallFailWithGlobalBlacklist(self):
    """Verify no extensions can be installed when all are blacklisted."""
    # TODO(krisr): Remove this when crosbug.com/27227 is fixed.
    self._RemoveTestingExtensions()
    # Block installs of all extensions
    self.SetPolicies({
      'ExtensionInstallBlacklist': ['*']
    })
    self._AttemptExtensionInstallThatShouldFail('good.crx')
    self._AttemptExtensionInstallThatShouldFail('adblock.crx')

  def testExtensionInstallWithGlobalBlacklistAndWhitelistedExtension(self):
    """Verify whitelisted extension is installed when all are blacklisted."""
    # TODO(krisr): Remove this when crosbug.com/27227 is fixed.
    self._RemoveTestingExtensions()
    # Block installs of all extensions, but whitelist adblock.crx
    self.SetPolicies({
      'ExtensionInstallBlacklist': ['*'],
      'ExtensionInstallWhitelist': [self._ADBLOCK_CRX_ID]
    })
    self._AttemptExtensionInstallThatShouldFail('good.crx')
    self._AttemptExtensionInstallThatShouldPass('adblock.crx')

  # TODO(krisr): Enable this test once we figure out why it isn't downloading
  # the extension, crbug.com/118123.
  def testExtensionInstallFromForceList(self):
    """Verify force install extensions are installed."""
    # TODO(krisr): Remove this when crosbug.com/27227 is fixed.
    self._RemoveTestingExtensions()
    # Force an extension download from the webstore.
    self.SetPolicies({
      'ExtensionInstallForcelist': [self._SCREEN_CAPTURE_CRX_ID],
    })
    # Give the system 30 seconds to go get this extension.  We are not sure how
    # long it will take the policy to take affect and download the extension.
    self.assertTrue(self.WaitUntil(lambda:
      self._CheckForExtensionByID(self._SCREEN_CAPTURE_CRX_ID),
      expect_retval=True),
      msg='The force install extension was never installed.')


if __name__ == '__main__':
  pyauto_functional.Main()
