#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os

import pyauto_functional  # must come before pyauto.
import policy_base
import policy_test_cases
import pyauto_errors
import pyauto


class PolicyTest(policy_base.PolicyTestBase):
  """Tests that the effects of policies are being enforced as expected."""

  def _GetPrefIsManagedError(self, pref, expected_value):
    """Verify the managed preferences cannot be modified.

    Args:
      pref: The preference key that you want to modify.
      expected_value: Current value of the preference.

    Returns:
      Error message if any, None if pref is successfully managed.
    """
    # Check if the current value of the preference is set as expected.
    local_state_pref_value = self.GetLocalStatePrefsInfo().Prefs(pref)
    profile_pref_value = self.GetPrefsInfo().Prefs(pref)
    actual_value = (profile_pref_value if profile_pref_value is not None else
                    local_state_pref_value)
    if actual_value is None:
      return 'Preference %s is not registered.'  % pref
    elif actual_value != expected_value:
      return ('Preference value "%s" does not match policy value "%s".' %
              (actual_value, expected_value))
    # If the preference is managed, this should throw an exception.
    try:
      if profile_pref_value is not None:
        self.SetPrefs(pref, expected_value)
      else:
        self.SetLocalStatePrefs(pref, expected_value)
    except pyauto_errors.JSONInterfaceError, e:
      if str(e) != 'pref is managed. cannot be changed.':
        return str(e)
      else:
        return None
    else:
      return 'Preference can be set even though a policy is in effect.'

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

  def setUp(self):
    policy_base.PolicyTestBase.setUp(self)
    if self.IsChromeOS():
      self.LoginWithTestAccount()

  def testPolicyToPrefMapping(self):
    """Verify that simple user policies map to corresponding prefs.

    Also verify that once these policies are in effect, the prefs cannot be
    modified by the user.
    """
    total = 0
    fails = []
    policy_prefs = policy_test_cases.PolicyPrefsTestCases
    for policy, values in policy_prefs.policies.iteritems():
      pref = values[policy_prefs.INDEX_PREF]
      value = values[policy_prefs.INDEX_VALUE]
      os = values[policy_prefs.INDEX_OS]
      if not pref or self.GetPlatform() not in os:
        continue
      self.SetUserPolicy({policy: value})
      error = self._GetPrefIsManagedError(getattr(pyauto, pref), value)
      if error:
        fails.append('%s: %s' % (policy, error))
      total += 1
    self.assertFalse(fails, msg='%d of %d policies failed.\n%s' %
                     (len(fails), total, '\n'.join(fails)))

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
    self.SetUserPolicy(policy)

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
    self.SetUserPolicy(policy)

    self.assertTrue(self.WaitForBookmarkBarVisibilityChange(True))
    self.assertTrue(self.GetBookmarkBarVisibility())
    self.assertFalse(self.IsBookmarkBarDetached())
    # The accelerator should be disabled by the policy.
    self.ApplyAccelerator(pyauto.IDC_SHOW_BOOKMARK_BAR)
    self.assertTrue(self.WaitForBookmarkBarVisibilityChange(True))
    self.assertTrue(self.GetBookmarkBarVisibility())
    self.assertFalse(self.IsBookmarkBarDetached())

    policy['BookmarkBarEnabled'] = False
    self.SetUserPolicy(policy)

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
    self.SetUserPolicy(policy)
    self.assertTrue(self._IsJavascriptEnabled())
    self.assertTrue(self.IsMenuCommandEnabled(pyauto.IDC_DEV_TOOLS))
    self.assertTrue(self.IsMenuCommandEnabled(pyauto.IDC_DEV_TOOLS_CONSOLE))

    policy['DeveloperToolsDisabled'] = True
    self.SetUserPolicy(policy)
    self.assertTrue(self._IsJavascriptEnabled())
    self.assertFalse(self.IsMenuCommandEnabled(pyauto.IDC_DEV_TOOLS))
    self.assertFalse(self.IsMenuCommandEnabled(pyauto.IDC_DEV_TOOLS_CONSOLE))

    policy['DeveloperToolsDisabled'] = False
    self.SetUserPolicy(policy)
    self.assertTrue(self._IsJavascriptEnabled())
    self.assertTrue(self.IsMenuCommandEnabled(pyauto.IDC_DEV_TOOLS))
    self.assertTrue(self.IsMenuCommandEnabled(pyauto.IDC_DEV_TOOLS_CONSOLE))

    # The Developer Tools still work when javascript is disabled.
    policy['JavascriptEnabled'] = False
    self.SetUserPolicy(policy)
    self.NavigateToURL('about:blank')
    self.assertFalse(self._IsJavascriptEnabled())
    self.assertTrue(self.IsMenuCommandEnabled(pyauto.IDC_DEV_TOOLS))
    self.assertTrue(self.IsMenuCommandEnabled(pyauto.IDC_DEV_TOOLS_CONSOLE))
    # Javascript is always enabled for internal Chrome pages.
    self.NavigateToURL('chrome://settings-frame')
    self.assertTrue(self._IsJavascriptEnabled())

    # The Developer Tools can be explicitly disabled.
    policy['DeveloperToolsDisabled'] = True
    self.SetUserPolicy(policy)
    self.NavigateToURL('about:blank')
    self.assertFalse(self._IsJavascriptEnabled())
    self.assertFalse(self.IsMenuCommandEnabled(pyauto.IDC_DEV_TOOLS))
    self.assertFalse(self.IsMenuCommandEnabled(pyauto.IDC_DEV_TOOLS_CONSOLE))

    # Javascript can also be disabled with content settings policies.
    policy = {
      'DefaultJavaScriptSetting': 2,
    }
    self.SetUserPolicy(policy)
    self.NavigateToURL('about:blank')
    self.assertFalse(self._IsJavascriptEnabled())
    self.assertTrue(self.IsMenuCommandEnabled(pyauto.IDC_DEV_TOOLS))
    self.assertTrue(self.IsMenuCommandEnabled(pyauto.IDC_DEV_TOOLS_CONSOLE))

    # The content setting overrides JavascriptEnabled.
    policy = {
      'DefaultJavaScriptSetting': 1,
      'JavascriptEnabled': False,
    }
    self.SetUserPolicy(policy)
    self.NavigateToURL('about:blank')
    self.assertTrue(self._IsJavascriptEnabled())

  def testDisable3DAPIs(self):
    """Tests the policy that disables the 3D APIs."""
    self.assertFalse(self.GetPrefsInfo().Prefs(pyauto.kDisable3DAPIs))
    self.assertTrue(self._IsWebGLEnabled())

    self.SetUserPolicy({
        'Disable3DAPIs': True
    })
    self.assertTrue(self.GetPrefsInfo().Prefs(pyauto.kDisable3DAPIs))
    # The Disable3DAPIs policy only applies updated values to new renderers.
    self._RestartRenderer()
    self.assertFalse(self._IsWebGLEnabled())

  def testStartupOptionsURLs(self):
    """Verify that user cannot modify the startup page options if "Open the
    following URLs" is set by a policy.
    """
    policy = {
      'RestoreOnStartup': 4,
      'RestoreOnStartupURLs': ['http://chromium.org']
    }
    self.SetUserPolicy(policy)
    # Verify startup option
    self.assertEquals(4, self.GetPrefsInfo().Prefs(pyauto.kRestoreOnStartup))
    self.assertRaises(
        pyauto.JSONInterfaceError,
        lambda: self.SetPrefs(pyauto.kRestoreOnStartup, 1))

  def testStartupOptionsHomepage(self):
    """Verify that user cannot modify the startup page options if the
    deprecated "Open the homepage" option is set by a policy.
    """
    policy = {
      'RestoreOnStartup': 0,
      'HomepageLocation': 'http://chromium.org',
      'HomepageIsNewTabPage': False,
    }
    self.SetUserPolicy(policy)
    self.assertEquals(4, self.GetPrefsInfo().Prefs(pyauto.kRestoreOnStartup))
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
    self.SetUserPolicy(policy)
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

  def testApplicationLocaleValue(self):
    """Verify that Chrome can be launched only in a specific locale."""
    if self.IsWin():
      policy = {'ApplicationLocaleValue': 'hi'}
      self.SetUserPolicy(policy)
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
    self.SetUserPolicy(policy)
    self.SetPrefs(pyauto.kDevToolsOpenDocked, False)
    self.ApplyAccelerator(pyauto.IDC_DEV_TOOLS)
    self.assertEquals(1, len(self.GetBrowserInfo()['windows']),
                      msg='Devtools window launched.')
    policy = {'DeveloperToolsDisabled': False}
    self.SetUserPolicy(policy)
    self.ApplyAccelerator(pyauto.IDC_DEV_TOOLS)
    self.assertEquals(2, len(self.GetBrowserInfo()['windows']),
                      msg='Devtools window not launched.')

  def testDisableSPDY(self):
    """Verify that SPDY is disabled."""
    policy = {'DisableSpdy': True}
    self.SetUserPolicy(policy)
    self.NavigateToURL('chrome://net-internals/#spdy')
    self.assertEquals(0, self.FindInPage('SPDY Enabled: true')['match_count'])
    self.assertEquals(
        1,
        self.FindInPage('SPDY Enabled: false', tab_index=0)['match_count'],
        msg='SPDY is not disabled.')
    policy = {'DisableSpdy': False}
    self.SetUserPolicy(policy)
    self.GetBrowserWindow(0).GetTab(0).Reload()
    self.assertEquals(
        1,
        self.FindInPage('SPDY Enabled: true', tab_index=0)['match_count'],
        msg='SPDY is not disabled.')

  def testDisabledPlugins(self):
    """Verify that disabled plugins cannot be enabled."""
    policy = {'DisabledPlugins': ['Shockwave Flash']}
    self.SetUserPolicy(policy)
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
    self.SetUserPolicy(policy)
    for plugin in self.GetPluginsInfo().Plugins():
      if 'Chrome PDF Viewer' in plugin['name']:
        self.EnablePlugin(plugin['path'])
        return

  def testEnabledPlugins(self):
    """Verify that enabled plugins cannot be disabled."""
    policy = {'EnabledPlugins': ['Shockwave Flash']}
    self.SetUserPolicy(policy)
    for plugin in self.GetPluginsInfo().Plugins():
      if 'Flash' in plugin['name']:
        self.assertRaises(pyauto.JSONInterfaceError,
                          lambda: self.DisablePlugin(plugin['path']))
        return
    logging.debug('Flash is not present.')

  def testAlwaysAuthorizePlugins(self):
    """Verify plugins are always allowed to run when policy is set."""
    policy = {'AlwaysAuthorizePlugins': True}
    self.SetUserPolicy(policy)
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
    self.SetUserPolicy(policy)
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
    self.SetUserPolicy(policy)
    self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
    self.assertEquals(1, self.GetBrowserWindowCount())
    policy = {'IncognitoEnabled': True}
    self.SetUserPolicy(policy)
    self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
    self.assertEquals(2, self.GetBrowserWindowCount())

  def testSavingBrowserHistoryDisabled(self):
    """Verify that browsing history is not being saved."""
    policy = {'SavingBrowserHistoryDisabled': True}
    self.SetUserPolicy(policy)
    url = self.GetFileURLForPath(os.path.join(self.DataDir(), 'empty.html'))
    self.NavigateToURL(url)
    self.assertFalse(self.GetHistoryInfo().History(),
                     msg='History is being saved.')
    policy = {'SavingBrowserHistoryDisabled': False}
    self.SetUserPolicy(policy)
    self.NavigateToURL(url)
    self.assertTrue(self.GetHistoryInfo().History(),
                     msg='History not is being saved.')

  def testTranslateEnabled(self):
    """Verify that translate happens if policy enables it."""
    policy = {'TranslateEnabled': True}
    self.SetUserPolicy(policy)
    self.assertTrue(self.GetPrefsInfo().Prefs(pyauto.kEnableTranslate))
    url = self.GetFileURLForDataPath('translate', 'es', 'google.html')
    self.NavigateToURL(url)
    self.assertTrue(self.WaitForInfobarCount(1))
    translate_info = self.GetTranslateInfo()
    self.assertEqual('es', translate_info['original_language'])
    self.assertFalse(translate_info['page_translated'])
    self.assertTrue(translate_info['can_translate_page'])
    self.assertTrue('translate_bar' in translate_info)
    self.assertFalse(self._GetPrefIsManagedError(pyauto.kEnableTranslate, True))
    policy = {'TranslateEnabled': False}
    self.SetUserPolicy(policy)
    self.assertFalse(self.GetPrefsInfo().Prefs(pyauto.kEnableTranslate))
    self.NavigateToURL(url)
    self.assertFalse(self.WaitForInfobarCount(1))
    self.assertFalse(self._GetPrefIsManagedError(pyauto.kEnableTranslate,
                                                 False))

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
    self.SetUserPolicy(policy)
    self.assertFalse(
        self._GetPrefIsManagedError(pyauto.kDefaultSearchProviderEnabled, True))
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
    self.SetUserPolicy(policy)
    self.assertFalse(
        self._GetPrefIsManagedError(pyauto.kDefaultSearchProviderEnabled,
                                    False))
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
    self.SetUserPolicy({
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
    self.SetUserPolicy({
      'ExtensionInstallBlacklist': ['*']
    })
    self._AttemptExtensionInstallThatShouldFail('good.crx')
    self._AttemptExtensionInstallThatShouldFail('adblock.crx')

  def testExtensionInstallWithGlobalBlacklistAndWhitelistedExtension(self):
    """Verify whitelisted extension is installed when all are blacklisted."""
    # TODO(krisr): Remove this when crosbug.com/27227 is fixed.
    self._RemoveTestingExtensions()
    # Block installs of all extensions, but whitelist adblock.crx
    self.SetUserPolicy({
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
    self.SetUserPolicy({
      'ExtensionInstallForcelist': [self._SCREEN_CAPTURE_CRX_ID],
    })
    # Give the system 30 seconds to go get this extension.  We are not sure how
    # long it will take the policy to take affect and download the extension.
    self.assertTrue(self.WaitUntil(lambda:
      self._CheckForExtensionByID(self._SCREEN_CAPTURE_CRX_ID),
      expect_retval=True),
      msg='The force install extension was never installed.')

  def testClearSiteDataOnExit(self):
    """Verify the ClearSiteDataOnExit policy is taking effect.

    Install a cookie and make sure the cookie gets removed on browser restart
    when the policy is set.
    """
    cookie_url = 'http://example.com'
    cookie_val = 'ponies=unicorns'
    self.SetCookie(pyauto.GURL(cookie_url),
                   cookie_val + ';expires=Wed Jan 01 3000 00:00:00 GMT')

    # Cookie should be kept over restarts.
    self.RestartBrowser(clear_profile=False)
    self.assertEqual(
        cookie_val, self.GetCookie(pyauto.GURL(cookie_url)),
        msg='Cookie on ' + cookie_url + ' does not match ' + cookie_val + '.');

    # With the policy set, the cookie should be gone after a restart.
    self.SetUserPolicy({
      'ClearSiteDataOnExit': True
    })
    self.RestartBrowser(clear_profile=False)
    self.assertFalse(
        self.GetCookie(pyauto.GURL(cookie_url)),
        msg='Cookie present on ' + cookie_url + '.');


if __name__ == '__main__':
  pyauto_functional.Main()
