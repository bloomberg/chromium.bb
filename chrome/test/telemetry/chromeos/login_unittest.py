# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import json
import os
import unittest

from telemetry.core import browser_finder
from telemetry.core import extension_to_load
from telemetry.core.chrome import cros_interface
from telemetry.core.chrome import cros_util
from telemetry.test import options_for_unittests

class CrOSAutoTest(unittest.TestCase):
  def setUp(self):
    options = options_for_unittests.GetCopy()
    self._cri = cros_interface.CrOSInterface(options.cros_remote,
                                             options.cros_ssh_identity)
    self._is_guest = options.browser_type == 'cros-chrome-guest'
    self._email = '' if self._is_guest else 'test@test.test'

  def _IsCryptohomeMounted(self):
    """Returns True if cryptohome is mounted"""
    cryptohomeJSON, _ = self._cri.RunCmdOnDevice(['/usr/sbin/cryptohome',
                                                 '--action=status'])
    cryptohomeStatus = json.loads(cryptohomeJSON)
    return (cryptohomeStatus['mounts'] and
            cryptohomeStatus['mounts'][0]['mounted'])

  def _FilesystemMountedAt(self, path):
    """Returns the filesystem mounted at |path|"""
    df_out, _ = self._cri.RunCmdOnDevice(['/bin/df', path])
    df_ary = df_out.split('\n')
    if (len(df_ary) == 3):
      chronos_ary = df_ary[1].split()
      if chronos_ary:
        return chronos_ary[0]
    return None

  def testCryptohomeMounted(self):
    options = options_for_unittests.GetCopy()
    browser_to_create = browser_finder.FindBrowser(options)
    if not browser_to_create:
      raise Exception('No browser found, cannot continue test.')
    with browser_to_create.Create() as b:
      self.assertEquals(1, len(b.tabs))
      self.assertTrue(b.tabs[0].url)
      self.assertTrue(self._IsCryptohomeMounted())

      chronos_fs = self._FilesystemMountedAt('/home/chronos/user')
      self.assertTrue(chronos_fs)
      if self._is_guest:
        self.assertEquals(chronos_fs, 'guestfs')
      else:
        home, _ = self._cri.RunCmdOnDevice(['/usr/sbin/cryptohome-path',
                                            'user', self._email])
        self.assertEquals(self._FilesystemMountedAt(home.rstrip()),
                          chronos_fs)

    self.assertFalse(self._IsCryptohomeMounted())
    self.assertEquals(self._FilesystemMountedAt('/home/chronos/user'),
                      '/dev/mapper/encstateful')

  def testLoginStatus(self):
    extension_path = os.path.join(os.path.dirname(__file__),
        'autotest_ext')
    load_extension = extension_to_load.ExtensionToLoad(extension_path, True)

    options = options_for_unittests.GetCopy()
    options.extensions_to_load = [load_extension]
    browser_to_create = browser_finder.FindBrowser(options)
    self.assertTrue(browser_to_create)
    with browser_to_create.Create() as b:
      extension = b.extensions[load_extension]
      self.assertTrue(extension)
      extension.ExecuteJavaScript('''
        chrome.autotestPrivate.loginStatus(function(s) {
          window.__autotest_result = s;
        });
      ''')
      login_status = extension.EvaluateJavaScript('window.__autotest_result')
      self.assertEquals(type(login_status), dict)

      self.assertEquals(not self._is_guest, login_status['isRegularUser'])
      self.assertEquals(self._is_guest, login_status['isGuest'])
      self.assertEquals(login_status['email'], self._email)
      self.assertFalse(login_status['isScreenLocked'])


