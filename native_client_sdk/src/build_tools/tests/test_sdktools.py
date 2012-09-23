#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import subprocess
import sys
import tempfile
import test_server
import unittest

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
BUILD_TOOLS_DIR = os.path.dirname(SCRIPT_DIR)
TOOLS_DIR = os.path.join(os.path.dirname(BUILD_TOOLS_DIR), 'tools')

sys.path.extend([BUILD_TOOLS_DIR, TOOLS_DIR])
import build_utils
import getos
import manifest_util
import oshelpers


MANIFEST_BASENAME = 'naclsdk_manifest2.json'

# Attribute '' defined outside __init__
# pylint: disable=W0201

class SdkToolsTestCase(unittest.TestCase):
  def tearDown(self):
    if self.server:
      self.server.Shutdown()
    oshelpers.Remove(['-rf', self.basedir])

  def SetupDefault(self):
    self.SetupWithBaseDirPrefix('sdktools')

  def SetupWithBaseDirPrefix(self, basedir_prefix):
    self.basedir = tempfile.mkdtemp(prefix=basedir_prefix)
    # We have to make sure that we build our updaters with a version that is at
    # least as large as the version in the sdk_tools bundle. If not, update
    # tests may fail because the "current" version (according to the sdk_cache)
    # is greater than the version we are attempting to update to.
    self.current_revision = self._GetSdkToolsBundleRevision()
    self._BuildUpdater(self.basedir, self.current_revision)
    self._LoadCacheManifest()
    self.server = test_server.LocalHTTPServer(self.basedir)

  def _GetSdkToolsBundleRevision(self):
    """Get the sdk_tools bundle revision.
    We get this from the checked-in path; this is the same file that
    build_updater uses to specify the current revision of sdk_tools."""

    manifest_filename = os.path.join(BUILD_TOOLS_DIR, 'json',
                                     'naclsdk_manifest0.json')
    manifest = manifest_util.SDKManifest()
    manifest.LoadDataFromString(open(manifest_filename, 'r').read())
    return manifest.GetBundle('sdk_tools').revision

  def _LoadCacheManifest(self):
    """Read the manifest from nacl_sdk/sdk_cache.

    This manifest should only contain the sdk_tools bundle.
    """
    manifest_filename = os.path.join(self.basedir, 'nacl_sdk', 'sdk_cache',
        MANIFEST_BASENAME)
    self.manifest = manifest_util.SDKManifest()
    self.manifest.LoadDataFromString(open(manifest_filename, 'r').read())
    self.sdk_tools_bundle = self.manifest.GetBundle('sdk_tools')

  def _WriteManifest(self):
    with open(os.path.join(self.basedir, MANIFEST_BASENAME), 'w') as stream:
      stream.write(self.manifest.GetDataAsString())

  def _BuildUpdater(self, out_dir, revision=None):
    build_updater_py = os.path.join(BUILD_TOOLS_DIR, 'build_updater.py')
    cmd = [sys.executable, build_updater_py, '-o', out_dir]
    if revision:
      cmd.extend(['-r', str(revision)])

    process = subprocess.Popen(cmd, stdout=subprocess.PIPE)
    _, _ = process.communicate()
    self.assertEqual(process.returncode, 0)

  def _BuildUpdaterArchive(self, rel_path, revision):
    """Build a new sdk_tools bundle.

    Args:
      rel_path: The relative path to build the updater.
      revision: The revision number to give to this bundle.
    Returns:
      A manifest_util.Archive() that points to this new bundle on the local
      server.
    """
    self._BuildUpdater(os.path.join(self.basedir, rel_path), revision)

    new_sdk_tools_tgz = os.path.join(self.basedir, rel_path, 'sdk_tools.tgz')
    with open(new_sdk_tools_tgz, 'rb') as sdk_tools_stream:
      archive_sha1, archive_size = manifest_util.DownloadAndComputeHash(
          sdk_tools_stream)

    archive = manifest_util.Archive('all')
    archive.url = self.server.GetURL('%s/sdk_tools.tgz' % (rel_path,))
    archive.checksum = archive_sha1
    archive.size = archive_size
    return archive

  def _Run(self, args):
    naclsdk_shell_script = os.path.join(self.basedir, 'nacl_sdk', 'naclsdk')
    if getos.GetPlatform() == 'win':
      naclsdk_shell_script += '.bat'
    cmd = [naclsdk_shell_script, '-U', self.server.GetURL(MANIFEST_BASENAME)]
    cmd.extend(args)
    process = subprocess.Popen(cmd, stdout=subprocess.PIPE)
    stdout, _ = process.communicate()
    self.assertEqual(process.returncode, 0)
    return stdout
  
  def _RunAndExtractRevision(self):
    stdout = self._Run(['-v'])
    match = re.search('version r(\d+)', stdout)
    self.assertTrue(match is not None)
    return int(match.group(1))


class TestSdkTools(SdkToolsTestCase):
  def testPathHasSpaces(self):
    """Test that running naclsdk from a path with spaces works."""
    self.SetupWithBaseDirPrefix('sdk tools')
    self._WriteManifest()
    self._RunAndExtractRevision()


class TestAutoUpdateSdkTools(SdkToolsTestCase):
  def setUp(self):
    self.SetupDefault()

  def testNoUpdate(self):
    """Test that running naclsdk with current revision does nothing."""
    self._WriteManifest()
    revision = self._RunAndExtractRevision()
    self.assertEqual(revision, self.current_revision)

  def testUpdate(self):
    """Test that running naclsdk with a new revision will auto-update."""
    new_revision = self.current_revision + 1
    archive = self._BuildUpdaterArchive('new', new_revision)
    self.sdk_tools_bundle.AddArchive(archive)
    self.sdk_tools_bundle.revision = new_revision
    self._WriteManifest()

    revision = self._RunAndExtractRevision()
    self.assertEqual(revision, new_revision)

  def testManualUpdateIsIgnored(self):
    """Test that attempting to manually update sdk_tools is ignored.

    If the sdk_tools bundle was updated normally (i.e. the old way), it would
    leave a sdk_tools_update folder that would then be copied over on a
    subsequent run. This test ensures that there is no folder made.
    """
    new_revision = self.current_revision + 1
    archive = self._BuildUpdaterArchive('new', new_revision)
    self.sdk_tools_bundle.AddArchive(archive)
    self.sdk_tools_bundle.revision = new_revision
    self._WriteManifest()

    stdout = self._Run(['update', 'sdk_tools'])
    self.assertTrue(stdout.find('Ignoring manual update request.') != -1)
    sdk_tools_update_dir = os.path.join(self.basedir, 'nacl_sdk',
        'sdk_tools_update')
    self.assertFalse(os.path.exists(sdk_tools_update_dir))


def main():
  suite = unittest.defaultTestLoader.loadTestsFromModule(sys.modules[__name__])
  result = unittest.TextTestRunner(verbosity=2).run(suite)

  return int(not result.wasSuccessful())

if __name__ == '__main__':
  sys.exit(main())
