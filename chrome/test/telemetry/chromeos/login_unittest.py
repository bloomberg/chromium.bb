# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import json
import logging
import os
import unittest

from telemetry.core import browser_finder
from telemetry.core import exceptions
from telemetry.core import extension_to_load
from telemetry.core import util
from telemetry.core.backends.chrome import cros_interface
from telemetry.unittest import options_for_unittests

class CrOSAutoTest(unittest.TestCase):
  def setUp(self):
    options = options_for_unittests.GetCopy()
    self._cri = cros_interface.CrOSInterface(options.cros_remote,
                                             options.cros_ssh_identity)
    self._is_guest = options.browser_type == 'cros-chrome-guest'
    self._username = '' if self._is_guest else options.browser_options.username
    self._password = options.browser_options.password

  def _IsCryptohomeMounted(self):
    """Returns True if cryptohome is mounted"""
    cryptohomeJSON, _ = self._cri.RunCmdOnDevice(['/usr/sbin/cryptohome',
                                                 '--action=status'])
    cryptohomeStatus = json.loads(cryptohomeJSON)
    return (cryptohomeStatus['mounts'] and
            cryptohomeStatus['mounts'][0]['mounted'])

  def _CreateBrowser(self, autotest_ext=False, auto_login=True):
    """Finds and creates a browser for tests. if autotest_ext is True,
    also loads the autotest extension"""
    options = options_for_unittests.GetCopy()

    if autotest_ext:
      extension_path = os.path.join(os.path.dirname(__file__), 'autotest_ext')
      self._load_extension = extension_to_load.ExtensionToLoad(
          path=extension_path,
          browser_type=options.browser_type,
          is_component=True)
      options.extensions_to_load = [self._load_extension]

    browser_to_create = browser_finder.FindBrowser(options)
    self.assertTrue(browser_to_create)
    options.browser_options.create_browser_with_oobe = True
    options.browser_options.auto_login = auto_login
    b = browser_to_create.Create()
    b.Start()
    return b

  def _GetAutotestExtension(self, browser):
    """Returns the autotest extension instance"""
    extension = browser.extensions[self._load_extension]
    self.assertTrue(extension)
    return extension

  def _GetLoginStatus(self, browser):
      extension = self._GetAutotestExtension(browser)
      self.assertTrue(extension.EvaluateJavaScript(
          "typeof('chrome.autotestPrivate') != 'undefined'"))
      extension.ExecuteJavaScript('''
        window.__login_status = null;
        chrome.autotestPrivate.loginStatus(function(s) {
          window.__login_status = s;
        });
      ''')
      return util.WaitFor(
          lambda: extension.EvaluateJavaScript('window.__login_status'), 10)

  def testCryptohomeMounted(self):
    """Verifies cryptohome mount status for regular and guest user and when
    logged out"""
    with self._CreateBrowser() as b:
      self.assertEquals(1, len(b.tabs))
      self.assertTrue(b.tabs[0].url)
      self.assertTrue(self._IsCryptohomeMounted())

      chronos_fs = self._cri.FilesystemMountedAt('/home/chronos/user')
      self.assertTrue(chronos_fs)
      if self._is_guest:
        self.assertEquals(chronos_fs, 'guestfs')
      else:
        home, _ = self._cri.RunCmdOnDevice(['/usr/sbin/cryptohome-path',
                                            'user', self._username])
        self.assertEquals(self._cri.FilesystemMountedAt(home.rstrip()),
                          chronos_fs)

    self.assertFalse(self._IsCryptohomeMounted())
    self.assertEquals(self._cri.FilesystemMountedAt('/home/chronos/user'),
                      '/dev/mapper/encstateful')

  def testLoginStatus(self):
    """Tests autotestPrivate.loginStatus"""
    with self._CreateBrowser(autotest_ext=True) as b:
      login_status = self._GetLoginStatus(b)
      self.assertEquals(type(login_status), dict)

      self.assertEquals(not self._is_guest, login_status['isRegularUser'])
      self.assertEquals(self._is_guest, login_status['isGuest'])
      self.assertEquals(login_status['email'], self._username)
      self.assertFalse(login_status['isScreenLocked'])

  def _IsScreenLocked(self, browser):
    return self._GetLoginStatus(browser)['isScreenLocked']

  def _LockScreen(self, browser):
      self.assertFalse(self._IsScreenLocked(browser))

      extension = self._GetAutotestExtension(browser)
      self.assertTrue(extension.EvaluateJavaScript(
          "typeof chrome.autotestPrivate.lockScreen == 'function'"))
      logging.info('Locking screen')
      extension.ExecuteJavaScript('chrome.autotestPrivate.lockScreen();')

      logging.info('Waiting for the lock screen')
      def ScreenLocked():
        return (browser.oobe and
            browser.oobe.EvaluateJavaScript("typeof Oobe == 'function'") and
            browser.oobe.EvaluateJavaScript(
            "typeof Oobe.authenticateForTesting == 'function'"))
      util.WaitFor(ScreenLocked, 10)
      self.assertTrue(self._IsScreenLocked(browser))

  def _AttemptUnlockBadPassword(self, browser):
      logging.info('Trying a bad password')
      def ErrorBubbleVisible():
        return not browser.oobe.EvaluateJavaScript('''
            document.getElementById('bubble').hidden
        ''')
      self.assertFalse(ErrorBubbleVisible())
      browser.oobe.ExecuteJavaScript('''
          Oobe.authenticateForTesting('%s', 'bad');
      ''' % self._username)
      util.WaitFor(ErrorBubbleVisible, 10)
      self.assertTrue(self._IsScreenLocked(browser))

  def _UnlockScreen(self, browser):
      logging.info('Unlocking')
      browser.oobe.ExecuteJavaScript('''
          Oobe.authenticateForTesting('%s', '%s');
      ''' % (self._username, self._password))
      util.WaitFor(lambda: not browser.oobe, 10)
      self.assertFalse(self._IsScreenLocked(browser))

  def testScreenLock(self):
    """Tests autotestPrivate.screenLock"""
    with self._CreateBrowser(autotest_ext=True) as browser:
      self._LockScreen(browser)
      self._AttemptUnlockBadPassword(browser)
      self._UnlockScreen(browser)

  def testLogout(self):
    """Tests autotestPrivate.logout"""
    with self._CreateBrowser(autotest_ext=True) as b:
      extension = self._GetAutotestExtension(b)
      try:
        extension.ExecuteJavaScript('chrome.autotestPrivate.logout();')
      except (exceptions.BrowserConnectionGoneException,
              exceptions.BrowserGoneException):
        pass
      util.WaitFor(lambda: not self._IsCryptohomeMounted(), 20)

  def _SwitchRegion(self, region):
    self._cri.RunCmdOnDevice(['stop', 'ui'])

    # Change VPD (requires RW-enabled firmware).
    # To save time, region and initial_timezone are not set.
    vpd = {'initial_locale': region.language_code,
           'keyboard_layout': region.keyboard}

    for (key, value) in vpd.items():
      self._cri.RunCmdOnDevice(['vpd', '-s', '"%s"="%s"' % (key, value)])

    # Remove cached files to clear initial locale info and force regeneration.
    self._cri.RunCmdOnDevice(['rm', '/home/chronos/Local\ State'])
    self._cri.RunCmdOnDevice(['rm', '/home/chronos/.oobe_completed'])
    self._cri.RunCmdOnDevice(['dump_vpd_log', '--force'])

    self._cri.RunCmdOnDevice(['start', 'ui'])

  def _OobeHasOption(self, browser, selectId, value):
    hasOptionJs = '''
      // Check that the option is present, and selected if it is the default.
      (function hasOption(selectId, value, isDefault) {
        var options = document.getElementById(selectId).options;
        for (var i = 0; i < options.length; i++) {
          if (options[i].value == value) {
            // The option is present. Make sure it's selected if necessary.
            return !isDefault || options.selectedIndex == i;
          }
        }
        return false;
      })("%s", "%s", %s);
    '''
    return browser.oobe.EvaluateJavaScript(
        hasOptionJs % (selectId, value, 'true'))

  def _ResolveLanguage(self, locale):
    # If the locale matches a language but not the country, fall back to
    # an existing locale. See ui/base/l10n/l10n_util.cc.
    lang, _, region = map(str.lower, locale.partition('-'))
    if not region:
      return ""

    # Map from other countries to a localized country
    if lang == 'es' and region == 'es':
      return 'es-419'
    if lang == 'zh':
      if region in ('hk', 'mo'):
        return 'zh-TW'
      return 'zh-CN'
    if lang == 'en':
      if region in ('au', 'ca', 'nz', 'za'):
        return 'en-GB'
      return 'en-US'

    # No mapping found
    return ""

  def testOobeLocalization(self):
    """Tests different region configurations at OOBE"""
    # Save the original device localization settings.
    # To save time, only read initial_locale and keyboard_layout.
    initial_region = self.Region('', '', '', '', '')
    initial_region.language_code, _ = self._cri.RunCmdOnDevice(
        ['vpd', '-g', 'initial_locale'])
    initial_region.keyboard, _ = self._cri.RunCmdOnDevice(
        ['vpd', '-g', 'keyboard_layout'])

    for region in self.REGIONS_LIST:
      self._SwitchRegion(region)
      with self._CreateBrowser(auto_login=False) as browser:
        # Ensure the dropdown lists have been created.
        util.WaitFor(lambda: browser.oobe.EvaluateJavaScript(
                     'document.getElementById("language-select") != null'),
                     10)

        # Find the language, or an acceptable fallback value.
        languageFound = self._OobeHasOption(browser,
                                            'language-select',
                                            region.language_code)
        if not languageFound:
          fallback = self._ResolveLanguage(region.language_code)
          self.assertTrue(fallback and
                          self._OobeHasOption(browser,
                                              'language-select',
                                              fallback))

        # Find the keyboard layout.
        self.assertTrue(self._OobeHasOption(
            browser, 'keyboard-select', region.keyboard))

    # Test is finished. Restore original region settings.
    self._SwitchRegion(initial_region)

  # The Region class and region list will be available in regions.py.
  class Region(object):
    def __init__(self, region_code, keyboard, time_zone, language_code,
                 keyboard_mechanical_layout, description=None, notes=None):
      self.region_code = region_code
      self.keyboard = keyboard
      self.time_zone = time_zone
      self.language_code = language_code
      self.keyboard_mechanical_layout = keyboard_mechanical_layout
      self.description = description or region_code
      self.notes = notes

  class Enum(frozenset):
    def __getattr__(self, name):
      if name in self:
        return name
      raise AttributeError

  KeyboardMechanicalLayout = Enum(['ANSI', 'ISO', 'JIS', 'ABNT2'])
  _KML = KeyboardMechanicalLayout
  REGIONS_LIST = [
    Region('au', 'xkb:us::eng', 'Australia/Sydney', 'en-AU', _KML.ANSI,
           'Australia'),
    Region('ca.ansi', 'xkb:us::eng', 'America/Toronto', 'en-CA', _KML.ANSI,
           'Canada (US keyboard)',
           'Canada with US (ANSI) keyboard; see http://goto/cros-canada'),
    Region('ca.fr', 'xkb:ca::fra', 'America/Toronto', 'fr-CA', _KML.ISO,
           'Canada (French keyboard)',
           ('Canadian French (ISO) keyboard. The most common configuration for '
            'Canadian French SKUs.  See http://goto/cros-canada')),
    Region('ca.hybrid', 'xkb:ca:eng:eng', 'America/Toronto', 'en-CA', _KML.ISO,
           'Canada (hybrid)',
           ('Canada with hybrid xkb:ca:eng:eng + xkb:ca::fra keyboard (ISO), '
            'defaulting to English language and keyboard.  Used only if there '
            'needs to be a single SKU for all of Canada.  See '
            'http://goto/cros-canada')),
    Region('ca.multix', 'xkb:ca:multix:fra', 'America/Toronto', 'fr-CA',
           _KML.ISO, 'Canada (multilingual)',
           ("Canadian Multilingual keyboard; you probably don't want this. See "
            "http://goto/cros-canada")),
    Region('de', 'xkb:de::ger', 'Europe/Berlin', 'de', _KML.ISO, 'Germany'),
    Region('fi', 'xkb:fi::fin', 'Europe/Helsinki', 'fi', _KML.ISO, 'Finland'),
    Region('fr', 'xkb:fr::fra', 'Europe/Paris', 'fr', _KML.ISO, 'France'),
    Region('gb', 'xkb:gb:extd:eng', 'Europe/London', 'en-GB', _KML.ISO, 'UK'),
    Region('ie', 'xkb:gb:extd:eng', 'Europe/Dublin', 'en-GB', _KML.ISO,
           'Ireland'),
    Region('in', 'xkb:us::eng', 'Asia/Calcutta', 'en-US', _KML.ANSI, 'India'),
    Region('my', 'xkb:us::eng', 'Asia/Kuala_Lumpur', 'ms', _KML.ANSI,
           'Malaysia'),
    Region('nl', 'xkb:us:intl:eng', 'Europe/Amsterdam', 'nl', _KML.ANSI,
           'Netherlands'),
    Region('nordic', 'xkb:se::swe', 'Europe/Stockholm', 'en-US', _KML.ISO,
           'Nordics',
           ('Unified SKU for Sweden, Norway, and Denmark.  This defaults '
            'to Swedish keyboard layout, but starts with US English language '
            'for neutrality.  Use if there is a single combined SKU for Nordic '
            'countries.')),
    Region('se', 'xkb:se::swe', 'Europe/Stockholm', 'sv', _KML.ISO, 'Sweden',
           ("Use this if there separate SKUs for Nordic countries (Sweden, "
            "Norway, and Denmark), or the device is only shipping to Sweden. "
            "If there is a single unified SKU, use 'nordic' instead.")),
    Region('sg', 'xkb:us::eng', 'Asia/Singapore', 'en-GB', _KML.ANSI,
           'Singapore'),
    Region('us', 'xkb:us::eng', 'America/Los_Angeles', 'en-US', _KML.ANSI,
           'United States'),
  ]
