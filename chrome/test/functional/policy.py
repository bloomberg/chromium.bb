#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

import pyauto_functional  # must come before pyauto.
import policy_base
import pyauto
from pyauto_errors import JSONInterfaceError


class PolicyTest(policy_base.PolicyTestBase):
  """Tests that the effects of policies are being enforced as expected."""

  def IsBlocked(self, url):
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

  def IsJavascriptEnabled(self):
    """Returns true if Javascript is enabled, false otherwise."""
    try:
      ret = self.ExecuteJavascript('domAutomationController.send("done");')
      return ret == 'done'
    except JSONInterfaceError as e:
      if 'Javascript execution was blocked' == str(e):
        logging.debug('The previous failure was expected')
        return False
      else:
        raise e

  def IsWebGLEnabled(self):
    """Returns true if WebGL is enabled, false otherwise."""
    ret = self.GetDOMValue("""
        document.createElement('canvas').
            getContext('experimental-webgl') ? 'ok' : ''
    """)
    return ret == 'ok'

  def RestartRenderer(self, windex=0):
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

    self.assertTrue(self.IsBlocked('http://news.google.com/'))
    self.assertFalse(self.IsBlocked('http://www.google.com/'))
    self.assertFalse(self.IsBlocked('http://google.com/'))

    self.assertTrue(self.IsBlocked('http://chromium.org/'))
    self.assertTrue(self.IsBlocked('http://www.chromium.org/'))
    self.assertFalse(self.IsBlocked('http://dev.chromium.org/'))
    self.assertFalse(self.IsBlocked('http://chromium.org/chromium-os/testing'))

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
    self.assertTrue(self.IsJavascriptEnabled())
    self.assertTrue(self.IsMenuCommandEnabled(pyauto.IDC_DEV_TOOLS))
    self.assertTrue(self.IsMenuCommandEnabled(pyauto.IDC_DEV_TOOLS_CONSOLE))

    policy['DeveloperToolsDisabled'] = True
    self.SetPolicies(policy)
    self.assertTrue(self.IsJavascriptEnabled())
    self.assertFalse(self.IsMenuCommandEnabled(pyauto.IDC_DEV_TOOLS))
    self.assertFalse(self.IsMenuCommandEnabled(pyauto.IDC_DEV_TOOLS_CONSOLE))

    policy['DeveloperToolsDisabled'] = False
    self.SetPolicies(policy)
    self.assertTrue(self.IsJavascriptEnabled())
    self.assertTrue(self.IsMenuCommandEnabled(pyauto.IDC_DEV_TOOLS))
    self.assertTrue(self.IsMenuCommandEnabled(pyauto.IDC_DEV_TOOLS_CONSOLE))

    # The Developer Tools still work when javascript is disabled.
    policy['JavascriptEnabled'] = False
    self.SetPolicies(policy)
    self.NavigateToURL('about:blank')
    self.assertFalse(self.IsJavascriptEnabled())
    self.assertTrue(self.IsMenuCommandEnabled(pyauto.IDC_DEV_TOOLS))
    self.assertTrue(self.IsMenuCommandEnabled(pyauto.IDC_DEV_TOOLS_CONSOLE))
    # Javascript is always enabled for internal Chrome pages.
    self.NavigateToURL('chrome://settings')
    self.assertTrue(self.IsJavascriptEnabled())

    # The Developer Tools can be explicitly disabled.
    policy['DeveloperToolsDisabled'] = True
    self.SetPolicies(policy)
    self.NavigateToURL('about:blank')
    self.assertFalse(self.IsJavascriptEnabled())
    self.assertFalse(self.IsMenuCommandEnabled(pyauto.IDC_DEV_TOOLS))
    self.assertFalse(self.IsMenuCommandEnabled(pyauto.IDC_DEV_TOOLS_CONSOLE))

    # Javascript can also be disabled with content settings policies.
    policy = {
      'DefaultJavaScriptSetting': 2,
    }
    self.SetPolicies(policy)
    self.NavigateToURL('about:blank')
    self.assertFalse(self.IsJavascriptEnabled())
    self.assertTrue(self.IsMenuCommandEnabled(pyauto.IDC_DEV_TOOLS))
    self.assertTrue(self.IsMenuCommandEnabled(pyauto.IDC_DEV_TOOLS_CONSOLE))

    # The content setting overrides JavascriptEnabled.
    policy = {
      'DefaultJavaScriptSetting': 1,
      'JavascriptEnabled': False,
    }
    self.SetPolicies(policy)
    self.NavigateToURL('about:blank')
    self.assertTrue(self.IsJavascriptEnabled())

  def testDisable3DAPIs(self):
    """Tests the policy that disables the 3D APIs."""
    self.assertFalse(self.GetPrefsInfo().Prefs(pyauto.kDisable3DAPIs))
    self.assertTrue(self.IsWebGLEnabled())

    self.SetPolicies({
        'Disable3DAPIs': True
    })
    self.assertTrue(self.GetPrefsInfo().Prefs(pyauto.kDisable3DAPIs))
    # The Disable3DAPIs policy only applies updated values to new renderers.
    self.RestartRenderer()
    self.assertFalse(self.IsWebGLEnabled())


if __name__ == '__main__':
  pyauto_functional.Main()
