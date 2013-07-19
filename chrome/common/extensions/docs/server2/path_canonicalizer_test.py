#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from path_canonicalizer import PathCanonicalizer
from server_instance import ServerInstance
import svn_constants

class PathCanonicalizerTest(unittest.TestCase):
  def setUp(self):
    self._server_instance = ServerInstance.ForLocal()

  def _Cze(self, path):
    return self._server_instance.path_canonicalizer.Canonicalize(path)

  def testSpecifyCorrectly(self):
    self._AssertIdentity('extensions/browserAction.html')
    self._AssertIdentity('extensions/storage.html')
    self._AssertIdentity('extensions/blah.html')
    self._AssertIdentity('extensions/index.html')
    self._AssertIdentity('extensions/whats_new.html')
    self._AssertIdentity('apps/storage.html')
    self._AssertIdentity('apps/bluetooth.html')
    self._AssertIdentity('apps/blah.html')
    self._AssertIdentity('static/browserAction.html')
    self._AssertIdentity('static/storage.html')
    self._AssertIdentity('static/bluetooth.html')
    self._AssertIdentity('static/blah.html')

  def testSpecifyIncorrectly(self):
    self._AssertTemporaryRedirect('extensions/browserAction.html',
                                  'apps/browserAction.html')
    self._AssertTemporaryRedirect('apps/bluetooth.html',
                                  'extensions/bluetooth.html')
    self._AssertTemporaryRedirect('extensions/index.html',
                                  'apps/index.html')

  def testUnspecified(self):
    self._AssertTemporaryRedirect('extensions/browserAction.html',
                                  'browserAction.html')
    self._AssertTemporaryRedirect('apps/bluetooth.html',
                                  'bluetooth.html')
    # Extensions are default for now.
    self._AssertTemporaryRedirect('extensions/storage.html',
                                  'storage.html')
    # Nonexistent APIs should be left alone.
    self._AssertIdentity('blah.html')

  def testSpellingErrors(self):
    for spelme in ('browseraction', 'browseraction.htm', 'BrowserAction',
                   'BrowserAction.html', 'browseraction.html', 'Browseraction',
                   'browser-action', 'Browser.action.html', 'browser_action',
                   'browser-action.html', 'Browser_Action.html'):
      self._AssertTemporaryRedirect('extensions/browserAction.html', spelme)
      self._AssertTemporaryRedirect('extensions/browserAction.html',
                                    'extensions/%s' % spelme)
      self._AssertTemporaryRedirect('extensions/browserAction.html',
                                    'apps/%s' % spelme)

  def testChannelRedirect(self):
    def assert_channel_redirect(channel, path):
      self._AssertPermanentRedirect(path, '%s/%s' % (channel, path))
    for channel in ('stable', 'beta', 'dev', 'trunk'):
      assert_channel_redirect(channel, 'extensions/browserAction.html')
      assert_channel_redirect(channel, 'extensions/storage.html')
      assert_channel_redirect(channel, 'apps/bluetooth.html')
      assert_channel_redirect(channel, 'apps/storage.html')

  def _AssertIdentity(self, path):
    self._AssertTemporaryRedirect(path, path)

  def _AssertTemporaryRedirect(self, to, from_):
    result = self._Cze(from_)
    self.assertEqual(to, result.path)
    self.assertFalse(result.permanent)

  def _AssertPermanentRedirect(self, to, from_):
    result = self._Cze(from_)
    self.assertEqual(to, result.path)
    self.assertTrue(result.permanent)

if __name__ == '__main__':
  unittest.main()
