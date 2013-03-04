#!/usr/bin/python
#
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import cPickle
import signal
import os
import sys
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))

from chromite.lib import commandline
from chromite.lib import cros_test_lib
from chromite.lib import git
from chromite.lib import gclient
from chromite.lib import gs


# pylint: disable=W0212
class TestShutDownException(cros_test_lib.TestCase):
  """Test that ShutDownException can be pickled."""

  def testShutDownException(self):
    """Test that ShutDownException can be pickled."""
    ex = commandline._ShutDownException(signal.SIGTERM, 'Received SIGTERM')
    ex2 = cPickle.loads(cPickle.dumps(ex))
    self.assertEqual(ex.signal, ex2.signal)
    self.assertEqual(ex.message, ex2.message)


class GSPathTest(cros_test_lib.TestCase):
  """Test type=gs_path normalization functionality."""

  GS_REL_PATH = 'bucket/path/to/artifacts'

  @staticmethod
  def _ParseCommandLine(argv):
    parser = commandline.OptionParser()
    parser.add_option('-g', '--gs-path', type='gs_path',
                      help=('GS path that contains the chrome to deploy.'))
    return parser.parse_args(argv)

  def _RunGSPathTestCase(self, raw, parsed):
    options, _ =  self._ParseCommandLine(['--gs-path', raw])
    self.assertEquals(options.gs_path, parsed)

  def testNoGSPathCorrectionNeeded(self):
    """Test case where GS path correction is not needed."""
    gs_path = '%s/%s' % (gs.BASE_GS_URL, self.GS_REL_PATH)
    self._RunGSPathTestCase(gs_path, gs_path)

  def testTrailingSlashRemoval(self):
    """Test case where GS path correction is not needed."""
    gs_path = '%s/%s/' % (gs.BASE_GS_URL, self.GS_REL_PATH)
    self._RunGSPathTestCase(gs_path, gs_path.rstrip('/'))

  def testCorrectionNeeded(self):
    """Test case where GS path correction is needed."""
    self._RunGSPathTestCase(
        '%s/%s/' % (gs.PRIVATE_BASE_HTTPS_URL, self.GS_REL_PATH),
        '%s/%s' % (gs.BASE_GS_URL, self.GS_REL_PATH))

  def testInvalidPath(self):
    """Path cannot be normalized."""
    with cros_test_lib.OutputCapturer():
      self.assertRaises2(
          SystemExit, self._RunGSPathTestCase, 'http://badhost.com/path', '',
          check_attrs={'code': 2})


class CacheTest(cros_test_lib.MockTestCase):
  """Test cache dir specification and finding functionality."""

  REPO_ROOT = '/fake/repo/root'
  GCLIENT_ROOT = '/fake/gclient/root'
  SUBMODULE_ROOT = '/fake/submodule/root'
  CACHE_DIR = '/fake/cache/dir'

  def setUp(self):
    self.PatchObject(commandline.ArgumentParser, 'ConfigureCacheDir')
    self.PatchObject(git, 'FindRepoCheckoutRoot')
    self.PatchObject(git, 'FindGitSubmoduleCheckoutRoot')
    self.PatchObject(gclient, 'FindGclientCheckoutRoot')
    self.parser = commandline.ArgumentParser(caching=True)

  def _SetCheckoutRoots(self, repo_root=None, gclient_root=None,
                        submodule_root=None):
    git.FindRepoCheckoutRoot.return_value = repo_root
    gclient.FindGclientCheckoutRoot.return_value = gclient_root
    git.FindGitSubmoduleCheckoutRoot.return_value = submodule_root

  def _CheckCall(self, expected):
    f = self.parser.ConfigureCacheDir
    self.assertEquals(1, f.call_count)
    self.assertTrue(f.call_args[0][0].startswith(expected))

  def testRepoRoot(self):
    """Test when we are inside a repo checkout."""
    self._SetCheckoutRoots(repo_root=self.REPO_ROOT)
    self.parser.parse_args([])
    self._CheckCall(self.REPO_ROOT)

  def testGclientRoot(self):
    """Test when we are inside a gclient checkout."""
    self._SetCheckoutRoots(gclient_root=self.GCLIENT_ROOT)
    self.parser.parse_args([])
    self._CheckCall(self.GCLIENT_ROOT)

  def testSubmoduleRoot(self):
    """Test when we are inside a git submodule Chrome checkout."""
    self._SetCheckoutRoots(submodule_root=self.SUBMODULE_ROOT)
    self.parser.parse_args([])
    self._CheckCall(self.SUBMODULE_ROOT)

  def testTempdir(self):
    """Test when we are not in any checkout."""
    self._SetCheckoutRoots()
    self.parser.parse_args([])
    self._CheckCall('/tmp')

  def testSpecifiedDir(self):
    """Test when user specifies a cache dir."""
    self._SetCheckoutRoots(repo_root=self.REPO_ROOT)
    self.parser.parse_args(['--cache-dir', self.CACHE_DIR])
    self._CheckCall(self.CACHE_DIR)


if __name__ == '__main__':
  cros_test_lib.main()
