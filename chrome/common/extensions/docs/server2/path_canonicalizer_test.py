#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from compiled_file_system import CompiledFileSystem
from extensions_paths import PUBLIC_TEMPLATES
from local_file_system import LocalFileSystem
from object_store_creator import ObjectStoreCreator
from path_canonicalizer import PathCanonicalizer


class PathCanonicalizerTest(unittest.TestCase):
  def setUp(self):
    self._path_canonicalizer = PathCanonicalizer(
        CompiledFileSystem.Factory(ObjectStoreCreator.ForTest()),
        LocalFileSystem.Create(PUBLIC_TEMPLATES))

  def testSpecifyCorrectly(self):
    self._AssertIdentity('extensions/browserAction.html')
    self._AssertIdentity('extensions/storage.html')
    self._AssertIdentity('extensions/blah.html')
    self._AssertIdentity('extensions/index.html')
    self._AssertIdentity('extensions/whats_new.html')
    self._AssertIdentity('apps/storage.html')
    self._AssertIdentity('apps/bluetooth.html')
    self._AssertIdentity('apps/blah.html')
    self._AssertIdentity('apps/tags/webview.html')

  def testSpecifyIncorrectly(self):
    self._AssertRedirect('extensions/browserAction.html',
                         'apps/browserAction.html')
    self._AssertRedirect('extensions/browserAction.html',
                         'apps/extensions/browserAction.html')
    self._AssertRedirect('apps/bluetooth.html', 'extensions/bluetooth.html')
    self._AssertRedirect('apps/bluetooth.html',
                         'extensions/apps/bluetooth.html')
    self._AssertRedirect('extensions/index.html', 'apps/index.html')
    self._AssertRedirect('extensions/browserAction.html',
                         'static/browserAction.html')
    self._AssertRedirect('apps/tags/webview.html',
                         'apps/webview.html')
    self._AssertRedirect('apps/tags/webview.html',
                         'extensions/webview.html')
    self._AssertRedirect('apps/tags/webview.html',
                         'extensions/tags/webview.html')
    # These are a little trickier because storage.html is in both directories.
    # They must canonicalize to the closest match.
    self._AssertRedirect('extensions/storage.html',
                         'extensions/apps/storage.html')
    self._AssertRedirect('apps/storage.html',
                         'apps/extensions/storage.html')

  def testUnspecified(self):
    self._AssertRedirect('extensions/browserAction.html', 'browserAction.html')
    self._AssertRedirect('apps/bluetooth.html', 'bluetooth.html')
    # Default happens to be apps because it's first alphabetically.
    self._AssertRedirect('apps/storage.html', 'storage.html')
    # Nonexistent APIs should be left alone.
    self._AssertIdentity('blah.html')

  def testDirectories(self):
    # Directories can be canonicalized too!
    self._AssertIdentity('apps/')
    self._AssertIdentity('apps/tags/')
    self._AssertIdentity('extensions/')
    # No trailing slash should be treated as files not directories, at least
    # at least according to PathCanonicalizer.
    self._AssertRedirect('extensions/apps.html', 'apps')
    self._AssertRedirect('extensions', 'extensions')
    # Just as tolerant of spelling mistakes.
    self._AssertRedirect('apps/', 'Apps/')
    self._AssertRedirect('apps/tags/', 'Apps/TAGS/')
    self._AssertRedirect('extensions/', 'Extensions/')
    # Find directories in the correct place.
    self._AssertRedirect('apps/tags/', 'tags/')
    self._AssertRedirect('apps/tags/', 'extensions/tags/')

  def testSpellingErrors(self):
    for spelme in ('browseraction', 'browseraction.htm', 'BrowserAction',
                   'BrowserAction.html', 'browseraction.html', 'Browseraction',
                   'browser-action', 'Browser.action.html', 'browser_action',
                   'browser-action.html', 'Browser_Action.html'):
      self._AssertRedirect('extensions/browserAction.html', spelme)
      self._AssertRedirect('extensions/browserAction.html',
                           'extensions/%s' % spelme)
      self._AssertRedirect('extensions/browserAction.html', 'apps/%s' % spelme)

  def _AssertIdentity(self, path):
    self._AssertRedirect(path, path)

  def _AssertRedirect(self, to, from_):
    self.assertEqual(to, self._path_canonicalizer.Canonicalize(from_))


if __name__ == '__main__':
  unittest.main()
