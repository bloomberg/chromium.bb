#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os

import pyauto_functional  # Must be imported before pyauto
import pyauto
import test_utils

class SpecialTabsTest(pyauto.PyUITest):
  """TestCase for Special Tabs like about:version, chrome://history, etc."""

  @staticmethod
  def GetSpecialAcceleratorTabs():
    """Get a dict of accelerators and corresponding tab titles."""
    ret = {
        pyauto.IDC_SHOW_HISTORY: 'History',
        pyauto.IDC_MANAGE_EXTENSIONS: 'Preferences - Extensions',
        pyauto.IDC_SHOW_DOWNLOADS: 'Downloads',
    }
    if pyauto.PyUITest.IsWin():
      ret[pyauto.IDC_MANAGE_EXTENSIONS] = 'Options - Extensions'
    elif pyauto.PyUITest.IsChromeOS():
      ret[pyauto.IDC_MANAGE_EXTENSIONS] = 'Settings - Extensions'
    return ret

  special_url_redirects = {
   'about:': 'chrome://version',
   'about:about': 'chrome://about',
   'about:appcache-internals': 'chrome://appcache-internals',
   'about:credits': 'chrome://credits',
   'about:dns': 'chrome://dns',
   'about:histograms': 'chrome://histograms',
   'about:plugins': 'chrome://plugins',
   'about:sync': 'chrome://sync-internals',
   'about:sync-internals': 'chrome://sync-internals',
   'about:version': 'chrome://version',
  }

  special_url_tabs = {
    'chrome://about': { 'title': 'Chrome URLs' },
    'chrome://appcache-internals': { 'title': 'AppCache Internals' },
    'chrome://blob-internals': { 'title': 'Blob Storage Internals' },
    'chrome://bugreport': {},
    'chrome://bugreport/#0': { 'title': 'Feedback' },
    'chrome://chrome-urls': { 'title': 'Chrome URLs' },
    'chrome://crashes': { 'title': 'Crashes' },
    'chrome://credits': { 'title': 'Credits', 'CSP': False },
    'chrome://downloads': { 'title': 'Downloads' },
    'chrome://dns': { 'title': 'About DNS' },
    'chrome://settings/extensions': { 'title': 'Preferences - Extensions' },
    'chrome://flags': {},
    'chrome://flash': {},
    'chrome://gpu-internals': {},
    'chrome://histograms': { 'title': 'About Histograms' },
    'chrome://history': { 'title': 'History' },
    'chrome://media-internals': { 'title': 'Media Internals' },
    'chrome://memory-redirect': { 'title': 'About Memory' },
    'chrome://net-internals': {},
    'chrome://net-internals/help.html': {},
    'chrome://newtab': { 'title': 'New Tab', 'CSP': False },
    'chrome://plugins': { 'title': 'Plug-ins' },
    'chrome://sessions': { 'title': 'Sessions' },
    'chrome://settings': { 'title': 'Preferences - Basics' },
    'chrome://stats': {},
    'chrome://sync': { 'title': 'Sync Internals' },
    'chrome://sync-internals': { 'title': 'Sync Internals' },
    'chrome://tasks': { 'title': 'Task Manager - Chromium' },
    'chrome://terms': {},
    'chrome://version': { 'title': 'About Version' },
    'chrome://view-http-cache': {},
    'chrome://workers': { 'title': 'Workers' },
  }
  broken_special_url_tabs = {
    # crashed under debug when invoked from location bar (bug 88223).
    'chrome://devtools': { 'CSP': False },

    # returns "not available" despite having an URL constant.
    'chrome://dialog': { 'CSP': False },

    # separate window on mac, PC untested, not implemented elsewhere.
    'chrome://ipc': { 'CSP': False },

    # race against redirects via meta-refresh.
    'chrome://memory': { 'CSP': False },
  }

  chromeos_special_url_tabs = {
    'chrome://active-downloads': { 'title': 'Downloads', 'CSP': False },
    'chrome://choose-mobile-network': { 'title': 'undefined', 'CSP': False },
    'chrome://imageburner': { 'title':'Create a Recovery Media', 'CSP': False },
    'chrome://keyboardoverlay': { 'title': 'Keyboard Overlay', 'CSP': False },
    'chrome://login': { 'CSP': False },
    'chrome://network': { 'title': 'About Network' },
    'chrome://oobe': { 'title': 'undefined', 'CSP': False },
    'chrome://os-credits': { 'title': 'Credits', 'CSP': False },
    'chrome://proxy-settings': { 'CSP': False },
    'chrome://register': { 'CSP': False },
    'chrome://sim-unlock': { 'title': 'Enter SIM Card PIN', 'CSP': False },
    'chrome://system': { 'title': 'About System', 'CSP': False },

    # OVERRIDE - usually a warning page without CSP (so far).
    'chrome://flags': { 'CSP': False },

    # OVERRIDE - title and page different on CrOS
    'chrome://settings/about': { 'title': 'Settings - About' },
    'chrome://settings/accounts': { 'title': 'Settings - Users' },
    'chrome://settings/advanced': { 'title': 'Settings - Under the Hood' },
    'chrome://settings/autofill': { 'title': 'Settings - Autofill Settings' },
    'chrome://settings/browser': { 'title': 'Settings - Basics' },
    'chrome://settings/clearBrowserData':
      { 'title': 'Settings - Clear Browsing Data' },
    'chrome://settings/content': { 'title': 'Settings - Content Settings' },
    'chrome://settings/extensions': { 'title': 'Settings - Extensions' },
    'chrome://settings/internet': { 'title': 'Settings - Internet' },
    'chrome://settings/languages':
      { 'title': 'Settings - Languages and Input' },
    'chrome://settings/passwords': { 'title': 'Settings - Passwords' },
    'chrome://settings/personal': { 'title': 'Settings - Personal Stuff' },
    'chrome://settings/proxy': { 'title': 'Proxy' },
    'chrome://settings/system': { 'title': 'Settings - System' },
  }
  broken_chromeos_special_url_tabs = {
    # returns "not available" page on chromeos=1 linux but has an URL constant.
    'chrome://activationmessage': { 'CSP': False },
    'chrome://cloudprintresources': { 'CSP': False },
    'chrome://cloudprintsetup': { 'CSP': False },
    'chrome://collected-cookies': { 'CSP': False },
    'chrome://constrained-test': { 'CSP': False },
    'chrome://enterprise-enrollment': { 'CSP': False },
    'chrome://http-auth': { 'CSP': False },
    'chrome://login-container': { 'CSP': False },
    'chrome://media-player': { 'CSP': False },
    'chrome://screenshots': { 'CSP': False },
    'chrome://slideshow': { 'CSP': False },
    'chrome://syncresources': { 'CSP': False },
    'chrome://theme': { 'CSP': False },

    # crashes on chromeos=1 on linux, possibly missing real CrOS features.
    'chrome://cryptohome': { 'CSP': False},
    'chrome://mobilesetup': { 'CSP': False },
    'chrome://print': { 'CSP': False },
    'chrome://tasks': {},
  }

  linux_special_url_tabs = {
    'chrome://linux-proxy-config': { 'title': 'Proxy Configuration Help' },
    'chrome://tcmalloc': { 'title': 'About tcmalloc' },
    'chrome://sandbox': { 'title': 'Sandbox Status' },
  }
  broken_linux_special_url_tabs = {}

  mac_special_url_tabs = {}
  broken_mac_special_url_tabs = {}

  win_special_url_tabs = {
    'chrome://conflicts': {},

    # OVERRIDE - different title for page.
    'chrome://settings': { 'title': 'Options - Basics' },
    'chrome://settings/extensions': { 'title': 'Options - Extensions' },
  }
  broken_win_special_url_tabs = {
    # Sync on windows badly broken at the moment.
    'chrome://sync': {},
  }

  google_special_url_tabs = {
    # OVERRIDE - different title for Google Chrome vs. Chromium.
    'chrome://terms': {
      'title': 'Google Chrome Terms of Service',
    },
    'chrome://tasks': { 'title': 'Task Manager - Google Chrome' },
  }
  broken_google_special_url_tabs = {}

  google_chromeos_special_url_tabs = {
    # OVERRIDE - different title for Google Chrome OS vs. Chromium OS.
    'chrome://terms': {
      'title': 'Google Chrome OS Terms',
    },
  }
  broken_google_chromeos_special_url_tabs = {}

  google_win_special_url_tabs = {}
  broken_google_win_special_url_tabs = {}

  google_mac_special_url_tabs = {}
  broken_google_mac_special_url_tabs = {}

  google_linux_special_url_tabs = {}
  broken_google_linux_special_url_tabs = {}

  def _VerifyAppCacheInternals(self):
    """Confirm about:appcache-internals contains expected content for Caches.
       Also confirms that the about page populates Application Caches."""
    # Navigate to html page to activate DNS prefetching.
    self.NavigateToURL('http://www.webkit.org/demos/sticky-notes/index.html')
    self.GetBrowserWindow(0).GetTab(0).GoBack()
    test_utils.StringContentCheck(
        self, self.GetTabContents(),
        ['Manifest',
         'http://www.webkit.org/demos/sticky-notes/StickyNotes.manifest'],
        [])

  def _VerifyAboutDNS(self):
    """Confirm about:dns contains expected content related to DNS info.
       Also confirms that prefetching DNS records propogate."""
    # Navigate to a page to activate DNS prefetching.
    self.NavigateToURL('http://www.google.com')
    self.GetBrowserWindow(0).GetTab(0).GoBack()
    test_utils.StringContentCheck(self, self.GetTabContents(),
                                  ['Host name', 'How long ago', 'Motivation'],
                                  [])

  def _GetPlatformSpecialURLTabs(self):
    tabs = self.special_url_tabs.copy()
    broken_tabs = self.broken_special_url_tabs.copy()
    if self.IsChromeOS():
      tabs.update(self.chromeos_special_url_tabs)
      broken_tabs.update(self.broken_chromeos_special_url_tabs)
    elif self.IsLinux():
      tabs.update(self.linux_special_url_tabs)
      broken_tabs.update(self.broken_linux_special_url_tabs)
    elif self.IsMac():
      tabs.update(self.mac_special_url_tabs)
      broken_tabs.update(self.broken_mac_special_url_tabs)
    elif self.IsWin():
      tabs.update(self.win_special_url_tabs)
      broken_tabs.update(self.broken_win_special_url_tabs)
    for key, value in broken_tabs.iteritems():
      if key in tabs:
       del tabs[key]
    broken_tabs = {}
    if self.GetBrowserInfo()['properties']['branding'] == 'Google Chrome':
      tabs.update(self.google_special_url_tabs)
      broken_tabs.update(self.broken_google_special_url_tabs)
      if self.IsChromeOS():
        tabs.update(self.google_chromeos_special_url_tabs)
        broken_tabs.update(self.broken_google_chromeos_special_url_tabs)
      elif self.IsLinux():
        tabs.update(self.google_linux_special_url_tabs)
        broken_tabs.update(self.broken_google_linux_special_url_tabs)
      elif self.IsMac():
        tabs.update(self.google_mac_special_url_tabs)
        broken_tabs.update(self.broken_google_mac_special_url_tabs)
      elif self.IsWin():
        tabs.update(self.google_win_special_url_tabs)
        broken_tabs.update(self.broken_google_win_special_url_tabs)
      for key, value in broken_tabs.iteritems():
        if key in tabs:
         del tabs[key]
    return tabs

  def testSpecialURLRedirects(self):
    """Test that older about: URLs are implemented by newer chrome:// URLs.
       The location bar may not get updated in all cases, so checking the
       tab URL is misleading, instead check for the same contents as the
       chrome:// page."""
    tabs = self._GetPlatformSpecialURLTabs()
    for url, redirect in self.special_url_redirects.iteritems():
      if redirect in tabs:
        logging.debug('Testing redirect from %s to %s.' % (url, redirect))
        self.NavigateToURL(url)
        self.assertEqual(self.special_url_tabs[redirect]['title'],
                         self.GetActiveTabTitle())

  def testSpecialURLTabs(self):
    """Test special tabs created by URLs like chrome://downloads,
       chrome://settings/extensionSettings, chrome://history etc.
       Also ensures they specify content-security-policy and not inline
       scripts for those pages that are expected to do so."""
    tabs = self._GetPlatformSpecialURLTabs()
    for url, properties in tabs.iteritems():
      logging.debug('Testing URL %s.' % url)
      self.NavigateToURL(url)
      expected_title = 'title' in properties and properties['title'] or url
      actual_title = self.GetActiveTabTitle()
      logging.debug('  %s title was %s (%s)' %
                    (url, actual_title, expected_title == actual_title))
      self.assertEqual(expected_title, actual_title)
      include_list = []
      exclude_list = []
      no_csp = 'CSP' in properties and not properties['CSP']
      if no_csp:
        exclude_list.extend(['X-WebKit-CSP'])
      else:
        exclude_list.extend(['<script>', 'onclick=', 'onload=',
                             'onchange=', 'onsubmit=', 'javascript:'])
      if 'includes' in properties:
        include_list.extend(properties['includes'])
      if 'excludes' in properties:
        exclude_list.extend(properties['exlcudes'])
      test_utils.StringContentCheck(self, self.GetTabContents(),
                                    include_list, exclude_list)
      result = self.ExecuteJavascript("""
          var r = 'blocked';
          var f = 'executed';
          var s = document.createElement('script');
          s.textContent = 'r = f';
          document.body.appendChild(s);
          window.domAutomationController.send(r);
        """)
      logging.debug('has csp %s, result %s.' % (not no_csp, result))
      if no_csp:
        self.assertEqual(result, 'executed',
                         msg='Got %s for %s' % (result, url))
      else:
        self.assertEqual(result, 'blocked');

  def testAboutAppCacheTab(self):
    """Test App Cache tab to confirm about page populates caches."""
    self.NavigateToURL('about:appcache-internals')
    self._VerifyAppCacheInternals()
    self.assertEqual('AppCache Internals', self.GetActiveTabTitle())

  def testAboutDNSTab(self):
    """Test DNS tab to confirm DNS about page propogates records."""
    self.NavigateToURL('about:dns')
    self._VerifyAboutDNS()
    self.assertEqual('About DNS', self.GetActiveTabTitle())

  def testSpecialAcceratorTabs(self):
    """Test special tabs created by acclerators like IDC_SHOW_HISTORY,
       IDC_SHOW_DOWNLOADS."""
    for accel, title in self.GetSpecialAcceleratorTabs().iteritems():
      self.RunCommand(accel)
      self.assertTrue(self.WaitUntil(
            self.GetActiveTabTitle, expect_retval=title),
          msg='Expected "%s"' % title)


if __name__ == '__main__':
  pyauto_functional.Main()
