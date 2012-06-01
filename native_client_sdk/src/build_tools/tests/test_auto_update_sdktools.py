#!/usr/bin/env python
# copyright (c) 2012 the chromium authors. all rights reserved.
# use of this source code is governed by a bsd-style license that can be
# found in the license file.

import os
import re
import subprocess
import sys
import tempfile
import test_server
import unittest

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
BUILD_TOOLS_DIR = os.path.dirname(SCRIPT_DIR)

sys.path.append(BUILD_TOOLS_DIR)
import buildbot_common
import build_utils
import build_updater
import getos
import manifest_util


MANIFEST_BASENAME = 'naclsdk_manifest2.json'


class TestAutoUpdateSdkTools(unittest.TestCase):
  def setUp(self):
    self.basedir = tempfile.mkdtemp()
    build_updater.BuildUpdater(self.basedir)
    self._LoadCacheManifest()
    self.current_revision = int(build_utils.ChromeRevision())
    self.server = test_server.LocalHTTPServer(self.basedir)

  def tearDown(self):
    if self.server:
      self.server.Shutdown()
    buildbot_common.RemoveDir(self.basedir)

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

  def _BuildUpdaterArchive(self, rel_path, revision):
    """Build a new sdk_tools bundle.

    Args:
      rel_path: The relative path to build the updater.
      revision: The revision number to give to this bundle.
    Returns:
      A manifest_util.Archive() that points to this new bundle on the local
      server.
    """
    build_updater.BuildUpdater(os.path.join(self.basedir, rel_path), revision)

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

  def testNoUpdate(self):
    """If the current revision is the newest, the shell script will run
    normally.
    """
    self._WriteManifest()
    revision = self._RunAndExtractRevision()
    self.assertEqual(revision, self.current_revision)

  def testUpdate(self):
    """Create a new bundle with a bumped revision number.
    When we run the shell script, we should see the new revision number.
    """
    new_revision = self.current_revision + 1
    archive = self._BuildUpdaterArchive('new', new_revision)
    self.sdk_tools_bundle.AddArchive(archive)
    self.sdk_tools_bundle.revision = new_revision
    self._WriteManifest()

    revision = self._RunAndExtractRevision()
    self.assertEqual(revision, new_revision)

  def testManualUpdateIsIgnored(self):
    """If the sdk_tools bundle was updated normally (i.e. the old way), it would
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
